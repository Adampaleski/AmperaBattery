#pragma once
// Adjustable BMS thresholds over USB serial (Adam 2026-10-21: "all BMS
// thresholds should be adjustable"). Pure C++, header-only: host checks and
// Teensy share it. Never drives an output; it only edits BmsCore::Config.
//
//   T                 list every threshold: value, default, allowed range, status
//   T<key> <value>    set one (refused outside its bounds or if it breaks an order rule)
//   T<key> -          back to the code default
//   TX                all back to code defaults
//
// Bounds are SOFTWARE SANITY LIMITS, not part specs. Trip levels Adam decided
// (650 A / 100 ms, 750 A instant) and the temperature cuts can only be made
// TIGHTER here, never looser; loosening needs a code change + review.
// Overrides persist in EEPROM after the FaultLog (kBase 1216), CRC-16 checked;
// blank/corrupt storage = no overrides (code defaults).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BmsCore.h"
#include "FaultLog.h"   // LogStore

namespace BmsCore {

struct ThrDef {
    const char *key;
    const char *unit;
    float lo, hi;       // hard bounds (inclusive)
    bool  placeholder;  // default is a PLACEHOLDER, not a decided/sourced value
    bool  isMs;         // uint32_t field in Config
    float    Config::*f;
    uint32_t Config::*u;
};

#define THR_F(k, m, unit, lo, hi, ph) {k, unit, lo, hi, ph, false, &Config::m, nullptr}
#define THR_U(k, m, lo, hi, ph)       {k, "ms", lo, hi, ph, true, nullptr, &Config::m}
static const ThrDef kThr[] = {
    // current trip (Adam 2026-10-20): may only be lowered
    THR_F("ocA",      overcurrentA,  "A",  100.0f, 650.0f, false),
    THR_U("ocMs",     overcurrentMs,       10,    100,     false),
    THR_F("instA",    instantTripA,  "A",  100.0f, 750.0f, false),
    // temperatures: may only be lowered (Adam: the more conservative limit wins)
    THR_F("tripC",    overTempC,     "C",  30.0f,  55.0f,  true),
    THR_F("derateC",  derateTempC,   "C",  25.0f,  55.0f,  true),
    THR_F("chgMaxC",  chargeMaxTempC,"C",  20.0f,  45.0f,  true),
    THR_F("chgMinC",  chargeMinTempC,"C",  0.0f,   15.0f,  true),
    THR_F("chgColdC", chargeColdC,   "C",  0.0f,   25.0f,  true),
    // cells / sag (Adam 2026-10-21, relative sag)
    THR_U("lowMs",    lowCutMs,            100,   5000,    true),
    THR_F("restA",    restA,         "A",  5.0f,   60.0f,  true),
    THR_F("sagStart", relSagStartV,  "V",  0.05f,  0.50f,  true),
    THR_F("sagZero",  relSagZeroV,   "V",  0.10f,  0.80f,  true),
    THR_F("floorV",   hardFloorV,    "V",  2.80f,  3.30f,  true),
    THR_U("floorMs",  hardFloorMs,         50,    2000,    true),
    THR_F("packBand", packSagBandV,  "V",  0.02f,  0.30f,  true),
    // LEM A/B imbalance
    THR_F("imbA",     imbalanceA,    "A",  20.0f,  300.0f, true),
    THR_U("imbMs",    imbalanceMs,         100,   10000,   true),
    THR_F("imbPct",   imbalancePct,  "%",  0.0f,   100.0f, true),
    // A-B link (Adam 2026-10-21): lost B heartbeat -> F_LINK fault, mains open, logged
    THR_U("linkMs",  linkStaleMs,         100,   2000,    true),
    // derate event logging
    THR_U("drLogMs",  derateLogMs,         20,    5000,    true),
    THR_U("drClrMs",  derateClearMs,       100,   30000,   true),
};
#undef THR_F
#undef THR_U
constexpr int kThrN = static_cast<int>(sizeof(kThr) / sizeof(kThr[0]));

class Thresholds {
public:
    static constexpr int      kBase  = FaultLog::kEnd;   // 1216
    static constexpr uint32_t kMagic = 0x54485231;       // "THR1"
    static constexpr int      kSize  = 4 + 4 + 4 * kThrN + 2;
    static constexpr int      kEnd   = kBase + kSize;
    static_assert(kThrN <= 32, "override mask is 32 bits");
    static_assert(kEnd <= 4284, "past Teensy 4.1 emulated EEPROM");

