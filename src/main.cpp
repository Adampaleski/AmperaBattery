#include "FlexCanCompat.h"
#include "ThrottleTrim.h"
#include "CurrentSense.h"
#include "KellyLimit.h"
#include <Arduino.h>
#include "Config.h"
#include "CanBus.h"
#include "ChargerCan.h"
#include "BicmSniffer.h"
#include "BicmDecode.h"
#include "BmsApp.h"
#include "FaultLogRun.h"
#include "SocRun.h"
#include "BmsCapabilities.h"
#include "BicmPackProfile.h"
#include "BalanceTx.h"
#include "ChargeTx.h"
#include "ContactorSeq.h"
#include "KellyDecode.h"
#include "HubStatus.h"
#include "Watchdog.h"
#include "BenchBleedRun.h"
#include "Settings.h"
#include "StringConfig.h"
#if DUAL_STRING
#include "Can1Bus.h"
#include "Link.h"
#include "StringMon.h"
#endif

namespace {

void warnCapsCompileTime() {
    SERIALCONSOLE.println(
        F("NOTE: BMS_CAP_BALANCE_TX / CHARGE_TX / CONTACTOR_DRV are compile-time."));
    SERIALCONSOLE.println(
        F("      Serial cannot enable TX or coil drive — edit include/BmsCapabilities.h and reflash."));
    SERIALCONSOLE.print(F("      This build FLAG balance="));
    SERIALCONSOLE.print(BMS_CAP_BALANCE_TX);
    SERIALCONSOLE.print(F(" charge="));
    SERIALCONSOLE.print(BMS_CAP_CHARGE_TX);
    SERIALCONSOLE.print(F(" contactors="));
    SERIALCONSOLE.print(BMS_CAP_CONTACTOR_DRV);
    SERIALCONSOLE.println(F("  → MONITOR / PRINT-ONLY dry-runs only."));
}

void onCanFrame(const CAN_message_t &msg) {
    BicmSniffer::onFrame(msg);
    BicmDecode::onFrame(msg);
#if DUAL_STRING
    StringMon::onFrame(0, msg);  // CAN3 = string 1
#endif
}

#if DUAL_STRING
void onCan1Frame(const CAN_message_t &msg) {
    StringMon::onFrame(1, msg);  // CAN1 = string 2
}
#endif

void onChargerFrame(const CAN_message_t &msg) {
    ChargeTx::onFrame(msg);
    KellyDecode::onFrame(msg);  // listen-only; BMS_CAP_KELLY_RX gates unpack
#if DUAL_STRING
    Link::onFrame(msg);
#endif
}

void printHelp() {
    SERIALCONSOLE.println(F("=== KEYS (single key, no Enter; click the terminal first) ==="));
    SERIALCONSOLE.println(F("  d  cells per module, LOW-LIVE diagnostic, temps (UNVERIFIED)"));
    SERIALCONSOLE.println(F("  s  CAN ID histogram          r  reset sniffer counters"));
    SERIALCONSOLE.println(F("  c  toggle candump (every frame)"));
    SERIALCONSOLE.println(F("  k  toggle 0x200 keep-alive (BICMs stop reporting without it)"));
    SERIALCONSOLE.println(F("  b  balance plan per board + WOULD TX 0x300/0x310/0x200 (PRINT-ONLY)"));
    SERIALCONSOLE.println(F("  e  toggle dead-man -> contactor sequence (PRINT-ONLY, coils stay LOW)"));
    SERIALCONSOLE.println(F("  p  contactor state, wanted vs driven pins"));
    SERIALCONSOLE.println(F("  g  toggle charge request; shows refusal reason + would-be Elcon frame"));
    SERIALCONSOLE.println(F("  y  Kelly CAN2 RX counts/raw"));
    SERIALCONSOLE.println(F("  j  one STATUS_SCHEMA v1 JSON line (hub format)"));
    SERIALCONSOLE.println(F("  f  fault log (survives power-off; dual role A)   FCLEAR + Enter = erase it"));
    SERIALCONSOLE.println(F("  h/?  this page"));
#if BENCH_BLEED_TEST
    SERIALCONSOLE.println(F("  bleed M C + Enter  BENCH: bleed ONE cell (module M ch C) 10 s; x aborts"));
    SERIALCONSOLE.println(F("  b + Enter          balance plan (in this build b needs Enter)"));
#endif
    SERIALCONSOLE.println(F("=== SETTINGS (type, then Enter; saved to EEPROM) ==="));
    SERIALCONSOLE.println(F("  P80    charge limit % (50-100; 80=3.95 V, 100=4.10 V/cell)   L3.30  low cutoff V/cell (3.00-3.50)"));
    SERIALCONSOLE.println(F("  T      list BMS thresholds   TocA 600  set one (bounded; trips/temps tighten only)   TocA -  default   TX  all defaults"));
    SERIALCONSOLE.println(F("  W10    balance deadband mV (2-50)          X      restore defaults"));
    SERIALCONSOLE.println(F("  U145.0 SIMULATE HV bus volts for the dry-run precharge (U0 = off)"));
    SERIALCONSOLE.println(F("  I120   SIMULATE pack amps (I0 = off)       Z      zero current sensor (no current flowing)"));
    SERIALCONSOLE.println(F("Auto report every 10 s. Watchdog ~2 s: a hung loop resets the Teensy."));
    warnCapsCompileTime();
}

void runCommand(char c) {
    if (c >= 'A' && c <= 'Z') {
        c = static_cast<char>(c - 'A' + 'a');
    }
    // Ignore noise (CR/LF/space) from terminal Enter / paste.
    if (c == '\r' || c == '\n' || c == ' ' || c == '\t') {
        return;
    }
    SERIALCONSOLE.print(F(">> cmd "));
    SERIALCONSOLE.println(c);

    switch (c) {
            case 'c':
                BicmSniffer::setCanDebug(!BicmSniffer::canDebugEnabled());
                break;
            case 's':
                BicmSniffer::printIdStats();
                break;
            case 'r':
                BicmSniffer::resetStats();
                break;
            case 'k':
                CanBus::setKeepAliveEnabled(!CanBus::keepAliveEnabled());
                SERIALCONSOLE.print(F("Keep-alive "));
                SERIALCONSOLE.println(CanBus::keepAliveEnabled() ? F("ON") : F("OFF"));
                break;
            case 'd':
                BicmDecode::printDecodeDetails();
                ThrottleTrim::print();
#if DUAL_STRING
                StringMon::print();
                Link::print();
                Can1Bus::printStatus();
#endif
                break;
            case 'b':
                BalanceTx::printStatus();
#if DUAL_STRING
                StringMon::printBalancePlan();
#endif
                break;
            case 'e':
                BmsApp::handleSerial();
                break;
            case 'p':
                ContactorSeq::printStatus();
                CurrentSense::printStatus();
                SocRun::printStatus();
                break;
            case 'g':
                ChargeTx::toggleChargeRequest();
                break;
            case 'y':
                KellyDecode::printStatus();
                KellyLimit::printStatus();
                break;
            case 'f':
                FaultLogRun::print();
                break;
            case 'j':
                SERIALCONSOLE.println(F("--- hub STATUS_SCHEMA v1 (USB one-shot) ---"));
                HubStatus::printFrame(SERIALCONSOLE);
                break;
            case '?':
            case 'h':
                printHelp();
                break;
            default:
                // Refuse mistaken "enable flag" / unknown attempts — caps are not runtime.
                SERIALCONSOLE.print(F("Unknown key '"));
                SERIALCONSOLE.print(c);
                SERIALCONSOLE.println(F("' — ignored. Press h or ? for the key list."));
                SERIALCONSOLE.println(
                    F("  (Flags are compile-time; no key can enable balance/charge/contactor output.)"));
                break;
    }
}

// Uppercase P/V/L/W/X/U/T/Z/I start a settings line (ended by Enter); every other key
// is a single-key command as before.
char     g_line[24];
uint8_t  g_lineLen = 0;
bool     g_inLine  = false;
uint32_t g_lineMs  = 0;

void dispatchSerial() {
    while (SERIALCONSOLE.available()) {
        const char c = static_cast<char>(SERIALCONSOLE.read());
#if BENCH_BLEED_TEST
        // x / X aborts a running bleed immediately (before any other parsing).
        if (BenchBleedRun::active() && (c == 'x' || c == 'X')) {
            g_inLine = false;
            BenchBleedRun::abort("x pressed");
            continue;
        }
        if (!g_inLine && c == 'b') {  // "bleed M C" + Enter; bare "b" + Enter = balance plan
            g_inLine = true;
            g_lineLen = 0;
        }
#endif
        if (!g_inLine && (c == 'P' || c == 'V' || c == 'L' || c == 'W' || c == 'X' || c == 'U' ||
                          c == 'T' || c == 'Z' || c == 'I' || c == 'F')) {
            g_inLine = true;
            g_lineLen = 0;
        }
        if (g_inLine) {
            g_lineMs = millis();
            if (c == '\r' || c == '\n') {
                g_line[g_lineLen] = 0;
                g_inLine = false;
#if BENCH_BLEED_TEST
                if (g_line[0] == 'b') {
                    if (!BenchBleedRun::handleLine(g_line)) runCommand('b');
                } else
#endif
                if (!FaultLogRun::handleLine(g_line))
                if (!ThrottleTrim::handleLine(g_line))
                Settings::handleLine(g_line);
            } else if (g_lineLen < sizeof(g_line) - 1) {
                g_line[g_lineLen++] = c;
            }
            continue;
        }
        runCommand(c);
    }
    if (g_inLine && millis() - g_lineMs > 10000) {
        g_inLine = false;
        SERIALCONSOLE.println(F("Settings line timed out (press Enter after e.g. V4.05)"));
    }
}

}  // namespace

