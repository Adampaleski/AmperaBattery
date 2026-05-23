#include "BicmDecode.h"
#include "Config.h"
#include <math.h>

namespace BicmDecode {

namespace {

constexpr int kMaxModules = 16;

#if K112_24S_SUBPACK

// Pack-relative cell voltages (1..24) from one K112 on CAN.
float g_packCells[25] = {};
bool  g_idSeen[0x480 - 0x460] = {};

struct K112FrameMap {
    uint32_t id;
    uint8_t  startCell;
    uint8_t  cellCount;
};

// Volt-style ID map: 3 cells per 6-byte frame, 4 cells per 8-byte frame.
constexpr K112FrameMap kK112Frames[] = {
    {0x460, 1, 3},
    {0x470, 4, 3},
    {0x461, 7, 4},
    {0x471, 11, 4},
    {0x462, 15, 4},
    {0x472, 19, 4},
    {0x463, 23, 2},
};

float decodeCell(uint8_t msb, uint8_t lsb) {
    const uint16_t raw = ((msb & 0x0F) << 8) | lsb;
    if (raw == 0) {
        return 0.0f;
    }
    return raw * 0.00125f;
}

void decodeK112Frame(const CAN_message_t &msg) {
    for (const K112FrameMap &e : kK112Frames) {
        if (msg.id != e.id) {
            continue;
        }
        if (msg.id >= 0x460 && msg.id < 0x480) {
            g_idSeen[msg.id - 0x460] = true;
        }
        for (uint8_t i = 0; i < e.cellCount; i++) {
            const uint8_t bi = static_cast<uint8_t>(i * 2);
            if (bi + 1 >= msg.len) {
                break;
            }
            const float v = decodeCell(msg.buf[bi], msg.buf[bi + 1]);
            if (v > 0.0f) {
                g_packCells[e.startCell + i] = v;
            }
        }
        return;
    }
}

int countPackCells() {
    int n = 0;
    for (int c = 1; c <= PACK_S_CELLS; c++) {
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

#else  // !K112_24S_SUBPACK

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

#endif  // K112_24S_SUBPACK

int g_stableTicks      = 0;
bool g_packStable      = false;
uint32_t g_printLastMs = 0;

void updateStability() {
    const int cells = seriesCellCount();
#if K112_24S_SUBPACK
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
#if !K112_24S_SUBPACK
    for (int i = 0; i <= kMaxModules; i++) {
        g_modules[i] = ModuleState{};
    }
#else
    for (int c = 0; c <= PACK_S_CELLS; c++) {
        g_packCells[c] = 0.0f;
    }
    for (unsigned i = 0; i < sizeof(g_idSeen); i++) {
        g_idSeen[i] = false;
    }
#endif
}

void onFrame(const CAN_message_t &msg) {
#if K112_24S_SUBPACK
    if (msg.id >= 0x460 && msg.id < 0x480) {
        decodeK112Frame(msg);
    }
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
#if K112_24S_SUBPACK
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
#if K112_24S_SUBPACK
    return countSeenIds();
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

void printDecodeDetails() {
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("DECODE "));
#if K112_24S_SUBPACK
    SERIALCONSOLE.print(F("K112 24S sub-pack  can_ids="));
    SERIALCONSOLE.print(moduleCount());
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

#if K112_24S_SUBPACK
    SERIALCONSOLE.println(F("  (one physical K112 — cells 1-24 on CAN)"));
    SERIALCONSOLE.print(F("  cells 1-6:  "));
    for (int c = 1; c <= 6; c++) {
        SERIALCONSOLE.print(g_packCells[c], 3);
        SERIALCONSOLE.print(' ');
    }
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("  cells 7-18: "));
    for (int c = 7; c <= 18; c++) {
        SERIALCONSOLE.print(g_packCells[c], 3);
        SERIALCONSOLE.print(' ');
    }
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("  cells 19-24:"));
    for (int c = 19; c <= 24; c++) {
        SERIALCONSOLE.print(g_packCells[c], 3);
        SERIALCONSOLE.print(' ');
    }
    SERIALCONSOLE.println();
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