    typedef void (*Sink)(const char *);

    explicit Thresholds(const LogStore &s) : st_(s) {}

    // Load overrides; false (and none applied) if blank/corrupt/out of bounds.
    bool load() {
        mask_ = 0;
        uint8_t b[kSize];
        for (int i = 0; i < kSize; i++) b[i] = st_.read(kBase + i);
        uint32_t magic, mask; memcpy(&magic, b, 4); memcpy(&mask, b + 4, 4);
        uint16_t crc; memcpy(&crc, b + kSize - 2, 2);
        if (magic != kMagic || crc != crc16(b, kSize - 2)) return false;
        float v[kThrN];
        for (int i = 0; i < kThrN; i++) {
            memcpy(&v[i], b + 8 + 4 * i, 4);
            if ((mask & (1u << i)) && !inBounds(i, v[i])) return false;
        }
        mask_ = mask & ((kThrN == 32) ? 0xFFFFFFFFu : ((1u << kThrN) - 1));
        memcpy(val_, v, sizeof(v));
        return true;
    }

    // Code default for every field, then the saved overrides on top.
    void apply(Config &c) const {
        for (int i = 0; i < kThrN; i++) if (mask_ & (1u << i)) put(c, i, val_[i]);
    }

    // Fields set elsewhere (e.g. low cutoff from the L setting) for the order rules.
    void setBase(const Config &b) { base_ = b; }

    int  overrides() const { return __builtin_popcount(mask_); }
    bool isSet(int i) const { return (mask_ >> i) & 1u; }

    static int find(const char *key, size_t n) {
        for (int i = 0; i < kThrN; i++)
            if (strlen(kThr[i].key) == n && strncmp(kThr[i].key, key, n) == 0) return i;
        return -1;
    }

    // One serial line starting with 'T'. Returns false only if it isn't ours.
    bool handleLine(const char *line, Sink out) {
        if (!line || line[0] != 'T') return false;
        const char *p = line + 1;
        while (*p == ' ') p++;
        if (!*p) { list(out); return true; }
        if (p[0] == 'X' && (p[1] == 0 || p[1] == ' ')) {
            mask_ = 0; save(); out("THRESHOLDS: all back to code defaults\n"); return true;
        }
        const char *k = p;
        while (*p && *p != ' ' && *p != '=' && !(*p == '-' && p > k) && !isNumStart(*p)) p++;
        const int i = find(k, static_cast<size_t>(p - k));
        char buf[120];
        if (i < 0) {
            snprintf(buf, sizeof buf, "THRESHOLDS: unknown key '%.*s' (T to list)\n", static_cast<int>(p - k), k);
            out(buf); return true;
        }
        while (*p == ' ' || *p == '=') p++;
        if (*p == '-' && (p[1] == 0 || p[1] == ' ')) {
            mask_ &= ~(1u << i); save();
            snprintf(buf, sizeof buf, "THRESHOLDS: %s back to default %g %s\n", kThr[i].key, def(i), kThr[i].unit);
            out(buf); return true;
        }
        char *end = nullptr;
        const float v = static_cast<float>(strtod(p, &end));
        if (!end || end == p) {
            snprintf(buf, sizeof buf, "THRESHOLDS: %s needs a number (e.g. T%s %g)\n", kThr[i].key, kThr[i].key, def(i));
            out(buf); return true;
        }
        if (!inBounds(i, v)) {
            snprintf(buf, sizeof buf, "THRESHOLDS REFUSED: %s=%g outside %g-%g %s (unchanged)\n",
                     kThr[i].key, v, kThr[i].lo, kThr[i].hi, kThr[i].unit);
            out(buf); return true;
        }
        // order rules checked on the would-be config
        Config c = base_; apply(c); put(c, i, v);
        const char *why = orderError(c);
        if (why) {
            snprintf(buf, sizeof buf, "THRESHOLDS REFUSED: %s=%g breaks rule: %s (unchanged)\n", kThr[i].key, v, why);
            out(buf); return true;
        }
        val_[i] = v; mask_ |= 1u << i; save();
        snprintf(buf, sizeof buf, "THRESHOLDS saved: %s=%g %s (default %g%s)\n", kThr[i].key, get(c, i), kThr[i].unit,
                 def(i), kThr[i].placeholder ? ", PLACEHOLDER" : "");
        out(buf); return true;
    }

