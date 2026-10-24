// Host check of src/SocRun.cpp (firmware SoC wiring + hub JSON), with stubbed
// Arduino/Settings/CurrentSense/ContactorSeq. Role A dual string.
//   g++ -std=c++17 -DDUAL_STRING=1 -DTEENSY_ROLE=0 -Iscripts/hoststub -Iinclude -Isrc scripts/test_soc_run.cpp -o /tmp/tsr && /tmp/tsr
#include <stdio.h>
#include <string.h>
#include <string>
#include <Arduino.h>
#include <EEPROM.h>
#include "BmsCore.h"
uint32_t g_hostMs = 0;
HostEeprom EEPROM;
Print Serial;
static BmsCore::Outputs g_out;
static bool g_valid = false; static float g_amps = 0;
namespace ContactorSeq { const BmsCore::Outputs &coreOut() { return g_out; } }
namespace CurrentSense { bool valid() { return g_valid; } float amps() { return g_amps; } }
static bool g_bValid = false; static float g_bAmps = 0;   // LEM B over the link (Link::remoteLemB)
namespace Link { bool remoteLemB(float *a) { if (g_bValid) *a = g_bAmps; return g_bValid; } }
namespace Settings { int chargePct() { return 100; } float lowCutoffV() { return 3.30f; } }
#include "../src/SocRun.cpp"

static int fails = 0, n = 0;
static void expect(const char *name, const std::string &got, const char *want) {
    n++;
    const bool ok = got.find(want) != std::string::npos;
    if (!ok) fails++;
    printf("%s %s\n   %s\n", ok ? "PASS" : "FAIL", name, got.c_str());
}
static std::string json() { Print p; SocRun::printJson(p); return p.buf; }
static std::string pct() { Print p; SocRun::printSocPct(p); return p.buf; }
static void run(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10) { g_hostMs += 10; SocRun::tick(); } }

int main() {
    SocRun::begin();
    g_out.chargeWhy = "";
    run(100);
    expect("boot: unknown, socPct null", json() + " socPct=" + pct(),
           "\"known\":false,\"state\":\"unknown\",\"usedAh\":null,\"pct\":null,\"drift\":false,\"why\":\"no anchor yet\",\"halfBank\":false,\"lemA\":false,\"lemB\":false,\"capAh\":null,\"restored\":false,\"saves\":0} socPct=null");
    g_out.chargeWhy = "at charge limit"; run(20);
    expect("anchor: state anchored", json(), "\"known\":true,\"state\":\"anchored\",\"usedAh\":0.000");
    // LEM A alone is not enough: the tick is current-lost, nothing counted.
    g_out.chargeWhy = "no request"; g_valid = true; g_amps = 100; run(1000);
    expect("LEM A only (no LEM B): nothing counted", json(), "\"usedAh\":0.000");
    expect("LEM A only: lemA true, lemB false", json(), "\"lemA\":true,\"lemB\":false");
    // A 60 A + B 40 A = 100 A bank for 36 s = 1 Ah.
    g_amps = 60; g_bValid = true; g_bAmps = 40; run(36000);
    expect("A 60 A + B 40 A x 36 s: ~1 Ah summed, pct null (capAh 0)",
           json() + " socPct=" + pct(), "\"state\":\"counting\",\"usedAh\":1.00");
    expect("socPct stays null", pct(), "null");
    expect("both LEMs ok in JSON", json(), "\"halfBank\":false,\"lemA\":true,\"lemB\":true");
    expect("anchor saved once; 1 Ah in 36 s waits for the 60 s gap", json(), "\"restored\":false,\"saves\":1}");
    // Sleep edge: DRIVE -> IDLE forces a save of the exact count.
    g_out.mode = BmsCore::Mode::Drive; run(20);
    g_amps = 0; run(20);
    g_out.mode = BmsCore::Mode::Idle; run(20);
    const std::string beforePull = json();
    expect("key-off edge saved the count (saves >= 1)", beforePull, "\"saves\":");
    // Power pull + reboot: same EEPROM, fresh RAM.
    SocRun::begin(); run(20);
    expect("reboot: count restored from EEPROM", json(), "\"known\":true,\"state\":\"restored\",\"usedAh\":1.00");
    expect("reboot: restored flag + no new saves yet", json(), "\"restored\":true,\"saves\":0}");
    expect("reboot: socPct still null (capAh unknown)", pct(), "null");
    g_bValid = false; run(3000);
    expect("3 s LEM B lost (LEM A fine): unknown, fail closed", json(), "\"known\":false,\"state\":\"unknown\",\"usedAh\":null");
    g_valid = false;
    // Loss is persisted as known=0, so the stale count can't come back.
    SocRun::begin(); run(20);
    expect("reboot after loss: nothing restored (fail closed)", json(), "\"known\":false,\"state\":\"unknown\",\"usedAh\":null");
    expect("reboot after loss: restored false", json(), "\"restored\":false");
    // Corrupt both slots: still fail closed.
    for (int a = BmsCore::SocStore::kBase; a < BmsCore::SocStore::kEnd; a++) EEPROM.b[a] ^= 0x5A;
    SocRun::begin(); run(20);
    expect("corrupt EEPROM: nothing restored", json(), "\"restored\":false");
    // Never-known boot does not wear EEPROM.
    const int w0 = EEPROM.writes; g_out.chargeWhy = "no request"; run(120000);
    expect("never-known boot: zero EEPROM bytes written", std::to_string(EEPROM.writes - w0), "0");
    printf("socrun: %d/%d checks passed\n", n - fails, n);
    return fails ? 1 : 0;
}
