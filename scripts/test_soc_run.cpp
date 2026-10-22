// Host check of src/SocRun.cpp (firmware SoC wiring + hub JSON), with stubbed
// Arduino/Settings/CurrentSense/ContactorSeq. Role A dual string.
//   g++ -std=c++17 -DDUAL_STRING=1 -DTEENSY_ROLE=0 -Iscripts/hoststub -Iinclude -Isrc scripts/test_soc_run.cpp -o /tmp/tsr && /tmp/tsr
#include <stdio.h>
#include <string.h>
#include <string>
#include <Arduino.h>
#include "BmsCore.h"
uint32_t g_hostMs = 0;
Print Serial;
static BmsCore::Outputs g_out;
static bool g_valid = false; static float g_amps = 0;
namespace ContactorSeq { const BmsCore::Outputs &coreOut() { return g_out; } }
namespace CurrentSense { bool valid() { return g_valid; } float amps() { return g_amps; } }
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
           "\"known\":false,\"state\":\"unknown\",\"usedAh\":null,\"pct\":null,\"drift\":false,\"why\":\"no anchor yet\",\"halfBank\":true,\"capAh\":null} socPct=null");
    g_out.chargeWhy = "at charge limit"; run(20);
    expect("anchor: state anchored", json(), "\"known\":true,\"state\":\"anchored\",\"usedAh\":0.000");
    g_out.chargeWhy = "no request"; g_valid = true; g_amps = 100; run(36000);
    expect("100 A x 36 s (LEM A dry-run): ~1 Ah, pct null (half bank, capAh 0)",
           json() + " socPct=" + pct(), "\"state\":\"counting\",\"usedAh\":1.00");
    expect("socPct stays null", pct(), "null");
    g_valid = false; run(3000);
    expect("3 s current lost: unknown", json(), "\"known\":false,\"state\":\"unknown\",\"usedAh\":null");
    printf("socrun: %d/%d checks passed\n", n - fails, n);
    return fails ? 1 : 0;
}
