// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// tools/fx68kbench/run.sh: the port's interpreter (machine/m68k.cpp) on the same image as tb.cpp: each case's test instruction's cycles, the
// memory at the end (pb.mem). Optional: a level-1 interrupt raised IRQ_AT cycles after the Nth marker A's write
// begins (as tb.cpp), with the board's rule (SetInterruptLevel at that cycle).
//   pb IMAGE MEMORY CASES|- [CASE CYCLES]
#include "machine/m68k.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
using namespace spelunk;
struct Bus : M68KBus {
    std::vector<uint8_t> mem = std::vector<uint8_t>(1 << 24);
    long markersA = -1, irqCase = -1; uint64_t markerAt = 0; bool end = false, markA = false;
    uint8_t Read8(uint32_t a) override { return mem[a & 0xFFFFFF]; }
    uint16_t Read16(uint32_t a) override { a &= 0xFFFFFE; return (mem[a] << 8) | mem[a + 1]; }
    void Write8(uint32_t a, uint8_t v) override { mem[a & 0xFFFFFF] = v; }
    void Write16(uint32_t a, uint16_t v) override {
        a &= 0xFFFFFE;
        if (a == 0xF00000) { markA = true; return; }
        if (a == 0xF00004) { end = true; return; }
        if (a == 0xF00002) { markB = true; return; }
        if ((a & 0xFFFFF0) == 0xF00000) return;
        mem[a] = v >> 8; mem[a + 1] = v & 0xFF;
    }
    uint16_t Fetch16(uint32_t a) override { return Read16(a); }
    bool acked = false, markB = false;
    void InterruptAck(unsigned) override { acked = true; }
};
int main(int argc, char **argv) {
    Bus bus;
    FILE *f = fopen(argv[1], "rb");
    size_t n = fread(bus.mem.data(), 1, bus.mem.size(), f);
    (void)n;
    fclose(f);
    long irqCase = argc > 4 ? atol(argv[4]) : -1, irqAt = argc > 5 ? atol(argv[5]) : 0;
    std::map<uint32_t, std::string> cases;
    if (FILE *c = fopen(argc > 3 && strcmp(argv[3], "-") ? argv[3] : "/dev/null", "r")) {
        char line[256];
        while (fgets(line, sizeof line, c)) {
            unsigned at, dump;
            char name[200];
            if (sscanf(line, "%x %x %[^\n]", &at, &dump, name) == 3) cases[at] = name;
        }
        fclose(c);
    }
    M68K cpu(bus);
    cpu.ram = nullptr;
    cpu.ramSize = 0;
    cpu.Reset(true);
    long markers = -1;
    for (uint64_t i = 0; i < 100000000 && !bus.end; i++) {
        const uint32_t pc = cpu.pc;
        const uint64_t before = cpu.cycles;
        const uint32_t c = cpu.Step();
        cpu.cycles += c;
        if (bus.acked) {
            bus.acked = false;
            cpu.SetInterruptLevel(0);
        }
        if (bus.markB) {
            bus.markB = false;
            if (irqCase >= 0) printf("B %llu\n", (unsigned long long)(cpu.cycles - 8));
        }
        if (bus.markA) {
            // (the marker's write is 8 cycles before its end: move.w d0,abs.l, as the board's writeTime)
            bus.markA = false;
            markers++;
            if (irqCase >= 0) printf("A %llu\n", (unsigned long long)(cpu.cycles - 8));
            if (markers == irqCase) {
                // the line rises at (write start) + irqAt; the write began 8 cycles before this move's end
                cpu.SetInterruptLevel(1, cpu.cycles - 8 + irqAt);
            }
        }
        if (auto it = cases.find(pc); it != cases.end() && irqCase < 0) printf("%u %s\n", c, it->second.c_str());
        (void)before;
    }
    FILE *o = fopen(argv[2], "wb");
    fwrite(bus.mem.data(), 1, 1 << 20, o);
    fclose(o);
}