// Chief review fix #3 (2026-10-22): every coil / relay / PWM driver and the MCP4131 digipot lines are
// latched to their safe level BEFORE anything else runs (no EEPROM, no Serial, no ThrottleTrim first).
// Order per pin: write the level, then make it an output, so the pin never drives HIGH.
// MCP4131 /CS is active LOW (SOURCED: Microchip MCP4131 datasheet), so its safe level is HIGH = deselected;
// SDI/SCK idle LOW. The wiper itself still powers up at mid-scale until ThrottleTrim::begin() writes 0
// (hardware gate, chief review CRITICAL #10). Pins float from reset until here: 10k gate pull-downs (Gate 0).
static void safeOutputsFirst() {
    static const int kLow[] = { Pins::OUT1, Pins::OUT2, Pins::OUT3, Pins::OUT4, Pins::OUT5, Pins::OUT6,
                                Pins::OUT7, Pins::OUT8, Pins::OUT9, Pins::OUT10,   // OUT9 fan relay, OUT10 pump (chief OK 2026-10-22)
                                Pins::LIGHT_DATA, Pins::LIGHT_CLK };
    for (int pin : kLow) { if (pin >= 0) { digitalWriteFast(pin, LOW); pinMode(pin, OUTPUT); digitalWriteFast(pin, LOW); } }
    if (Pins::THR_CS >= 0) { digitalWriteFast(Pins::THR_CS, HIGH); pinMode(Pins::THR_CS, OUTPUT); digitalWriteFast(Pins::THR_CS, HIGH); }
}

