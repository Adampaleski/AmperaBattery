#pragma once

#include <Arduino.h>

// Per-build pack layout (set in platformio.ini).
#ifndef PACK_S_CELLS
#define PACK_S_CELLS 24
#endif

#ifndef PACK_BICM_COUNT
#define PACK_BICM_COUNT 1
#endif

#ifndef K112_PACK_DECODE
#define K112_PACK_DECODE 0
#endif

#if K112_PACK_DECODE

struct BicmCanStreamDef {
    uint32_t idLow;
    uint32_t idHigh;
    uint8_t  startCell;
    uint8_t  cellCount;
    const char *label;
};

#if PACK_BICM_COUNT == 1
// One stream: 24S default, or 48S probe (accumulate 0x460/0x470 past cell 24).
constexpr BicmCanStreamDef kBicmStreams[] = {
    {0x460, 0x470, 1, PACK_S_CELLS, "BICM-A"},
};
#elif PACK_BICM_COUNT == 2
// K112A 24S burst + K112B 12S (sparse 0x46D–0x47E or burst 0x461/0x471).
#if K112_SPARSE_SECOND_BICM
constexpr BicmCanStreamDef kBicmStreams[] = {
    {0x460, 0x470, 1, 24, "BICM-A"},
};
#else
constexpr BicmCanStreamDef kBicmStreams[] = {
    {0x460, 0x470, 1, 24, "BICM-A"},
    {0x461, 0x471, 25, 12, "BICM-B"},
};
#endif
#else
#error "PACK_BICM_COUNT must be 1 or 2 for K112_PACK_DECODE"
#endif

constexpr uint8_t kBicmStreamCount = sizeof(kBicmStreams) / sizeof(kBicmStreams[0]);

#endif  // K112_PACK_DECODE
