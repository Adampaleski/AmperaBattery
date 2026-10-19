// PC test for the factory BICM bleed map + print-only planner.
//   g++ -std=c++17 -Wall -Wextra -Iinclude scripts/test_factory_balance.cpp -o /tmp/tfb && /tmp/tfb
// No Teensy, no CAN. Exit code = number of failed checks.
#include "FactoryBalancePlan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace FactoryBalance;

static int g_fail = 0, g_ok = 0;
static void check(bool c, const char *what) {
    if (c) g_ok++; else { g_fail++; printf("FAIL %s\n", what); }
}

// ---- Tom's balanceCells(), transcribed from github.com/Tom-evnut/AmperaBattery
// @ f029e07 VoltBMSV2/BMSModuleManager.cpp lines 108-198 (MIT). Only CAN writes
// replaced by copying the buffers out; selection "avg < cell" kept as-is.
struct TomModule { bool exists = false; float cell[9] = {}; };
static void tomBalanceCells(TomModule modules[16], float avg, uint8_t o300[8], uint8_t o310[5]) {
    uint8_t buf[8];
    for (int c = 0; c < 8; c++) buf[c] = 0;
    for (int y = 1; y <= 9; y++) {
        if (modules[y].exists) {
            int balance = 0;
            for (int i = 1; i < 9; i++)
                if (avg < modules[y].cell[i]) balance = balance | (1 << (i - 1));
            if (y == 9) buf[y - 2] = balance;  // hack for missing module #8
            else        buf[y - 1] = balance;
        }
    }
    memcpy(o300, buf, 8);
    for (int c = 0; c < 8; c++) buf[c] = 0;
    for (int y = 10; y <= 15; y++) {
        if (modules[y].exists) {
            int balance = 0;
            for (int i = 1; i < 9; i++)
                if (avg < modules[y].cell[i]) balance = balance | (1 << (i - 1));
            if (y > 11) buf[y - 11] = balance;  // hack for missing module #12
            else        buf[y - 10] = balance;
        }
    }
    memcpy(o310, buf, 5);
}

// board (module, ch) -> (circuit, bit)
static bool locate(int m, int ch, int &ci, int &bit) {
    for (int i = 0; i < kCircuits; i++)
        if (kCircuit[i].module == m && ch >= kCircuit[i].firstCh && ch < kCircuit[i].firstCh + kCircuit[i].cells) {
            ci = i; bit = ch - kCircuit[i].firstCh; return true;
        }
    return false;
}

static void oneCell(int m, int ch, uint8_t b300[8], uint8_t b310[5]) {
    uint8_t bits[kCircuits] = {};
    int ci, bit;
    if (locate(m, ch, ci, bit)) bits[ci] = static_cast<uint8_t>(1u << bit);
    encode(bits, b300, b310);
}

static void handCase(int m, int ch, int frame, int byte, uint8_t want) {
    uint8_t b300[8], b310[5];
    oneCell(m, ch, b300, b310);
    uint8_t all[13];
    memcpy(all, b300, 8); memcpy(all + 8, b310, 5);
    const int idx = frame == 0 ? byte : 8 + byte;
    bool ok = all[idx] == want;
    for (int k = 0; k < 13; k++) if (k != idx && all[k]) ok = false;
    char what[96];
    snprintf(what, sizeof what, "%s ch%d -> 0x%s[%d]=0x%02X", kBoardName[m], ch, frame ? "310" : "300", byte, want);
    printf("%s %s (got 0x%02X)\n", ok ? "ok  " : "FAIL", what, all[idx]);
    check(ok, what);
}

static void line(const char *s) { puts(s); }

static uint32_t g_rng = 12345;
static float rnd(float lo, float hi) {
    g_rng = g_rng * 1103515245u + 12345u;
    return lo + (hi - lo) * ((g_rng >> 8) & 0xFFFF) / 65535.0f;
}

