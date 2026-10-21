#pragma once
// Fault log that survives power-off (pure C++, header-only; host checks and
// Teensy share it). NOT wired into the firmware yet: nothing calls it from
// BmsApp, and it never drives an output. Backlog item 5, step 1.
//
// Storage: a byte read/write pair (Teensy: EEPROM.read / EEPROM.update, the
// emulated EEPROM in flash, E2END 0x10BB = 4284 bytes per the installed
// framework-arduinoteensy core). Settings::Blob lives at address 0 (< 32 B),
// so the log starts at kBase = 256 and ends well under 4284.
//
// Layout: kSlots fixed 20-byte records in a ring. No header block: each record
// carries a sequence number and a CRC, so on boot the newest valid record is
// found by scanning. A record torn by power loss mid-write fails its CRC and
// is skipped (the rest survive). Blank flash (0xFF) reads as empty.
//
// Flash wear: writes happen only on fault edges and are rate-limited:
//   - the same code+kind at most once per minGapMs (default 1 s),
//   - at most maxPerBoot records per power-on; the last one is a
//     "log limit reached" note and nothing more is written until reboot.
// Teensy's emulated EEPROM wear-levels across its flash sectors; the real
// endurance of that flash is not verified here, so keep the limits.
#include <stdint.h>
#include <string.h>

namespace BmsCore {

enum class LogKind : uint8_t { FaultSet = 1, FaultClear = 2, Note = 3, DerateSet = 4, DerateClear = 5 };  // Derate*: code = BmsCore::DerateCode, value = power %
enum : uint8_t { NOTE_LIMIT = 1, NOTE_BOOT = 2 };  // Note codes

struct LogRec {
    uint32_t seq = 0;       // global, increases forever (newest = highest)
    uint16_t boot = 0;      // power-on number (1 = first boot with a log)
    LogKind  kind = LogKind::Note;
    uint8_t  code = 0;      // fault bit index (0..31) for FaultSet/Clear, NOTE_* for Note, DerateCode for Derate*
    uint32_t ms = 0;        // uptime ms in that boot
    int16_t  value = 0;     // optional context (e.g. lowest cell mV, amps); 0 = none
};

struct LogStore {           // byte access to non-volatile memory
    uint8_t (*read)(int addr);
    void    (*write)(int addr, uint8_t v);   // should skip unchanged bytes (EEPROM.update)
};

class FaultLog {
public:
    static constexpr int kBase    = 256;
    static constexpr int kRecSize = 20;
    static constexpr int kSlots   = 48;
    static constexpr int kEnd     = kBase + kRecSize * kSlots;   // 1216
    static_assert(kEnd <= 4284, "fault log must fit Teensy 4.1 emulated EEPROM");

    uint32_t minGapMs   = 1000;
    int      maxPerBoot = 24;
    int      maxDeratePerBoot = 12;   // derates stop here so they can't use up the slots real faults need

    // Scan storage, find the newest record, start a new boot number.
    void begin(const LogStore &s) {
        st_ = s; count_ = 0; head_ = 0; seq_ = 0; written_ = 0; derates_ = 0; dropped_ = 0; limited_ = false;
        uint16_t maxBoot = 0;
        for (int i = 0; i < kSlots; i++) {
            LogRec r;
            if (!load(i, r)) continue;
            count_++;
            if (count_ == 1 || r.seq > seq_) { seq_ = r.seq; head_ = (i + 1) % kSlots; }
            if (r.boot > maxBoot) maxBoot = r.boot;
        }
        boot_ = static_cast<uint16_t>(maxBoot + 1);
        memset(lastMs_, 0, sizeof lastMs_);
        memset(seen_, 0, sizeof seen_);
    }

    // Append one record. Returns false if rate-limited or past the boot limit.
    bool add(LogKind kind, uint8_t code, uint32_t ms, int16_t value = 0) {
        if (limited_) return false;
        const int k = key(kind, code);
        if (k >= 0 && seen_[k] && ms - lastMs_[k] < minGapMs) return false;
        const bool der = kind == LogKind::DerateSet || kind == LogKind::DerateClear;
        if (der && derates_ >= maxDeratePerBoot) { dropped_++; return false; }
        if (written_ >= maxPerBoot - 1) {      // last slot this boot: note + stop
            put(LogKind::Note, NOTE_LIMIT, ms, static_cast<int16_t>(maxPerBoot));
            limited_ = true;
            return false;
        }
        if (k >= 0) { seen_[k] = true; lastMs_[k] = ms; }
        if (der) derates_++;
        put(kind, code, ms, value);
        return true;
    }

    // Log fault-bit edges between two fault words (call each tick).
    void edges(uint32_t prev, uint32_t now, uint32_t ms, int16_t value = 0) {
        const uint32_t up = now & ~prev, down = prev & ~now;
        for (uint8_t b = 0; b < 32; b++) {
            if (up & (1u << b))   add(LogKind::FaultSet, b, ms, value);
            if (down & (1u << b)) add(LogKind::FaultClear, b, ms, value);
        }
    }

