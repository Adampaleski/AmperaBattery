#pragma once
// Pack current (factory Volt LEM DHAB S/44). Placeholder until the
// CurrentSense commit: reports "not fitted".
namespace CurrentSense {
inline void  begin() {}
inline void  tick() {}
inline bool  valid() { return false; }
inline float amps() { return 0.0f; }
}  // namespace CurrentSense
