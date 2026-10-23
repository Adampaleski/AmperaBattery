#pragma once
// Block reasons (Adam t284, Build 15). ONE function that says, in plain words, why a driver request
// (drive / wake / shift / change a drive setting) is refused RIGHT NOW.
// NOT a fault: nothing here latches, logs, or touches FaultLog. The sim + dash show the result as a pop-up.
// Pure C++ (no Arduino). Core facts come from BmsCore::Outputs (plug lock, key-cycle lock, drive faults,
// contactor state); the rest (park brake, neutral, phone key, PIN, e-stop latch) are passed in by the caller.
// Host test: scripts/test_block_reasons.cpp. Sim: sim/wasm_api.cpp sim_block_mask / sim_block_text.
#include <cstdint>
#include "BmsCore.h"
namespace BmsCore {
namespace Block {
enum Action : uint8_t { Drive = 0, Wake = 1, Shift = 2, Setting = 3, kActions = 4 };
// Bit order = display order (most important first).
enum Bit : uint32_t {
    KeyOff      = 1u << 0,   // key off
    Estop       = 1u << 1,   // e-stop latch set (Estop::Latch)
    Plugged     = 1u << 2,   // charger plugged in -> drive locked (BmsCore plugIn)
    KeyCycle    = 1u << 3,   // key went on while plugged in -> key cycle needed (BmsCore keyRefused)
    Fault       = 1u << 4,   // a drive fault is active/latched (the fault itself is shown elsewhere)
    ParkFault   = 1u << 5,   // park-brake contact fault (ParkState)
    ParkOffWake = 1u << 6,   // PROPOSED: precharge needs the parking brake set
    Starting    = 1u << 7,   // key on, contactors still closing
    ParkOn      = 1u << 8,   // parking brake set: Kelly Foot_SW open
    Neutral     = 1u << 9,   // F/N/R in neutral
    PinDrive    = 1u << 10,  // PROPOSED PIN to Drive: phone not seen now and PIN not entered this key cycle (Build 18: caller evaluates it live, never latched)
    BrakeShift  = 1u << 11,  // PROPOSED: brake pressed to shift into / out of reverse
    PinSetting  = 1u << 12,  // PROPOSED phone-as-key: no phone, PIN not entered
    kBits       = 13
};
struct In {
    bool keyOn = false, plugIn = false, keyRefused = false, driveFault = false, ready = false;  // from BmsCore
    bool estopLatched = false, parkBrake = false, neutral = false, parkFault = false, prechargeAllowed = true;
    bool brakePressed = false, phone = false, pinOk = false, pinToDrive = false;
};
// Fill the BmsCore part from the core outputs (keyOn/plugIn are the driver's inputs).
inline void fromCore(In &i, const Outputs &o, bool keyOn, bool plugIn) {
    i.keyOn = keyOn; i.plugIn = plugIn; i.keyRefused = o.keyRefused;
    i.driveFault = ((o.faults | o.latched) & kDriveFaults) != 0;
    i.ready = o.state == CState::Closed;
}
inline uint32_t reasons(Action a, const In &i) {
    uint32_t m = 0;
    const bool pinLocked = i.pinToDrive && !i.phone && !i.pinOk;
    switch (a) {
    case Drive:
        if (!i.keyOn) m |= KeyOff;
        if (i.estopLatched) m |= Estop;
        if (i.plugIn) m |= Plugged;
        else if (i.keyRefused) m |= KeyCycle;
        if (i.driveFault) m |= Fault;
        if (i.keyOn && !i.ready && !(m & (Estop | Plugged | KeyCycle | Fault))) m |= Starting;
        if (i.parkBrake) m |= ParkOn;
        if (i.neutral) m |= Neutral;
        if (i.keyOn && pinLocked) m |= PinDrive;
        break;
    case Wake:
        if (i.estopLatched) m |= Estop;
        if (i.plugIn) m |= Plugged;
        if (i.parkFault) m |= ParkFault;
        else if (!i.prechargeAllowed) m |= ParkOffWake;
        break;
    case Shift:
        if (!i.keyOn) m |= KeyOff;
        if (!i.brakePressed) m |= BrakeShift;
        break;
    case Setting:
        if (!i.keyOn) m |= KeyOff;
        else if (!i.phone && !i.pinOk) m |= PinSetting;
        break;
    default: break;
    }
    return m;
}
inline const char *text(uint32_t bit) {
    switch (bit) {
    case KeyOff:      return "Key is off";
    case Estop:       return "E-stop latched: release it, then turn the key off and on";
    case Plugged:     return "Charger plugged in, drive locked";
    case KeyCycle:    return "Turn the key off and on again (it was on while plugged in)";
    case Fault:       return "A fault is stopping drive (see Faults)";
    case ParkFault:   return "Parking brake switch fault: clear park faults";
    case ParkOffWake: return "Set the parking brake to power up";
    case Starting:    return "Powering up, wait for the contactors";
    case ParkOn:      return "Parking brake is on, release it to drive";
    case Neutral:     return "Shifter in Neutral, pick forward or reverse";
    case PinDrive:    return "Enter PIN or bring phone to drive";
    case BrakeShift:  return "Brake must be pressed to shift";
    case PinSetting:  return "PIN needed (no phone nearby)";
    default:          return "";
    }
}
inline int count(uint32_t m) { int n = 0; for (; m; m &= m - 1) n++; return n; }
}  // namespace Block
}  // namespace BmsCore
