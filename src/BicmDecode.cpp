#include "BicmDecode.h"
#include "Config.h"
#include <math.h>

namespace BicmDecode {

namespace {

constexpr int kMaxModules = 16;
constexpr int kCellsPerMod = BICM_CELLS_PER_MODULE;
constexpr int kCellsBuf    = 8;

struct ModuleState {
    bool     exists       = false;
    bool     sawLowFrame  = false;  // id nibble -> 0xX60
    bool     sawHighFrame = false;  // id nibble -> 0xX70
    float    cells[kCellsBuf] = {};
    float    temperature    = 0.0f;
    bool     hasTemp        = false;
    uint32_t lastUpdateMs   = 0;
};

ModuleState g_modules[kMaxModules + 1];

int g_stableTicks     = 0;
bool g_packStable     = false;
uint32_t g_printLastMs = 0;

float decodeCell(uint8_t msb, uint8_t lsb) {
    const uint16_t raw = ((msb & 0x0F) << 8) | lsb;
    if (raw == 0) {
        return 0.0f;
    }
    return raw * 0.00125f;
}

// DBC Temp: factor 0.0556, offset -27.778 on 10-bit field at bit 1.
float decodeTempDbc(const CAN_message_t &msg) {
    if (msg.len < 2) {
        return 0.0f;
    }
    const uint16_t raw10 = ((msg.buf[0] & 0x03) << 8) | msg.buf[1];
    return raw10 * 0.0556f - 27.778f;
}

// Legacy Volt frame layout (bytes 6–7) — kept for cross-check on bench.
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

void updateStability() {
    const int cells = seriesCellCount();
    const int mods  = moduleCount();

    if (cells == PACK_S_CELLS && mods == PACK_MODULE_COUNT) {
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
    for (int i = 0; i <= kMaxModules; i++) {
        g_modules[i] = ModuleState{};
    }
}

void onFrame(const CAN_message_t &msg) {
    if (msg.id >= 0x460 && msg.id < 0x480) {
        const int idx = moduleIndexFromId(msg.id);
        if (idx < 1 || idx > kMaxModules) {
            return;
        }
        ModuleState &mod = g_modules[idx];
        mod.exists       = true;
        mod.lastUpdateMs = millis();
        decodeCellFrame(mod, msg);
        if (mod.sawLowFrame && mod.sawHighFrame) {
            mod.exists = true;
        }
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
}

void tick() {
    updateStability();

    if (millis() - g_printLastMs >= 3000 && g_packStable) {
        g_printLastMs = millis();
        printDecodeDetails();
    }
}

int seriesCellCount() {
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
}

int moduleCount() {
    int n = 0;
    for (int m = 1; m <= kMaxModules; m++) {
        if (g_modules[m].exists && g_modules[m].sawLowFrame && g_modules[m].sawHighFrame) {
            if (countModuleCells(g_modules[m]) > 0) {
                n++;
            }
        }
    }
    return n;
}

bool packStable() { return g_packStable; }

void printDecodeDetails() {
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("DECODE modules="));
    SERIALCONSOLE.print(moduleCount());
    SERIALCONSOLE.print(F(" cells="));
    SERIALCONSOLE.print(seriesCellCount());
    SERIALCONSOLE.print(F(" expected="));
    SERIALCONSOLE.print(PACK_S_CELLS);
    SERIALCONSOLE.print(F(" stable="));
    SERIALCONSOLE.println(g_packStable ? F("Y") : F("N"));

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
}

void handleSerial(char c) {
    if (c == 'd') {
        printDecodeDetails();
    }
}

}  // namespace BicmDecode
