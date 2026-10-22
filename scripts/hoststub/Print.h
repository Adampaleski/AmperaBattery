#pragma once
// Host stub of Arduino Print (enough for the JSON printers in src/*Run.cpp).
#include <stdio.h>
#include <string>
#define F(s) (s)
struct Print {
    std::string buf;
    void print(const char *s) { buf += s; }
    void print(char c) { buf += c; }
    void print(int v) { buf += std::to_string(v); }
    void print(unsigned v) { buf += std::to_string(v); }
    void print(long v) { buf += std::to_string(v); }
    void print(unsigned long v) { buf += std::to_string(v); }
    void print(double v, int d = 2) { char t[32]; snprintf(t, sizeof t, "%.*f", d, v); buf += t; }
    void println() { buf += '\n'; }
    template <class T> void println(T v) { print(v); println(); }
};
