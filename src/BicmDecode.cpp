#include "BicmDecode.h"
#include "BicmPackProfile.h"
#include "BicmSparseMap.h"
#include "Config.h"
#include <math.h>

namespace BicmDecode {

namespace {

constexpr int kMaxModules = 16;

#if K112_PACK_DECODE

float g_packCells[PACK_S_CELLS + 1] = {};
bool  g_idSeen[0x480 - 0x460] = {};

struct StreamState {
    uint32_t idLow;
    uint32_t idHigh;
    uint8_t  startCell;
    uint8_t  endCell;
    uint8_t  nextCell;
    uint16_t framesLow;
    uint16_t framesHigh;
    const char *label;
};

StreamState g_streams[kBicmStreamCount];

bool     g_inBurst         = false;
uint32_t g_lastBurstMarkMs = 0;

float decodeCell(uint8_t msb, uint8_t lsb) {
    const uint16_t raw = ((msb & 0x0F) << 8) | lsb;
    if (raw == 0) {
        return 0.0f;
    }
    return raw * 0.00125f;
}

void initStreams() {
    for (uint8_t i = 0; i < kBicmStreamCount; i++) {
        const BicmCanStreamDef &def = kBicmStreams[i];
        g_streams[i].idLow      = def.idLow;
        g_streams[i].idHigh     = def.idHigh;
        g_streams[i].startCell  = def.startCell;
        g_streams[i].endCell    = static_cast<uint8_t>(def.startCell + def.cellCount - 1);
        g_streams[i].nextCell   = def.startCell;
        g_streams[i].framesLow  = 0;
        g_streams[i].framesHigh = 0;
        g_streams[i].label      = def.label;
    }
}

void clearPackCells() {
    for (int c = 0; c <= PACK_S_CELLS; c++) {
        g_packCells[c] = 0.0f;
    }
    for (uint8_t i = 0; i < kBicmStreamCount; i++) {
        g_streams[i].nextCell = g_streams[i].startCell;
    }
}

void resetStream(StreamState &s) {
    for (uint8_t c = s.startCell; c <= s.endCell; c++) {
        g_packCells[c] = 0.0f;
    }
    s.nextCell = s.startCell;
}

void startBurst() {
    clearPackCells();
    g_inBurst         = true;
    g_lastBurstMarkMs = millis();
}

void endBurst() {
    g_inBurst = false;
}

void appendCellsFromFrame(StreamState &s, const CAN_message_t &msg) {
    for (uint8_t i = 0; i + 1 < msg.len && s.nextCell <= s.endCell; i += 2) {
        const float v = decodeCell(msg.buf[i], msg.buf[i + 1]);
        if (v > 0.5f && v < 5.5f) {
            g_packCells[s.nextCell++] = v;
        }
    }
}

StreamState *streamForId(uint32_t id) {
    for (uint8_t i = 0; i < kBicmStreamCount; i++) {
        if (g_streams[i].idLow == id || g_streams[i].idHigh == id) {
            return &g_streams[i];
        }
    }
    return nullptr;
}

#if K112_SPARSE_SECOND_BICM
void markIdSeen(uint32_t id) {
    if (id >= 0x460 && id < 0x480) {
        g_idSeen[id - 0x460] = true;
    }
}

int countSecondBicmCells() {
    int n = 0;
    for (int c = 25; c <= PACK_S_CELLS && c <= 36; c++) {
        if (g_packCells[c] > 0.5f && g_packCells[c] < 5.5f) {
            n++;
        }
    }
    return n;
}

void decodeSparseSecondBicm(const CAN_message_t &msg) {
    for (const SparseCellFrameDef &e : kSparseSecondBicm) {
        if (msg.id != e.id) {
            continue;
        }
        markIdSeen(msg.id);
        for (uint8_t i = 0; i < e.cellCount; i++) {
            const uint8_t bi = static_cast<uint8_t>(i * 2);
            if (bi + 1 >= msg.len) {
                break;
            }
            const float v = decodeCell(msg.buf[bi], msg.buf[bi + 1]);
            if (v > 0.5f && v < 5.5f) {
                g_packCells[static_cast<uint8_t>(e.startCell + i)] = v;
            }
        }
        return;
    }
}
#endif

void decodeK112Frame(const CAN_message_t &msg) {
    if (msg.id == 0x4E0) {
        startBurst();
        return;
    }
    if (msg.id == 0x500) {
        endBurst();
        return;
    }

    StreamState *s = streamForId(msg.id);
    if (!s) {
        return;
    }

    g_idSeen[msg.id - 0x460] = true;

    if (!g_inBurst) {
        if (s->nextCell > s->endCell) {
            resetStream(*s);
        } else if (s->nextCell == s->startCell && g_packCells[s->startCell] <= 0.5f) {
            resetStream(*s);
        }
    }

    if (msg.id == s->idLow) {
        s->framesLow++;
    } else {
        s->framesHigh++;
    }
    appendCellsFromFrame(*s, msg);
}

#if K112_SPARSE_SECOND_BICM
void decodeK112Extended(const CAN_message_t &msg) {
    if (msg.id >= 0x460 && msg.id < 0x480) {
        decodeK112Frame(msg);
    }
    decodeSparseSecondBicm(msg);
}
#endif

int countPackCells() {
    int n = 0;
    for (int c = 1; c <= PACK_S_CELLS; c++) {
        if (g_packCells[c] > 0.5f && g_packCells[c] < 5.5f) {
            n++;
        }
    }
    return n;
}

int countStreamCells(const StreamState &s) {
    int n = 0;
    for (uint8_t c = s.startCell; c <= s.endCell; c++) {
        if (g_packCells[c] > 0.5f && g_packCells[c] < 5.5f) {
            n++;
        }
    }
    return n;
}

int countSeenIds() {
    int n = 0;
    for (unsigned i = 0; i < sizeof(g_idSeen); i++) {
        if (g_idSeen[i]) {
            n++;
        }
    }
    return n;
}

#else  // !K112_PACK_DECODE

constexpr int kCellsPerMod = BICM_CELLS_PER_MODULE;
constexpr int kCellsBuf    = 8;

struct ModuleState {
    bool     exists       = false;
    bool     sawLowFrame  = false;
    bool     sawHighFrame = false;
    float    cells[kCellsBuf] = {};
    float    temperature    = 0.0f;
    bool     hasTemp        = false;
    uint32_t lastUpdateMs   = 0;
};

ModuleState g_modules[kMaxModules + 1];

float decodeCell(uint8_t msb, uint8_t lsb) {
    const uint16_t raw = ((msb & 0x0F) << 8) | lsb;
    if (raw == 0) {
        return 0.0f;
    }
    return raw * 0.00125f;
}

float decodeTempDbc(const CAN_message_t &msg) {
    if (msg.len < 2) {
        return 0.0f;
    }
    const uint16_t raw10 = ((msg.buf[0] & 0x03) << 8) | msg.buf[1];
    return raw10 * 0.0556f - 27.778f;
}

float decodeTempLegacy(const CAN_message_t &msg) {
    if (msg.len < 8) {
        return 0.0f;
    }
    return (((msg.buf[6] << 8) + msg.buf[7]) * -0.0324f) + 150.0f;
}

int moduleIndexFromId(uint32_t id) {
    return static_cast<int>((id & 0x0F) + 1);
}

void decodeCellFrame(ModuleState &mod, const CAN_message_t &msg) {
    const uint8_t sub = static_cast<uint8_t>(msg.id & 0xF0);
    const uint8_t cellBytes = (msg.len > 6) ? 6 : msg.len;

    if (sub == 0x60) {
        mod.sawLowFrame = true;
        int cell = 1;
        for (uint8_t i = 0; i + 1 < cellBytes && cell <= 3; i += 2, cell++) {
            const float v = decodeCell(msg.buf[i], msg.buf[i + 1]);
            if (v > 0.0f) {
                mod.cells[cell] = v;
            }
        }
    } else if (sub == 0x70) {
        mod.sawHighFrame = true;
        int cell = 4;
        for (uint8_t i = 0; i + 1 < cellBytes && cell <= kCellsPerMod; i += 2, cell++) {
            const float v = decodeCell(msg.buf[i], msg.buf[i + 1]);
            if (v > 0.0f) {
                mod.cells[cell] = v;
            }
        }
    }
}

int countModuleCells(const ModuleState &mod) {
    int n = 0;
    for (int c = 1; c <= kCellsPerMod; c++) {
        if (mod.cells[c] > 0.5f && mod.cells[c] < 5.5f) {
            n++;
        }
    }
    return n;
}

#endif  // K112_PACK_DECODE

int g_stableTicks      = 0;
bool g_packStable      = false;
uint32_t g_printLastMs = 0;

void updateStability() {
    const int cells = seriesCellCount();
#if K112_PACK_DECODE
    const bool ok = (cells == PACK_S_CELLS);
#else
    const bool ok = (cells == PACK_S_CELLS && moduleCount() == PACK_MODULE_COUNT);
#endif
    if (ok) {
        if (g_stableTicks < 255) {
            g_stableTicks++;
        }
    } else {
        g_stableTicks = 0;
    }
    g_packStable = (g_stableTicks >= 3);
}

}  // namespace

void begin() {
#if !K112_PACK_DECODE
    for (int i = 0; i <= kMaxModules; i++) {
        g_modules[i] = ModuleState{};
    }
#else
    initStreams();
    clearPackCells();
    g_inBurst = false;
    for (unsigned i = 0; i < sizeof(g_idSeen); i++) {
        g_idSeen[i] = false;
    }
#endif
}

void onFrame(const CAN_message_t &msg) {
#if K112_PACK_DECODE
#if K112_SPARSE_SECOND_BICM
    if (msg.id >= 0x460 && msg.id <= 0x510) {
        decodeK112Extended(msg);
    }
#else
    if (msg.id >= 0x460 && msg.id < 0x480) {
        decodeK112Frame(msg);
    }
#endif
    return;
#else
    if (msg.id >= 0x460 && msg.id < 0x480) {
        const int idx = moduleIndexFromId(msg.id);
        if (idx < 1 || idx > kMaxModules) {
            return;
        }
        ModuleState &mod = g_modules[idx];
        mod.exists       = true;
        mod.lastUpdateMs = millis();
        decodeCellFrame(mod, msg);
        return;
    }

    if (msg.id >= 0x7E0 && msg.id < 0x7F0) {
        const int idx = moduleIndexFromId(msg.id);
        if (idx < 1 || idx > kMaxModules) {
            return;
        }
        ModuleState &mod = g_modules[idx];
        mod.exists       = true;
        mod.lastUpdateMs = millis();
        mod.temperature  = decodeTempDbc(msg);
        mod.hasTemp      = isfinite(mod.temperature);
        (void)decodeTempLegacy(msg);
    }
#endif
}

void tick() {
    updateStability();
    if (millis() - g_printLastMs >= 3000 && g_packStable) {
        g_printLastMs = millis();
        printDecodeDetails();
    }
}

int seriesCellCount() {
#if K112_PACK_DECODE
    return countPackCells();
#else
    int total = 0;
    for (int m = 1; m <= kMaxModules; m++) {
        if (!g_modules[m].exists) {
            continue;
        }
        if (!g_modules[m].sawLowFrame || !g_modules[m].sawHighFrame) {
            continue;
        }
        total += countModuleCells(g_modules[m]);
    }
    return total;
#endif
}

int moduleCount() {
#if K112_PACK_DECODE
    int n = 0;
    for (uint8_t i = 0; i < kBicmStreamCount; i++) {
        const int expected = static_cast<int>(g_streams[i].endCell - g_streams[i].startCell + 1);
        const int have     = countStreamCells(g_streams[i]);
#if K112_SPARSE_SECOND_BICM
        if (have >= expected - 1) {
            n++;
        }
#else
        if (have >= expected) {
            n++;
        }
#endif
    }
#if K112_SPARSE_SECOND_BICM
    if (countSecondBicmCells() >= 9) {
        n++;
    }
#endif
    return n;
#else
    int n = 0;
    for (int m = 1; m <= kMaxModules; m++) {
        if (g_modules[m].exists && g_modules[m].sawLowFrame && g_modules[m].sawHighFrame) {
            if (countModuleCells(g_modules[m]) > 0) {
                n++;
            }
        }
    }
    return n;
#endif
}

bool packStable() { return g_packStable; }

float cellVoltage(uint8_t cell) {
    if (cell < 1 || cell > PACK_S_CELLS) {
        return 0.0f;
    }
#if K112_PACK_DECODE
    return g_packCells[cell];
#else
    const int per = kCellsPerMod;
    const int idx = static_cast<int>((cell - 1) / per) + 1;
    const int sub = static_cast<int>((cell - 1) % per) + 1;
    if (idx < 1 || idx > kMaxModules || !g_modules[idx].exists) {
        return 0.0f;
    }
    return g_modules[idx].cells[sub];
#endif
}


#if K112_PACK_DECODE
static void printCellRange(uint8_t from, uint8_t to) {
    for (uint8_t c = from; c <= to; c++) {
        SERIALCONSOLE.print(g_packCells[c], 3);
        SERIALCONSOLE.print(' ');
    }
    SERIALCONSOLE.println();
}

static float sumCellRange(uint8_t from, uint8_t to) {
    float sum = 0.0f;
    for (uint8_t c = from; c <= to; c++) {
        if (g_packCells[c] > 0.5f && g_packCells[c] < 5.5f) {
            sum += g_packCells[c];
        }
    }
    return sum;
}

static void printBlockSummary(uint8_t from, uint8_t to, const __FlashStringHelper *label) {
    float minV = 5.5f;
    float maxV = 0.0f;
    for (uint8_t c = from; c <= to; c++) {
        const float v = g_packCells[c];
        if (v > 0.5f && v < 5.5f) {
            if (v < minV) {
                minV = v;
            }
            if (v > maxV) {
                maxV = v;
            }
        }
    }
    SERIALCONSOLE.print(label);
    SERIALCONSOLE.print(F(" sum="));
    SERIALCONSOLE.print(sumCellRange(from, to), 2);
    SERIALCONSOLE.print(F("V  min="));
    SERIALCONSOLE.print(minV, 3);
    SERIALCONSOLE.print(F("  max="));
    SERIALCONSOLE.println(maxV, 3);
}
#endif

void printDecodeDetails() {
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("DECODE "));
#if K112_PACK_DECODE
    SERIALCONSOLE.print(F("K112 "));
    SERIALCONSOLE.print(PACK_S_CELLS);
    SERIALCONSOLE.print(F("S  bicms="));
    SERIALCONSOLE.print(moduleCount());
    SERIALCONSOLE.print(F("/"));
#if K112_SPARSE_SECOND_BICM
    SERIALCONSOLE.print(kBicmStreamCount + 1);
#else
    SERIALCONSOLE.print(kBicmStreamCount);
#endif
    SERIALCONSOLE.print(F("  can_ids="));
    SERIALCONSOLE.print(countSeenIds());
#else
    SERIALCONSOLE.print(F("modules="));
    SERIALCONSOLE.print(moduleCount());
#endif
    SERIALCONSOLE.print(F("  cells="));
    SERIALCONSOLE.print(seriesCellCount());
    SERIALCONSOLE.print(F("  expected="));
    SERIALCONSOLE.print(PACK_S_CELLS);
    SERIALCONSOLE.print(F("  stable="));
    SERIALCONSOLE.println(g_packStable ? F("Y") : F("N"));

#if K112_PACK_DECODE
    for (uint8_t i = 0; i < kBicmStreamCount; i++) {
        const StreamState &s = g_streams[i];
        SERIALCONSOLE.print(F("  "));
        SERIALCONSOLE.print(s.label);
        SERIALCONSOLE.print(F(" 0x"));
        SERIALCONSOLE.print(s.idLow, HEX);
        SERIALCONSOLE.print(F("/0x"));
        SERIALCONSOLE.print(s.idHigh, HEX);
        SERIALCONSOLE.print(F(" 460#="));
        SERIALCONSOLE.print(s.framesLow);
        SERIALCONSOLE.print(F(" 470#="));
        SERIALCONSOLE.print(s.framesHigh);
        SERIALCONSOLE.print(F("  cells="));
        SERIALCONSOLE.print(countStreamCells(s));
        SERIALCONSOLE.print(F("/"));
        SERIALCONSOLE.println(static_cast<int>(s.endCell - s.startCell + 1));
    }
#if K112_SPARSE_SECOND_BICM
    SERIALCONSOLE.print(F("  BICM-B sparse 46D/47D/46E/47E  cells="));
    SERIALCONSOLE.print(countSecondBicmCells());
    SERIALCONSOLE.println(F("/12"));
#endif
    if (PACK_S_CELLS <= 24) {
        SERIALCONSOLE.println(F("  cells 1-6:  "));
        printCellRange(1, 6);
        SERIALCONSOLE.println(F("  cells 7-18: "));
        printCellRange(7, 18);
        SERIALCONSOLE.println(F("  cells 19-24:"));
        printCellRange(19, 24);
    } else {
        SERIALCONSOLE.println(F("  cells 1-12: "));
        printCellRange(1, 12);
        SERIALCONSOLE.println(F("  cells 13-24:"));
        printCellRange(13, 24);
        SERIALCONSOLE.println(F("  cells 25-36:"));
        printCellRange(25, 36);
        printBlockSummary(1, 24, F("  block A (cells 1-24):"));
        printBlockSummary(25, 36, F("  block B (cells 25-36):"));
        SERIALCONSOLE.print(F("  pack total: "));
        SERIALCONSOLE.print(sumCellRange(1, static_cast<uint8_t>(PACK_S_CELLS)), 2);
        SERIALCONSOLE.println(F("V"));
    }
#else
    for (int m = 1; m <= PACK_MODULE_COUNT; m++) {
        const ModuleState &mod = g_modules[m];
        if (!mod.exists) {
            continue;
        }
        SERIALCONSOLE.print(F("  M"));
        SERIALCONSOLE.print(m);
        SERIALCONSOLE.print(F(": "));
        for (int c = 1; c <= kCellsPerMod; c++) {
            SERIALCONSOLE.print(mod.cells[c], 3);
            if (c < kCellsPerMod) {
                SERIALCONSOLE.print(' ');
            }
        }
        if (mod.hasTemp) {
            SERIALCONSOLE.print(F("  T="));
            SERIALCONSOLE.print(mod.temperature, 1);
        }
        SERIALCONSOLE.println();
    }
#endif
}

void handleSerial(char c) {
    if (c == 'd') {
        printDecodeDetails();
    }
}

}  // namespace BicmDecode
