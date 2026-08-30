// Host compile-check for K112 36S 0x300/0x310 packing. No Teensy, no CAN.
#include "K112BalanceMap.h"

#include <stdio.h>
#include <stdint.h>

static int g_fail = 0;

static void expect_eq(const char *name, unsigned got, unsigned want) {
    if (got != want) {
        printf("FAIL %s: got 0x%02X want 0x%02X\n", name, got, want);
        g_fail++;
    } else {
        printf("ok   %s = 0x%02X\n", name, got);
    }
}

int main() {
    uint8_t b300[8];
    uint8_t b310[5];
    uint8_t want[37] = {};

    // Cell 1 only: empirical 0x300#01.00.00.00.00.00.00.00 (most-positive of 0x460 group)
    want[1] = 1;
    k112PackWanted(want, 36, b300, b310);
    expect_eq("cell1 0x300[0]", b300[0], 0x01);
    expect_eq("cell1 rest 300", b300[1] | b300[2] | b300[3] | b300[4], 0);
    expect_eq("cell1 310 empty", b310[0] | b310[1] | b310[2] | b310[3] | b310[4], 0);

    // 0x460 group (cells 1-3) + 0x470 group (cells 4-6) share byte 0
    for (int c = 1; c <= 36; c++) {
        want[c] = (c >= 1 && c <= 6) ? 1 : 0;
    }
    k112PackWanted(want, 36, b300, b310);
    expect_eq("cells1-6 0x300[0]", b300[0], 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20);

    // 24S burst must NOT collapse into byte 0: cell 7 is byte 1
    for (int c = 1; c <= 36; c++) {
        want[c] = (c == 7) ? 1 : 0;
    }
    k112PackWanted(want, 36, b300, b310);
    expect_eq("cell7 not in [0]", b300[0], 0);
    expect_eq("cell7 0x300[1]", b300[1], 0x01);

    // All 24 BICM-A cells → 0x300[0..3] = 0x3F each, 0x310 still empty
    for (int c = 1; c <= 36; c++) {
        want[c] = (c <= 24) ? 1 : 0;
    }
    k112PackWanted(want, 36, b300, b310);
    expect_eq("A 0x300[0]", b300[0], 0x3F);
    expect_eq("A 0x300[1]", b300[1], 0x3F);
    expect_eq("A 0x300[2]", b300[2], 0x3F);
    expect_eq("A 0x300[3]", b300[3], 0x3F);
    expect_eq("A 0x300[4]", b300[4], 0);
    expect_eq("A 0x310 empty", b310[3] | b310[4], 0);

    // Sparse 12S: 0x46D cells 25-27 bits 0-2; 0x47D 28-30 bits 3-5 on 0x310[3]
    for (int c = 1; c <= 36; c++) {
        want[c] = (c >= 25 && c <= 30) ? 1 : 0;
    }
    k112PackWanted(want, 36, b300, b310);
    expect_eq("B 0x310[3] 46D/47D", b310[3], 0x3F);
    expect_eq("B 0x310[4] empty", b310[4], 0);
    expect_eq("B 0x300 empty", b300[0] | b300[3], 0);

    // 0x46E/0x47E → 0x310[4]
    for (int c = 1; c <= 36; c++) {
        want[c] = (c == 36) ? 1 : 0;
    }
    k112PackWanted(want, 36, b300, b310);
    expect_eq("cell36 0x310[4]", b310[4], 0x20);

    // Adjacent drop keeps the higher cell
    float cells[37] = {};
    uint8_t adj[37] = {};
    cells[5] = 4.00f;
    cells[6] = 4.10f;
    adj[5] = 1;
    adj[6] = 1;
    k112DropAdjacent(adj, cells, 36);
    expect_eq("drop cell5", adj[5], 0);
    expect_eq("keep cell6", adj[6], 1);

    if (g_fail) {
        printf("%d failure(s)\n", g_fail);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
