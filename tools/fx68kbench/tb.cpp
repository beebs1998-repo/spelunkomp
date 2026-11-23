// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// tools/fx68kbench/run.sh: fx68k alone on memory with no waits: runs IMAGE from reset, logs the CPU cycle of each marker write
// (F00000 A, F00002 B, F00004 end), optional level-1 interrupt raised IRQ_AT cycles after the Nth marker A
// (autovectored, VPA), and writes the first megabyte of memory at the end to MEMORY.
//   fxbench IMAGE MEMORY [CASE CYCLES]
#include "Vfx68k.h"
#include "verilated.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::assertOn(false);
    const char *imagePath = argc > 1 ? argv[1] : "image.bin";
    const char *memOut = argc > 2 ? argv[2] : "fx.mem";
    long irqCase = argc > 3 ? atol(argv[3]) : -1;   // marker A number
    long irqAt = argc > 4 ? atol(argv[4]) : 0;      // cycles after that marker's write began
    std::vector<uint8_t> mem(1 << 24);
    FILE *f = fopen(imagePath, "rb");
    size_t n = fread(mem.data(), 1, mem.size(), f);
    fclose(f);
    (void)n;
    Vfx68k *t = new Vfx68k;
    t->HALTn = 1; t->BERRn = 1; t->BRn = 1; t->BGACKn = 1; t->DTACKn = 1; t->VPAn = 1;
    t->IPL0n = 1; t->IPL1n = 1; t->IPL2n = 1; t->extReset = 1; t->pwrUp = 1;
    t->enPhi1 = 0; t->enPhi2 = 0; t->clk = 0; t->eval();
    uint64_t cyc = 0; bool asBefore = true; long markersA = -1; uint64_t irqCycle = ~0ull;
    bool done = false;
    for (uint64_t step = 0; step < 400000000ull && !done; step++) {
        for (int ph = 0; ph < 2; ph++) {
            t->enPhi1 = ph == 0; t->enPhi2 = ph == 1;
            t->clk = 1; t->eval();
            // bus: combinational answer
            const uint32_t addr = (uint32_t)t->eab << 1;
            const bool as = !t->ASn;
            const unsigned fc = t->FC0 | (t->FC1 << 1) | (t->FC2 << 2);
            if (as && !asBefore) {
                if (!t->eRWn && (addr & 0xFFFFF0) == 0xF00000) {
                    if (addr == 0xF00000) {
                        markersA++;
                        printf("A %llu\n", (unsigned long long)cyc);
                        if (markersA == irqCase) irqCycle = cyc + irqAt;
                    } else if (addr == 0xF00002) {
                        printf("B %llu\n", (unsigned long long)cyc);
                    } else {
                        done = true;
                    }
                }
                if (fc == 7) printf("I %llu\n", (unsigned long long)cyc);
            }
            asBefore = as;
            if (as && fc == 7) { t->VPAn = 0; t->DTACKn = 1; }
            else if (as) { t->DTACKn = 0; t->VPAn = 1; }
            else { t->DTACKn = 1; t->VPAn = 1; }
            if (as && t->eRWn) t->iEdb = (mem[addr & 0xFFFFFE] << 8) | mem[(addr & 0xFFFFFE) + 1];
            if (as && !t->eRWn) {
                if (!t->UDSn) mem[addr & 0xFFFFFE] = t->oEdb >> 8;
                if (!t->LDSn) mem[(addr & 0xFFFFFE) + 1] = t->oEdb & 0xFF;
            }
            const bool irq = cyc >= irqCycle && irqCycle != ~0ull;
            // (the line held until the acknowledge)
            t->IPL0n = irq ? 0 : 1;
            t->clk = 0; t->eval();
            if (as && fc == 7) irqCycle = ~0ull;
            if (ph == 1) cyc++;
            if (step == 20) { t->extReset = 0; t->pwrUp = 0; }
        }
    }
    FILE *o = fopen(memOut, "wb");
    fwrite(mem.data(), 1, 1 << 20, o);
    fclose(o);
    printf("end %llu\n", (unsigned long long)cyc);
    delete t;
}