    // i = 0 newest .. count()-1 oldest. Skips slots that fail CRC.
    bool get(int i, LogRec &r) const {
        int seen = 0;
        for (int n = 1; n <= kSlots; n++) {
            const int slot = (head_ - n + kSlots) % kSlots;
            if (!load(slot, r)) continue;
            if (seen++ == i) return true;
        }
        return false;
    }

    // Erase (0xFF) every slot. Only on an explicit user command.
    void clear() {
        for (int a = kBase; a < kEnd; a++) st_.write(a, 0xFF);
        count_ = 0; head_ = 0; seq_ = 0;
    }

    int      count() const { return count_; }
    uint16_t boot() const { return boot_; }
    bool     limited() const { return limited_; }
    int      writtenThisBoot() const { return written_; }
    int      deratesThisBoot() const { return derates_; }
    int      deratesDropped() const { return dropped_; }   // past maxDeratePerBoot (RAM only)

    static uint16_t crc16(const uint8_t *p, int n) {   // CRC-16/CCITT-FALSE
        uint16_t c = 0xFFFF;
        for (int i = 0; i < n; i++) {
            c ^= static_cast<uint16_t>(p[i]) << 8;
            for (int b = 0; b < 8; b++) c = (c & 0x8000) ? static_cast<uint16_t>((c << 1) ^ 0x1021) : static_cast<uint16_t>(c << 1);
        }
        return c;
    }

private:
    static constexpr uint8_t kMagic = 0xF7;
    LogStore st_{nullptr, nullptr};
    int      head_ = 0, count_ = 0, written_ = 0, derates_ = 0, dropped_ = 0;
    uint32_t seq_ = 0;
    uint16_t boot_ = 1;
    bool     limited_ = false;
    uint32_t lastMs_[96];
    bool     seen_[96];

    static int key(LogKind kind, uint8_t code) {
        if (code >= 32) return -1;
        if (kind == LogKind::FaultSet) return code;
        if (kind == LogKind::FaultClear) return 32 + code;
        if (code >= 16) return -1;
        if (kind == LogKind::DerateSet) return 64 + code;
        if (kind == LogKind::DerateClear) return 80 + code;
        return -1;
    }

    // byte 0 magic, 1 kind, 2 code, 3 reserved, 4-7 seq, 8-9 boot, 10-11 value,
    // 12-15 ms, 16-17 crc over 0..15, 18-19 reserved (0xFF). Little-endian.
    void put(LogKind kind, uint8_t code, uint32_t ms, int16_t value) {
        uint8_t b[kRecSize];
        memset(b, 0xFF, sizeof b);
        const uint32_t seq = seq_ + 1;
        const uint16_t v = static_cast<uint16_t>(value);
        b[0] = kMagic; b[1] = static_cast<uint8_t>(kind); b[2] = code; b[3] = 0;
        for (int i = 0; i < 4; i++) b[4 + i] = static_cast<uint8_t>(seq >> (8 * i));
        b[8] = static_cast<uint8_t>(boot_); b[9] = static_cast<uint8_t>(boot_ >> 8);
        b[10] = static_cast<uint8_t>(v); b[11] = static_cast<uint8_t>(v >> 8);
        for (int i = 0; i < 4; i++) b[12 + i] = static_cast<uint8_t>(ms >> (8 * i));
        const uint16_t c = crc16(b, 16);
        b[16] = static_cast<uint8_t>(c); b[17] = static_cast<uint8_t>(c >> 8);
        const int a = kBase + head_ * kRecSize;
        for (int i = 0; i < kRecSize; i++) st_.write(a + i, b[i]);
        seq_ = seq;
        head_ = (head_ + 1) % kSlots;
        if (count_ < kSlots) count_++;
        written_++;
    }

    bool load(int slot, LogRec &r) const {
        uint8_t b[kRecSize];
        const int a = kBase + slot * kRecSize;
        for (int i = 0; i < kRecSize; i++) b[i] = st_.read(a + i);
        if (b[0] != kMagic) return false;
        if ((b[16] | (b[17] << 8)) != crc16(b, 16)) return false;
        if (b[1] < 1 || b[1] > 5) return false;
        r.kind = static_cast<LogKind>(b[1]);
        r.code = b[2];
        r.seq = 0; r.ms = 0;
        for (int i = 0; i < 4; i++) { r.seq |= static_cast<uint32_t>(b[4 + i]) << (8 * i); r.ms |= static_cast<uint32_t>(b[12 + i]) << (8 * i); }
        r.boot = static_cast<uint16_t>(b[8] | (b[9] << 8));
        r.value = static_cast<int16_t>(static_cast<uint16_t>(b[10] | (b[11] << 8)));
        return true;
    }
};

}  // namespace BmsCore