int main() {
    printf("== hand cases (Tom's layout; M1 rows also match swoozle #4 table)\n");
    handCase(1, 1, 0, 0, 0x01);   // swoozle: 0x460 cell1 = 300#01..
    handCase(1, 4, 0, 0, 0x08);   // swoozle: 0x470 cell4 = 08
    handCase(1, 6, 0, 0, 0x20);   // swoozle: 0x470 cell6 = 20
    handCase(1, 15, 0, 2, 0x01);  // swoozle: 0x462 cell15 = 300#00.00.01
    handCase(1, 22, 0, 2, 0x80);  // swoozle: 0x472 cell22 = 80
    handCase(2, 1, 0, 4, 0x01);   // M2 0x464 c1
    handCase(2, 5, 0, 4, 0x10);   // M2 0x474 c5 (8-cell: bit 4, not bit 0-5 group)
    handCase(2, 8, 0, 4, 0x80);   // M2 0x474 c8 -> bit 7
    handCase(2, 24, 0, 6, 0x80);  // M2 0x476 last
    handCase(3, 1, 0, 7, 0x01);   // M3 0x468 -> 0x300[7] (skips missing 0x467)
    handCase(3, 9, 1, 0, 0x01);   // M3 0x469 -> 0x310[0]
    handCase(3, 24, 1, 1, 0x80);  // M3 0x47A last
    handCase(4, 1, 1, 2, 0x01);   // M4 0x46C -> 0x310[2] (skips missing 0x46B)
    handCase(4, 4, 1, 2, 0x08);   // M4 0x47C first
    handCase(4, 18, 1, 4, 0x20);  // M4 0x47E last

    printf("== cross-check vs Tom's balanceCells() transcription, 2000 random packs\n");
    int mism = 0;
    for (int t = 0; t < 2000; t++) {
        TomModule mods[16];
        uint8_t bits[kCircuits] = {};
        float sum = 0; int n = 0;
        float v[kCircuits][8] = {};
        for (int i = 0; i < kCircuits; i++)
            for (int b = 0; b < kCircuit[i].cells; b++) { v[i][b] = rnd(3.70f, 4.10f); sum += v[i][b]; n++; }
        const float avg = sum / n;
        for (int i = 0; i < kCircuits; i++) {
            TomModule &tm = mods[tomCmu(i)];
            tm.exists = true;
            for (int b = 0; b < kCircuit[i].cells; b++) {
                tm.cell[b + 1] = v[i][b];
                if (avg < v[i][b]) bits[i] |= static_cast<uint8_t>(1u << b);
            }
        }
        uint8_t t300[8], t310[5], o300[8], o310[5];
        tomBalanceCells(mods, avg, t300, t310);
        encode(bits, o300, o310);
        if (memcmp(t300, o300, 8) || memcmp(t310, o310, 5)) mism++;
    }
    printf("%s Tom vs ours byte-identical: %d/2000 mismatches\n", mism ? "FAIL" : "ok  ", mism);
    check(mism == 0, "Tom cross-check");

    printf("== safety: random plans x 25 cycles\n");
    int bad = 0, adj = 0, full = 0, sampleDirty = 0;
    for (int t = 0; t < 2000; t++) {
        PlanInput in;
        for (int i = 0; i < kCircuits; i++) {
            in.use[i] = 1; in.fresh[i] = 1; in.group[i] = static_cast<uint8_t>(t & 1 ? kCircuit[i].module : 0);
            for (int b = 0; b < 8; b++) in.v[i][b] = rnd(3.90f, 4.10f);
        }
        if (t % 7 == 0) for (int i = 0; i < kCircuits; i++) for (int b = 0; b < 8; b++) in.v[i][b] = 4.05f;
        if (t % 7 == 0) in.v[0][0] = 3.92f;  // one low cell -> everything else is a candidate
        Plan p = evaluate(in);
        for (uint32_t c = 0; c < 25; c++) {
            uint8_t b300[8], b310[5]; int u = 0;
            cycleFrames(p, c, b300, b310, &u);
            if (u) bad++;
            for (int i = 0; i < kCircuits; i++) {
                const uint8_t x = kCircuit[i].frame ? b310[kCircuit[i].byte] : b300[kCircuit[i].byte];
                if (x & (x >> 1)) adj++;
                if (x && x == cellMask(i)) full++;
                if (cycleKind(c) == 2 && x) sampleDirty++;
            }
            // across circuit boundary in one board: last ch of circuit i, first of i+1
            for (int i = 0; i + 1 < kCircuits; i++) {
                if (kCircuit[i].module != kCircuit[i + 1].module) continue;
                const uint8_t x = kCircuit[i].frame ? b310[kCircuit[i].byte] : b300[kCircuit[i].byte];
                const uint8_t y = kCircuit[i + 1].frame ? b310[kCircuit[i + 1].byte] : b300[kCircuit[i + 1].byte];
                if ((x >> (kCircuit[i].cells - 1)) & 1u && (y & 1u)) adj++;
            }
        }
    }
    printf("%s no adjacent cells (in circuit or across circuit edge): %d violations\n", adj ? "FAIL" : "ok  ", adj);
    printf("%s never every cell of a circuit: %d violations\n", full ? "FAIL" : "ok  ", full);
    printf("%s safety guard never had to fire on planner output: %d\n", bad ? "FAIL" : "ok  ", bad);
    printf("%s sample cycles carry blank frames: %d dirty\n", sampleDirty ? "FAIL" : "ok  ", sampleDirty);
    check(!adj, "adjacent"); check(!full, "full circuit"); check(!bad, "guard"); check(!sampleDirty, "sample blank");

    {
        uint8_t b300[8] = {0x03, 0, 0, 0, 0, 0, 0, 0}, b310[5] = {};
        const bool a = framesUnsafe(b300, b310) == 1;
        b300[0] = 0; b310[2] = 0x40;  // bit 6 on a 6-cell circuit
        const bool c3 = framesUnsafe(b300, b310) == 3;
        printf("%s guard rejects crafted adjacent (0x03) and out-of-range (6-cell bit 6) frames\n", a && c3 ? "ok  " : "FAIL");
        check(a && c3, "guard crafted");
    }
    {
        PlanInput in;
        for (int i = 4; i <= 6; i++) { in.use[i] = in.fresh[i] = 1; for (int b = 0; b < 8; b++) in.v[i][b] = 3.95f; }
        in.v[5][2] = 3.20f;
        const bool low = evaluate(in).status == kBelowLowCut;
        in.v[5][2] = 3.95f; in.fresh[6] = 0;
        const bool stale = evaluate(in).status == kNotFresh;
        printf("%s low-cutoff cell and stale circuit both give 'no plan'\n", low && stale ? "ok  " : "FAIL");
        check(low && stale, "gates");
    }

    printf("\n== SAMPLE: one bus, M2 24S + M4 18S, one series string (group 0)\n");
    PlanInput in;
    const float m2[24] = {3.951f, 3.953f, 3.982f, 3.976f, 3.955f, 3.958f, 3.954f, 3.957f,
                          3.956f, 3.952f, 3.989f, 3.960f, 3.955f, 3.957f, 3.953f, 3.954f,
                          3.958f, 3.956f, 3.955f, 3.971f, 3.957f, 3.953f, 3.956f, 3.955f};
    const float m4[18] = {3.957f, 3.955f, 3.954f, 3.956f, 3.985f, 3.956f, 3.954f, 3.953f, 3.958f,
                          3.955f, 3.957f, 3.956f, 3.954f, 3.979f, 3.975f, 3.956f, 3.955f, 3.957f};
    for (int i = 4; i <= 6; i++) { in.use[i] = in.fresh[i] = 1; for (int b = 0; b < 8; b++) in.v[i][b] = m2[(i - 4) * 8 + b]; }
    for (int i = 10; i <= 12; i++) { in.use[i] = in.fresh[i] = 1; for (int b = 0; b < 6; b++) in.v[i][b] = m4[(i - 10) * 6 + b]; }
    in.deadbandV = 0.010f; in.lowCutoffV = 3.30f; in.minBleedV = 3.90f;
    Plan p = evaluate(in);
    printPlan(line, in, p, "BUS CAN3");

    printf("\n%d checks ok, %d failed\n", g_ok, g_fail);
    return g_fail;
}
