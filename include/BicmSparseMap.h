#pragma once

#include <Arduino.h>

#ifndef K112_SPARSE_SECOND_BICM
#define K112_SPARSE_SECOND_BICM 0
#endif

#if K112_SPARSE_SECOND_BICM

// Second 12S BICM on this bus: four single frames (see Volt_BMS.dbc 0x46D–0x47E).
// Maps to pack cells 25–36 (3 cells per frame × 4 frames).
struct SparseCellFrameDef {
    uint32_t id;
    uint8_t  startCell;
    uint8_t  cellCount;
};

constexpr SparseCellFrameDef kSparseSecondBicm[] = {
    {0x46D, 25, 3},
    {0x47D, 28, 3},
    {0x46E, 31, 3},
    {0x47E, 34, 3},
};

constexpr uint8_t kSparseSecondBicmCount =
    sizeof(kSparseSecondBicm) / sizeof(kSparseSecondBicm[0]);

#endif
