#include "KellyLimit.h"
#include <Arduino.h>
#include "BmsCapabilities.h"
#include "Config.h"
#include "ContactorSeq.h"

namespace KellyLimit {

#if DUAL_STRING && ROLE_IS_A
namespace {
constexpr bool kDrive = (BMS_CAP_KELLY_LIMIT_DRV != 0) && !BMS_CAP_TELEMETRY_ONLY;
bool g_limp = false, g_regen = false;
float g_pct = 0.0f;
}  // namespace

void tick() {
    const BmsCore::Outputs &o = ContactorSeq::coreOut();
    g_limp  = o.kellyLimp;
    g_regen = o.kellyRegenOk;
    g_pct   = o.kellyPct;
    if (!kDrive) return;
    digitalWrite(Pins::OUT3, g_limp ? HIGH : LOW);
    analogWrite(Pins::OUT5, static_cast<int>(g_pct * 2.55f + 0.5f));
    digitalWrite(Pins::OUT6, g_regen ? HIGH : LOW);
}

void printStatus() {
    SERIALCONSOLE.print(F("KELLY LIMIT ("));
    SERIALCONSOLE.print(kDrive ? F("DRIVING pins") : F("PRINT-ONLY, BMS_CAP_KELLY_LIMIT_DRV=0"));
    SERIALCONSOLE.print(F(")  allowed="));
    SERIALCONSOLE.print(g_pct, 0);
    SERIALCONSOLE.print(F("%  limp(OUT3/20)="));
    SERIALCONSOLE.print(g_limp ? F("ON") : F("off"));
    SERIALCONSOLE.print(F("  regen_ok(OUT6/3)="));
    SERIALCONSOLE.println(g_regen ? F("yes") : F("no"));
    SERIALCONSOLE.println(F("  KLS CAN is broadcast-only (0x0CF11E05/0x0CF11F05): limits use digital inputs, not CAN"));
}
#else
void tick() {}
void printStatus() {}
#endif

}  // namespace KellyLimit
