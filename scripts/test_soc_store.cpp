// Host check of include/SocStore.h (SoC count that survives power-off).
//   /workspace/tmp/zigvenv/bin/python -m ziglang c++ -std=c++17 -O1 -Iinclude scripts/test_soc_store.cpp -o /tmp/ss && /tmp/ss
#include <cstdio>
#include <cstring>
#include "SocStore.h"
using namespace BmsCore;
static uint8_t g_mem[4284]; static int g_wr = 0, g_tear = -1;
static uint8_t rd(int a) { return g_mem[a]; }
static void wr(int a, uint8_t v) { if (g_tear >= 0 && g_wr++ >= g_tear) return; if (g_mem[a] != v) g_mem[a] = v; }
static int pass = 0, fail = 0;
static void check(bool c, const char *m) { std::printf("%s %s\n", c ? "PASS" : "FAIL", m); (c ? pass : fail)++; }
static SocSaved rec(float used, float anchor = 100, float cap = 0, bool known = true) {
    SocSaved s; s.known = known; s.usedAh = used; s.anchorPct = anchor; s.capAh = cap; return s;
}
int main() {
    const LogStore st{rd, wr};
    std::memset(g_mem, 0xFF, sizeof g_mem);
    char m[160];
    std::snprintf(m, sizeof m, "layout: %d..%d after DriveStore (%d), inside 4284 B", SocStore::kBase, SocStore::kEnd - 1, DriveProfile::Store::kEnd);
    check(SocStore::kBase == DriveProfile::Store::kEnd && SocStore::kEnd <= 4284, m);

    SocStore s; s.begin(st); SocSaved o;
    check(!s.load(o), "blank EEPROM: nothing restored (stay UNKNOWN)");

    check(s.update(0, rec(0), true), "anchor at full: written (force)");
    check(!s.update(1000, rec(0.3f), false), "0.3 Ah moved: no write (< 0.5 Ah step)");
    check(!s.update(30000, rec(0.6f), false), "0.6 Ah after 30 s: no write (< 60 s gap)");
    check(s.update(61000, rec(0.6f), false), "0.6 Ah after 61 s: written");
    check(s.update(62000, rec(12.25f, 100, 0), true), "key-off edge: written immediately (force)");
    check(!s.update(63000, rec(12.25f, 100, 0), true), "identical record: not rewritten");

    SocStore b; b.begin(st);
    check(b.load(o) && o.known && o.usedAh == 12.25f && o.anchorPct == 100, "reboot: restores 12.25 Ah used, anchor 100");

    // torn write: the 3rd byte onward of the next record never lands
    g_wr = 0; g_tear = 3; b.update(0, rec(20.0f), true); g_tear = -1;
    SocStore c; c.begin(st);
    check(c.load(o) && o.usedAh == 12.25f, "power pulled mid-write: previous count (12.25 Ah) restored");

    c.update(0, rec(20.0f, 80, 0), true);
    SocStore d; d.begin(st);
    check(d.load(o) && o.usedAh == 20.0f && o.anchorPct == 80, "next write lands in the good slot: 20 Ah, anchor P80");

    d.update(0, rec(0, 100, 0, false), true);
    SocStore e; e.begin(st);
    check(!e.load(o), "count went UNKNOWN (current lost): stale count NOT restored after reboot");

    // corrupt both slots -> nothing
    e.update(0, rec(5.0f), true);
    for (int i = SocStore::kBase; i < SocStore::kEnd; i += 7) g_mem[i] ^= 0x5A;
    SocStore f; f.begin(st);
    check(!f.load(o), "both slots corrupt (CRC): nothing restored");

    // out-of-range value with a valid CRC is rejected
    std::memset(g_mem, 0xFF, sizeof g_mem);
    SocStore g; g.begin(st); g.update(0, rec(5.0f, 150), true);
    SocStore h; h.begin(st);
    check(!h.load(o), "anchorPct 150 (valid CRC) rejected as out of range");

    // learned capacity + drift survive
    std::memset(g_mem, 0xFF, sizeof g_mem);
    SocSaved r = rec(3.5f, 100, 42.0f); r.drift = true;
    SocStore i; i.begin(st); i.update(0, r, true);
    SocStore j; j.begin(st);
    check(j.load(o) && o.capAh == 42.0f && o.drift, "learned capAh + drift flag survive reboot");

    // wear cap per boot
    SocStore k; k.begin(st); int w = 0;
    for (int n = 0; n < 500; n++) w += k.update(n * 1000u, rec(n * 1.0f), true);
    std::snprintf(m, sizeof m, "wear cap: %d writes in one boot (max %d), limited flag set", w, SocStore::kMaxPerBoot);
    check(w == SocStore::kMaxPerBoot && k.limited(), m);

    std::printf("%d/%d pass\n", pass, pass + fail);
    return fail ? 1 : 0;
}
