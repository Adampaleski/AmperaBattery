#pragma once
namespace BenchBleedRun {
bool active();
void start(int module, int cell);   // runs guards, prints refusal or arms
void abort(const char *why);        // all-off now
void tick();                        // resend / print / timeout
bool handleLine(const char *line);  // "bleed M C"; true if consumed
}