void setup() {
    safeOutputsFirst();      // FIRST statement: coil drivers LOW, digipot bus idle (chief review fix #3)
    ThrottleTrim::begin();   // wiper 0 (reset/watchdog safe state)
    Watchdog::captureResetCause();

    pinMode(Pins::LED, OUTPUT);
    digitalWrite(Pins::LED, HIGH);

    SERIALCONSOLE.begin(115200);
    delay(1500);

    SERIALCONSOLE.println();
    SERIALCONSOLE.println(F("=== Ampera BICM Teensy 4.1 ==="));
    SERIALCONSOLE.print(F("Last reset: "));
    SERIALCONSOLE.print(Watchdog::lastResetWasWatchdog() ? F("*** WATCHDOG ***") : F("normal"));
    SERIALCONSOLE.print(F("  SRC_SRSR=0x"));
    SERIALCONSOLE.println(Watchdog::lastResetRaw(), HEX);
    SERIALCONSOLE.println(
        F("*** BUILD: MONITOR / DRY-RUN — FLAG=0  DRIVE=OFF  no live bleed/charge/coil ***"));
#if BENCH_BLEED_TEST
    SERIALCONSOLE.println(F("*** BENCH BLEED TEST BUILD: 'bleed <module> <ch>' + Enter bleeds ONE cell for 10 s (x = abort). 'b' + Enter = balance plan. ***"));
#endif
    SERIALCONSOLE.print(F("Pack: "));
    SERIALCONSOLE.print(PACK_S_CELLS);
    SERIALCONSOLE.print(F("S"));
#if FACTORY_BUS_DECODE
    SERIALCONSOLE.println(F(" mapped / up to 4 factory-strapped BICMs on ONE CAN3 bus (FactoryCellMap.h)"));
#elif K112_PACK_DECODE
    SERIALCONSOLE.print(F(" / "));
    SERIALCONSOLE.print(kBicmStreamCount);
    SERIALCONSOLE.println(F(" physical BICM(s) on daisy chain"));
#else
    SERIALCONSOLE.print(F(" / "));
    SERIALCONSOLE.print(PACK_MODULE_COUNT);
    SERIALCONSOLE.print(F(" modules / "));
    SERIALCONSOLE.print(BICM_CELLS_PER_MODULE);
    SERIALCONSOLE.println(F(" cells per module"));
#endif
#if TELEMETRY_ONLY
    SERIALCONSOLE.println(F("Mode: MONITOR — keep-alive + decode only; charge/contactor PRINT-ONLY"));
#else
    SERIALCONSOLE.println(F("Mode: BMS GPIO (coil drive still gated by BMS_CAP_CONTACTOR_DRV)"));
#endif
#if BMS_CAP_BALANCE_TX
    SERIALCONSOLE.println(F("CAN3 @ 125k — keep-alive 0x200/1s + LIVE balance 0x300/0x310"));
#else
    SERIALCONSOLE.println(
        F("CAN3 @ 125k — keep-alive 0x200/1s  |  balance FLAG=0 PRINT-ONLY (b)"));
#endif
#if BMS_CAP_CHARGE_TX
    SERIALCONSOLE.println(F("CAN2 @ 250k pins 0/1 — LIVE Elcon 0x1806E5F4 TX"));
#else
    SERIALCONSOLE.println(
        F("CAN2 @ 250k pins 0/1 — Elcon FLAG=0 PRINT-ONLY (g); bus optional this weekend"));
#endif
    SERIALCONSOLE.print(F("Kelly KLS RX on CAN2 (share Elcon): FLAG="));
    SERIALCONSOLE.print(BMS_CAP_KELLY_RX);
    SERIALCONSOLE.println(F("  keys: y=status  IDs 0x0CF11E05/0x0CF11F05"));
    SERIALCONSOLE.print(F("Capabilities: cells="));
    SERIALCONSOLE.print(BMS_CAP_READ_CELL_VOLTS);
    SERIALCONSOLE.print(F(" keepalive="));
    SERIALCONSOLE.print(BMS_CAP_KEEPALIVE_TX);
    SERIALCONSOLE.print(F(" balance="));
    SERIALCONSOLE.print(BMS_CAP_BALANCE_TX);
    SERIALCONSOLE.print(F(" charge="));
    SERIALCONSOLE.print(BMS_CAP_CHARGE_TX);
    SERIALCONSOLE.print(F(" contactors="));
    SERIALCONSOLE.println(BMS_CAP_CONTACTOR_DRV);
    warnCapsCompileTime();
#if FACTORY_BUS_DECODE
    SERIALCONSOLE.println(F("Profile: factory IDs M1 0x460-463/470-473, M2 0x464-466/474-476,"));
    SERIALCONSOLE.println(F("         M3 0x468-46A/478-47A, M4 0x46C-46E/47C-47E.  d = per-module cells"));
#elif K112_PACK_DECODE
#if PACK_BICM_COUNT == 1
    SERIALCONSOLE.println(F("Profile: one K112, 24S (0x460/0x470 burst)"));
#elif PACK_BICM_COUNT == 2
#if K112_SPARSE_SECOND_BICM
    SERIALCONSOLE.println(F("Profile: 36S — A burst 0x460/470, B sparse 0x46D-0x47E"));
#else
    SERIALCONSOLE.println(F("Profile: two KICMs, 36S (A=0x460/470, B=0x461/471)"));
#endif
    SERIALCONSOLE.println(F("If B cells stay 0, run `s` and adjust BicmSparseMap.h."));
#endif
#else
    SERIALCONSOLE.print(F("Expect "));
    SERIALCONSOLE.print(PACK_MODULE_COUNT);
    SERIALCONSOLE.print(F(" CAN module(s) = "));
    SERIALCONSOLE.print(PACK_S_CELLS);
    SERIALCONSOLE.println(F(" cells."));
#endif
    SERIALCONSOLE.println(F("Keys: c s d b k r e p g y j f ?  (no Enter — click terminal first)"));
    SERIALCONSOLE.println(F("b/e/p/g = PRINT-ONLY dry-runs. ID list + cells every 10 s."));

    Settings::begin();  // prints ceiling / low cutoff / deadband from EEPROM
#if DUAL_STRING
    SERIALCONSOLE.print(F("DUAL-STRING 4P  role="));
    SERIALCONSOLE.println(ROLE_IS_A ? F("A (master: contactor SM, listens to B)") : F("B (reports to A, no contactors)"));
    SERIALCONSOLE.println(F("  " BICM1_NAME " @125k = string 1   " BICM2_NAME " @125k = string 2   " VEH_NAME " @250k = Elcon+Kelly+link"));
    SERIALCONSOLE.print(F("  string1 circuits=0x"));
    SERIALCONSOLE.print(static_cast<uint32_t>(STRING1_CIRCUITS), HEX);
    SERIALCONSOLE.print(F("  string2 circuits=0x"));
    SERIALCONSOLE.print(static_cast<uint32_t>(STRING2_CIRCUITS), HEX);
    SERIALCONSOLE.print(F("  each "));
    SERIALCONSOLE.print(STRING_EXPECT_S);
    SERIALCONSOLE.println(F("S  (bit n = circuit n, see StringConfig.h)"));
    SERIALCONSOLE.println(F("  pins: PRE=12 MAIN+=11 MAIN-=21  OUT5=2 OUT6=3 (moved off 22/23)  B fault=4 (disabled)"));
#endif
    CanBus::begin();
    CanBus::setFrameHandler(onCanFrame);
#if DUAL_STRING
    Can1Bus::begin();
    Can1Bus::setFrameHandler(onCan1Frame);
    StringMon::begin();
    Link::begin();
#endif
    ChargerCan::begin();
    ChargerCan::setFrameHandler(onChargerFrame);
    BicmSniffer::begin();
    BicmDecode::begin();
    BalanceTx::begin();
    ChargeTx::begin();
    KellyDecode::begin();
    BmsApp::begin();
    FaultLogRun::begin();  // after Settings (EEPROM) + BmsApp; logs only, no pins
    SocRun::begin();       // SoC count state; read-only, no pins, no EEPROM
    HubStatus::begin();  // Serial4 after BmsApp so UART owns pins 16/17
    Watchdog::begin(2000);
    SERIALCONSOLE.println(F("Watchdog: WDOG3 ~2 s, fed from loop()"));
}