    void list(Sink out) const {
        Config c = base_; apply(c);
        char buf[120];
        snprintf(buf, sizeof buf, "THRESHOLDS (%d overridden; T<key> <v> / T<key> - / TX):\n", overrides());
        out(buf);
        for (int i = 0; i < kThrN; i++) {
            snprintf(buf, sizeof buf, "  %-9s %8g %-2s  default %-7g range %g-%g%s%s\n", kThr[i].key, get(c, i),
                     kThr[i].unit, def(i), kThr[i].lo, kThr[i].hi, isSet(i) ? "  SET" : "",
                     kThr[i].placeholder ? "  PLACEHOLDER" : "");
            out(buf);
        }
    }

    // Cross-field rules; nullptr = ok.
    static const char *orderError(const Config &c) {
        if (c.overcurrentA >= c.instantTripA) return "ocA < instA";
        if (c.derateTempC >= c.overTempC)     return "derateC < tripC";
        if (c.chargeMinTempC >= c.chargeMaxTempC) return "chgMinC < chgMaxC";
        if (c.chargeColdC < c.chargeMinTempC || c.chargeColdC > c.chargeMaxTempC) return "chgMinC <= chgColdC <= chgMaxC";
        if (c.relSagStartV >= c.relSagZeroV)  return "sagStart < sagZero";
        if (c.hardFloorV >= c.lowCutV)        return "floorV < low cutoff (L)";
        return nullptr;
    }

private:
    static bool isNumStart(char ch) { return (ch >= '0' && ch <= '9') || ch == '.'; }
    static bool inBounds(int i, float v) {
        return v == v && v >= kThr[i].lo - 1e-4f && v <= kThr[i].hi + 1e-4f;
    }
    static float get(const Config &c, int i) {
        return kThr[i].isMs ? static_cast<float>(c.*(kThr[i].u)) : c.*(kThr[i].f);
    }
    static void put(Config &c, int i, float v) {
        if (kThr[i].isMs) c.*(kThr[i].u) = static_cast<uint32_t>(v + 0.5f);
        else              c.*(kThr[i].f) = v;
    }
    static float def(int i) { static const Config d; return get(d, i); }
    static uint16_t crc16(const uint8_t *p, int n) {
        uint16_t c = 0xFFFF;
        for (int i = 0; i < n; i++) {
            c ^= static_cast<uint16_t>(p[i]) << 8;
            for (int b = 0; b < 8; b++) c = (c & 0x8000) ? static_cast<uint16_t>((c << 1) ^ 0x1021) : static_cast<uint16_t>(c << 1);
        }
        return c;
    }
    void save() const {
        uint8_t b[kSize];
        memcpy(b, &kMagic, 4); memcpy(b + 4, &mask_, 4);
        memcpy(b + 8, val_, 4 * kThrN);
        const uint16_t crc = crc16(b, kSize - 2); memcpy(b + kSize - 2, &crc, 2);
        for (int i = 0; i < kSize; i++) st_.write(kBase + i, b[i]);
    }

    LogStore st_;
    Config   base_{};
    uint32_t mask_ = 0;
    float    val_[kThrN] = {};
};

}  // namespace BmsCore