void loop() {
    Watchdog::feed();  // ONLY feed point
    CanBus::tick();
    ChargerCan::tick();
#if DUAL_STRING
    Can1Bus::tick();
    StringMon::tick();
    Link::tick();
#endif
    BicmSniffer::tick();
    BicmDecode::tick();
    BalanceTx::tick();
    ChargeTx::tick();
    KellyDecode::tick();
    BmsApp::tick();
    FaultLogRun::tick();   // after ContactorSeq ran BmsCore this loop
    SocRun::tick();        // same: uses Core "at charge limit" as the anchor
    ThrottleTrim::tick();
    HubStatus::tick();
    dispatchSerial();
    BenchBleedRun::tick();

    static uint32_t autoReportMs = 0;
    if (millis() - autoReportMs >= 10000) {
        autoReportMs = millis();
        SERIALCONSOLE.println(F("--- auto report (10s) ---"));
        BicmSniffer::printIdStats();
        BicmDecode::printDecodeDetails();
#if DUAL_STRING
        StringMon::print();
        Link::print();
        StringMon::printBalancePlan();
#endif
        BalanceTx::printStatus();
        ContactorSeq::printStatus();
        ChargeTx::printStatus();
        KellyDecode::printStatus();
    }

    static uint32_t ledMs = 0;
    if (millis() - ledMs >= 500) {
        ledMs = millis();
        digitalWrite(Pins::LED, !digitalRead(Pins::LED));
    }
}
