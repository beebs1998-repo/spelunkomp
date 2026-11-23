// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The ARM7TDMI interpreter's self-test (machine/arm7.h): short programs of each kind of instruction the IGS027A's
// code uses, in both instruction sets, run from memory, with the registers, the flags and (for some) the time they
// must end with. What is expected was worked out from ARM's description of the instructions, not by running them.
// The words are the assembly in each case's comment, put through clang's assembler (-target armv4t-none-eabi); a
// Thumb case begins with two ARM instructions that go over to Thumb (add r12, pc, #1; bx r12), and every case ends
// at a branch to itself.
//   ninja -C build/<game> arm7-test && build/<game>/arm7-test      (tools/check_arm.sh runs it)
#include "../machine/arm7.h"

#include <cstdio>
#include <utility>
#include <vector>

using namespace spelunk;

namespace {

// 64 KB of memory at 0, low byte first
struct Memory : Arm7Bus {
    std::vector<uint8_t> bytes = std::vector<uint8_t>(0x10000, 0);
    uint32_t ArmRead32(uint32_t a) override {
        return ArmRead16(a) | static_cast<uint32_t>(ArmRead16(a + 2)) << 16;
    }
    uint16_t ArmRead16(uint32_t a) override {
        return static_cast<uint16_t>(ArmRead8(a) | ArmRead8(a + 1) << 8);
    }
    uint8_t ArmRead8(uint32_t a) override {
        return bytes[a & 0xFFFF];
    }
    void ArmWrite32(uint32_t a, uint32_t v) override {
        ArmWrite16(a, static_cast<uint16_t>(v));
        ArmWrite16(a + 2, static_cast<uint16_t>(v >> 16));
    }
    void ArmWrite16(uint32_t a, uint16_t v) override {
        ArmWrite8(a, static_cast<uint8_t>(v));
        ArmWrite8(a + 1, static_cast<uint8_t>(v >> 8));
    }
    void ArmWrite8(uint32_t a, uint8_t v) override {
        bytes[a & 0xFFFF] = v;
    }
};

struct Case {
    const char *name;
    bool thumb;
    std::vector<uint32_t> words;
    std::vector<std::pair<int, uint32_t>> before, after; // registers set, and expected
    uint32_t flags;                                      // N Z C V expected, as the status register's top four bits
    int cycles;                                          // expected (-1: not looked at)
};

const Case kCases[] = {
    // adds r2, r0, r1
    {"ADDS: carry out and zero", false,
     {0xE0902001, 0xEAFFFFFE},
     {{0, 0xFFFFFFFF}, {1, 0x1}},
     {{2, 0x0}}, 0x6, -1},
    // adds r2, r0, r1
    {"ADDS: overflow", false,
     {0xE0902001, 0xEAFFFFFE},
     {{0, 0x7FFFFFFF}, {1, 0x1}},
     {{2, 0x80000000}}, 0x9, -1},
    // subs r2, r0, r1
    {"SUBS: no borrow sets carry", false,
     {0xE0502001, 0xEAFFFFFE},
     {{0, 0x5}, {1, 0x3}},
     {{2, 0x2}}, 0x2, -1},
    // subs r2, r0, r1
    {"SUBS: borrow clears carry", false,
     {0xE0502001, 0xEAFFFFFE},
     {{0, 0x3}, {1, 0x5}},
     {{2, 0xFFFFFFFE}}, 0x8, -1},
    // cmp r0, r1
    {"CMP: equal", false,
     {0xE1500001, 0xEAFFFFFE},
     {{0, 0x7}, {1, 0x7}},
     {}, 0x6, -1},
    // rsbs r2, r0, #0
    {"RSBS", false,
     {0xE2702000, 0xEAFFFFFE},
     {{0, 0x1}},
     {{2, 0xFFFFFFFF}}, 0x8, -1},
    // adds r2, r0, #1; adc r3, r1, #5
    {"ADC takes the carry", false,
     {0xE2902001, 0xE2A13005, 0xEAFFFFFE},
     {{0, 0xFFFFFFFF}, {1, 0x0}},
     {{2, 0x0}, {3, 0x6}}, 0x6, -1},
    // subs r2, r0, #1; sbc r3, r0, #0
    {"SBC takes the borrow", false,
     {0xE2502001, 0xE2C03000, 0xEAFFFFFE},
     {{0, 0x0}},
     {{2, 0xFFFFFFFF}, {3, 0xFFFFFFFF}}, 0x8, -1},
    // subs r2, r0, #1; rsc r3, r0, #10
    {"RSC", false,
     {0xE2502001, 0xE2E0300A, 0xEAFFFFFE},
     {{0, 0x1}},
     {{2, 0x0}, {3, 0x9}}, 0x6, -1},
    // and r2, r0, r1; eor r3, r0, r1; orr r4, r0, r1; bic r5, r0, r1; mvn r6, r0
    {"AND, EOR, ORR, BIC, MVN", false,
     {0xE0002001, 0xE0203001, 0xE1804001, 0xE1C05001, 0xE1E06000, 0xEAFFFFFE},
     {{0, 0xF0F0F0F0}, {1, 0xFF00FF0}},
     {{2, 0xF000F0}, {3, 0xFF00FF00}, {4, 0xFFF0FFF0}, {5, 0xF000F000}, {6, 0xF0F0F0F}}, 0x0, -1},
    // movs r0, #0x80000000
    {"MOVS of a rotated constant sets the carry", false,
     {0xE3B00102, 0xEAFFFFFE},
     {},
     {{0, 0x80000000}}, 0xA, -1},
    // movs r1, r0, lsl #1; mov r2, r0, lsr #4; mov r3, r0, asr #4; mov r4, r0, ror #4; mov r5, r0, rrx
    {"Shifts by a constant", false,
     {0xE1B01080, 0xE1A02220, 0xE1A03240, 0xE1A04260, 0xE1A05060, 0xEAFFFFFE},
     {{0, 0x80000003}},
     {{1, 0x6}, {2, 0x8000000}, {3, 0xF8000000}, {4, 0x38000000}, {5, 0xC0000001}}, 0x2, -1},
    // movs r1, r0, lsr #32; mov r2, r0, asr #32
    {"LSR #32 and ASR #32", false,
     {0xE1B01020, 0xE1A02040, 0xEAFFFFFE},
     {{0, 0x80000000}},
     {{1, 0x0}, {2, 0xFFFFFFFF}}, 0x6, -1},
    // movs r5, r0, lsl r1; movs r6, r0, lsl r2; mov r7, r0, lsr r4; mov r8, r0, asr r2; mov r9, r0, ror r4; mov r10, r0, lsl r3
    {"Shifts by a register", false,
     {0xE1B05110, 0xE1B06210, 0xE1A07430, 0xE1A08250, 0xE1A09470, 0xE1A0A310, 0xEAFFFFFE},
     {{0, 0x80000001}, {1, 0x20}, {2, 0x21}, {3, 0x0}, {4, 0x4}},
     {{5, 0x0}, {6, 0x0}, {7, 0x8000000}, {8, 0xFFFFFFFF}, {9, 0x18000000}, {10, 0x80000001}}, 0x4, 12},
    // mul r3, r0, r1; mla r4, r0, r1, r2
    {"MUL, MLA", false,
     {0xE0030190, 0xE0242190, 0xEAFFFFFE},
     {{0, 0x7}, {1, 0x6}, {2, 0x64}},
     {{3, 0x2A}, {4, 0x8E}}, 0x0, 5},
    // umull r2, r3, r0, r1; smull r4, r5, r0, r1; umlal r6, r7, r0, r1; smlal r8, r9, r0, r1
    {"UMULL, SMULL, UMLAL, SMLAL", false,
     {0xE0832190, 0xE0C54190, 0xE0A76190, 0xE0E98190, 0xEAFFFFFE},
     {{0, 0xFFFFFFFF}, {1, 0x2}, {6, 0x1}, {7, 0x0}, {8, 0x5}, {9, 0x0}},
     {{2, 0xFFFFFFFE}, {3, 0x1}, {4, 0xFFFFFFFE}, {5, 0xFFFFFFFF}, {6, 0xFFFFFFFF}, {7, 0x1}, {8, 0x3}, {9, 0x0}}, 0x0, -1},
    // str r1, [r0]; ldrb r2, [r0, #1]; ldrh r3, [r0, #2]; ldr r4, [r0], #4; str r1, [r0, #4]!; ldr r5, [r0]
    {"LDR, STR, LDRB, LDRH, before and after the index", false,
     {0xE5801000, 0xE5D02001, 0xE1D030B2, 0xE4904004, 0xE5A01004, 0xE5905000, 0xEAFFFFFE},
     {{0, 0x4000}, {1, 0x11223344}},
     {{0, 0x4008}, {2, 0x33}, {3, 0x1122}, {4, 0x11223344}, {5, 0x11223344}}, 0x0, 16},
    // strh r1, [r0]; ldrsb r2, [r0]; ldrsh r3, [r0]; ldrsb r4, [r0, #1]; strb r1, [r0, #3]; ldr r5, [r0]
    {"LDRSB, LDRSH, STRH, STRB", false,
     {0xE1C010B0, 0xE1D020D0, 0xE1D030F0, 0xE1D040D1, 0xE5C01003, 0xE5905000, 0xEAFFFFFE},
     {{0, 0x4000}, {1, 0x8081}},
     {{2, 0xFFFFFF81}, {3, 0xFFFF8081}, {4, 0xFFFFFF80}, {5, 0x81008081}}, 0x0, -1},
    // str r1, [r0]; ldr r2, [r0, #1]
    {"LDR from an address that is not a whole one", false,
     {0xE5801000, 0xE5902001, 0xEAFFFFFE},
     {{0, 0x4000}, {1, 0x11223344}},
     {{2, 0x44112233}}, 0x0, -1},
    // str r2, [r0, r1, lsl #2]; ldr r3, [r0, #12]; ldr r5, [r4, #-4]
    {"A shifted register as the offset", false,
     {0xE7802101, 0xE590300C, 0xE5145004, 0xEAFFFFFE},
     {{0, 0x4000}, {1, 0x3}, {2, 0xAABBCCDD}, {4, 0x4010}},
     {{3, 0xAABBCCDD}, {5, 0xAABBCCDD}}, 0x0, -1},
    // stmdb sp!, {r0, r1, r2}; ldmia sp!, {r3, r4, r5}; stmib r6, {r0, r1}; ldmda r7, {r8, r9}
    {"STMDB, LDMIA, STMIB, LDMDA", false,
     {0xE92D0007, 0xE8BD0038, 0xE9860003, 0xE8170300, 0xEAFFFFFE},
     {{0, 0x1}, {1, 0x2}, {2, 0x3}, {6, 0x4100}, {7, 0x4108}, {13, 0x5000}},
     {{3, 0x1}, {4, 0x2}, {5, 0x3}, {8, 0x1}, {9, 0x2}, {13, 0x5000}}, 0x0, 16},
    // bl 1f; mov r1, #1; b 2f; 1: mov r0, #5; mov pc, lr; 2:
    {"BL and the return", false,
     {0xEB000001, 0xE3A01001, 0xEA000001, 0xE3A00005, 0xE1A0F00E, 0xEAFFFFFE},
     {},
     {{0, 0x5}, {1, 0x1}}, 0x0, 11},
    // cmp r0, #1; moveq r1, #10; movne r1, #20; movhi r2, #1; movls r2, #2; movge r3, #1; movlt r3, #2
    {"Conditions", false,
     {0xE3500001, 0x03A0100A, 0x13A01014, 0x83A02001, 0x93A02002, 0xA3A03001, 0xB3A03002, 0xEAFFFFFE},
     {{0, 0x1}},
     {{1, 0xA}, {2, 0x2}, {3, 0x1}}, 0x6, 7},
    // mov sp, #0x100; mrs r0, cpsr; msr cpsr_c, #0xD2; mov sp, #0x200; msr cpsr_c, #0xD3; mov r1, sp
    {"MRS, MSR: each mode its own stack pointer", false,
     {0xE3A0DC01, 0xE10F0000, 0xE321F0D2, 0xE3A0DC02, 0xE321F0D3, 0xE1A0100D, 0xEAFFFFFE},
     {},
     {{0, 0xD3}, {1, 0x100}}, 0x0, -1},
    // mov r8, #1; msr cpsr_c, #0xD1; mov r8, #2; msr cpsr_c, #0xD3; mov r0, r8
    {"FIQ mode has its own r8", false,
     {0xE3A08001, 0xE321F0D1, 0xE3A08002, 0xE321F0D3, 0xE1A00008, 0xEAFFFFFE},
     {},
     {{0, 0x1}, {8, 0x1}}, 0x0, -1},
    // ldr r0, =0xE1B0F00E; mov r1, #8; str r0, [r1]; mov r2, #1; swi 0; mov r3, #7; b 9f; .ltorg; 9:
    {"SWI and MOVS PC, LR back", false,
     {0xE59F0014, 0xE3A01008, 0xE5810000, 0xE3A02001, 0xEF000000, 0xE3A03007, 0xEA000000, 0xE1B0F00E, 0xEAFFFFFE},
     {},
     {{2, 0x1}, {3, 0x7}}, 0x0, -1},
    // add r0, pc, #1; bx r0; .thumb; movs r1, #5; adds r1, #3; bx pc; nop; .arm; mov r2, #9
    {"BX to Thumb and back", false,
     {0xE28F0001, 0xE12FFF10, 0x31032105, 0x46C04778, 0xE3A02009, 0xEAFFFFFE},
     {},
     {{1, 0x8}, {2, 0x9}}, 0x0, -1},
    // movs r0, #200; adds r0, #100; subs r1, r0, #7; cmp r1, #255
    {"MOV, ADD, SUB, CMP with constants", true,
     {0xE28FC001, 0xE12FFF1C, 0x306420C8, 0x29FF1FC1, 0x0000E7FE},
     {},
     {{0, 0x12C}, {1, 0x125}}, 0x2, 4},
    // lsls r1, r0, #1; lsrs r2, r0, #31; asrs r3, r0, #31
    {"Shifts by a constant", true,
     {0xE28FC001, 0xE12FFF1C, 0x0FC20041, 0xE7FE17C3},
     {{0, 0x80000001}},
     {{1, 0x2}, {2, 0x1}, {3, 0xFFFFFFFF}}, 0x8, -1},
    // movs r3, r0; orrs r3, r1; movs r4, r3; ands r4, r1; movs r5, r3; eors r5, r0; movs r6, r3; bics r6, r0; mvns r7, r0
    {"ORR, AND, EOR, BIC, MVN", true,
     {0xE28FC001, 0xE12FFF1C, 0x430B0003, 0x400C001C, 0x4045001D, 0x4386001E, 0xE7FE43C7},
     {{0, 0xF}, {1, 0xF0}},
     {{3, 0xFF}, {4, 0xF0}, {5, 0xF0}, {6, 0xF0}, {7, 0xFFFFFFF0}}, 0x8, -1},
    // lsls r0, r1; asrs r2, r1; rors r3, r1; negs r4, r1; muls r5, r1, r5
    {"Shifts by a register, NEG, MUL", true,
     {0xE28FC001, 0xE12FFF1C, 0x410A4088, 0x424C41CB, 0xE7FE434D},
     {{0, 0x10}, {1, 0x4}, {2, 0x80000000}, {3, 0xF}, {5, 0x6}},
     {{0, 0x100}, {2, 0xF8000000}, {3, 0xF0000000}, {4, 0xFFFFFFFC}, {5, 0x18}}, 0x0, 9},
    // adds r4, r0, r1; adcs r2, r3; cmn r0, r1; sbcs r3, r1; tst r0, r1
    {"ADC, SBC, CMN, TST", true,
     {0xE28FC001, 0xE12FFF1C, 0x415A1844, 0x418B42C8, 0xE7FE4208},
     {{0, 0xFFFFFFFF}, {1, 0x1}, {2, 0x0}, {3, 0x5}},
     {{4, 0x0}, {2, 0x6}, {3, 0x4}}, 0x2, -1},
    // mov r8, r0; add r8, r0; mov r1, r8; cmp r1, r8
    {"The high registers", true,
     {0xE28FC001, 0xE12FFF1C, 0x44804680, 0x45414641, 0x0000E7FE},
     {{0, 0x5}},
     {{8, 0xA}, {1, 0xA}}, 0x6, -1},
    // str r1, [r0]; str r1, [r0, r2]; ldr r3, [r0, #8]; ldrb r4, [r0, #3]; ldrh r5, [r0, #2]; strb r2, [r0, #1]; strh r2, [r0, #4]; ldr r6, [r0]; ldr r7, [r0, #4]
    {"Loads and stores", true,
     {0xE28FC001, 0xE12FFF1C, 0x50816001, 0x78C46883, 0x70428845, 0x68068082, 0xE7FE6847},
     {{0, 0x4000}, {1, 0xCAFEBABE}, {2, 0x8}},
     {{3, 0xCAFEBABE}, {4, 0xCA}, {5, 0xCAFE}, {6, 0xCAFE08BE}, {7, 0x8}}, 0x0, 23},
    // strh r1, [r0, r2]; ldrsb r3, [r0, r2]; ldrsh r4, [r0, r2]; ldrb r5, [r0, r2]; ldrh r6, [r0, r2]
    {"Signed loads", true,
     {0xE28FC001, 0xE12FFF1C, 0x56835281, 0x5C855E84, 0xE7FE5A86},
     {{0, 0x4000}, {1, 0x8091}, {2, 0x0}},
     {{3, 0xFFFFFF91}, {4, 0xFFFF8091}, {5, 0x91}, {6, 0x8091}}, 0x0, -1},
    // push {r0, r1}; sub sp, #8; str r0, [sp, #4]; ldr r2, [sp, #4]; add r3, sp, #4; add sp, #8; pop {r4, r5}
    {"PUSH, POP and the stack pointer", true,
     {0xE28FC001, 0xE12FFF1C, 0xB082B403, 0x9A019001, 0xB002AB01, 0xE7FEBC30},
     {{0, 0x1}, {1, 0x2}, {13, 0x5000}},
     {{2, 0x1}, {3, 0x4FF4}, {4, 0x1}, {5, 0x2}, {13, 0x5000}}, 0x0, 15},
    // bl 1f; movs r1, #1; b 2f; 1: push {lr}; movs r0, #5; pop {pc}; 2:
    {"BL, PUSH {LR} and POP {PC}", true,
     {0xE28FC001, 0xE12FFF1C, 0xF802F000, 0xE0022101, 0x2005B500, 0xE7FEBD00},
     {{13, 0x5000}},
     {{0, 0x5}, {1, 0x1}, {13, 0x5000}}, 0x0, 16},
    // ldr r0, =0x12345678; b 9f; .ltorg; 9:
    {"LDR of a constant after the code", true,
     {0xE28FC001, 0xE12FFF1C, 0xE0014800, 0x12345678, 0x0000E7FE},
     {},
     {{0, 0x12345678}}, 0x0, 6},
    // movs r1, #0; 1: adds r1, #2; subs r0, #1; bne 1b
    {"A loop on BNE", true,
     {0xE28FC001, 0xE12FFF1C, 0x31022100, 0xD1FC3801, 0x0000E7FE},
     {{0, 0x3}},
     {{0, 0x0}, {1, 0x6}}, 0x6, 14},
    // stmia r0!, {r1, r2}; subs r0, #8; ldmia r0!, {r3, r4}
    {"STMIA, LDMIA", true,
     {0xE28FC001, 0xE12FFF1C, 0x3808C006, 0xE7FEC818},
     {{0, 0x4000}, {1, 0x7}, {2, 0x9}},
     {{0, 0x4008}, {3, 0x7}, {4, 0x9}}, 0x2, 8},
    // cmp r0, r1; blt 1f; movs r2, #9; 1: bhi 2f; movs r3, #9; 2:
    {"Signed and unsigned compares", true,
     {0xE28FC001, 0xE12FFF1C, 0xDB004288, 0xD8002209, 0xE7FE2309},
     {{0, 0xFFFFFFFF}, {1, 0x1}, {2, 0x0}, {3, 0x0}},
     {{2, 0x0}, {3, 0x0}}, 0xA, -1},

};

constexpr uint32_t kCodeAt = 0x100;

// The case's program put at kCodeAt and run to its last instruction (the branch to itself)
bool RunToEnd(Arm7 &cpu, Memory &memory, const Case &c, uint64_t &took) {
    cpu.Reset();
    cpu.cycles = 0;
    for (size_t n = 0; n < c.words.size(); n++) {
        memory.ArmWrite32(kCodeAt + static_cast<uint32_t>(n) * 4, c.words[n]);
    }
    // (reset leaves the PC at 0: a branch there to the code)
    memory.ArmWrite32(0, 0xEA000000 | ((kCodeAt - 8) >> 2));
    cpu.Step();
    if (c.thumb) {
        cpu.Step();
        cpu.Step();
    }
    for (const auto &[reg, value] : c.before) {
        cpu.r[reg] = value;
    }
    const uint64_t began = cpu.cycles;
    for (int steps = 0; steps < 1000; steps++) {
        const uint32_t pc = cpu.Pc();
        if (cpu.Thumb() ? memory.ArmRead16(pc) == 0xE7FE : memory.ArmRead32(pc) == 0xEAFFFFFE) {
            took = cpu.cycles - began;
            return true;
        }
        cpu.Step();
    }
    return false;
}

} // namespace

int main() {
    int failed = 0, cases = 0;
    for (const Case &c : kCases) {
        Memory memory;
        Arm7 cpu(memory);
        uint64_t took = 0;
        cases++;
        bool ok = RunToEnd(cpu, memory, c, took);
        if (!ok) {
            printf("FAIL %s: did not come to its end (pc %08X)\n", c.name, cpu.Pc());
        }
        for (const auto &[reg, value] : c.after) {
            if (cpu.r[reg] != value) {
                printf("FAIL %s: r%d is %08X, not %08X\n", c.name, reg, cpu.r[reg], value);
                ok = false;
            }
        }
        if ((cpu.Cpsr() >> 28) != c.flags) {
            printf("FAIL %s: the flags are %X, not %X\n", c.name, cpu.Cpsr() >> 28, c.flags);
            ok = false;
        }
        if (c.cycles >= 0 && took != static_cast<uint64_t>(c.cycles)) {
            printf("FAIL %s: took %llu cycles, not %d\n", c.name, static_cast<unsigned long long>(took), c.cycles);
            ok = false;
        }
        failed += !ok;
    }
    // The fast interrupt: taken before the next instruction, into FIQ mode with both interrupts off and the way back
    // in its link register; SUBS PC, LR, #4 goes back to the instruction it came before, the status as it was
    {
        cases++;
        Memory memory;
        Arm7 cpu(memory);
        cpu.Reset();
        memory.ArmWrite32(0x00, 0xEA00003E);  // b 0x100
        memory.ArmWrite32(0x1C, 0xE25EF004);  // subs pc, lr, #4
        memory.ArmWrite32(0x100, 0xE321F013); // msr cpsr_c, #0x13 (supervisor, interrupts on)
        memory.ArmWrite32(0x104, 0xE3A00001); // mov r0, #1
        memory.ArmWrite32(0x108, 0xE3A01002); // mov r1, #2
        cpu.Step();
        cpu.Step();
        cpu.Step();
        cpu.SetFiq(true);
        cpu.Step();
        bool ok = cpu.CurrentMode() == Arm7::kFiq && cpu.Pc() == 0x1C && cpu.r[14] == 0x10C && (cpu.Cpsr() & 0xC0) == 0xC0;
        cpu.SetFiq(false);
        cpu.Step();
        ok = ok && cpu.CurrentMode() == Arm7::kSupervisor && cpu.Pc() == 0x108 && (cpu.Cpsr() & 0xC0) == 0;
        cpu.Step();
        ok = ok && cpu.r[0] == 1 && cpu.r[1] == 2;
        if (!ok) {
            printf("FAIL the fast interrupt and the return from it (mode %02X, pc %08X)\n", cpu.CurrentMode(), cpu.Pc());
            failed++;
        }
    }
    // A loop that waits: Run counts its rounds without running them, and ends exactly where running them ends
    {
        cases++;
        uint64_t steps[2] = {}, cycles[2] = {};
        uint32_t pcs[2] = {};
        for (int plain = 0; plain < 2; plain++) {
            Memory memory;
            Arm7 cpu(memory);
            cpu.Reset();
            cpu.skipWaits = !plain;
            memory.ArmWrite32(0x00, 0xE5910000); // 1: ldr r0, [r1]
            memory.ArmWrite32(0x04, 0xE3500000); //    cmp r0, #0
            memory.ArmWrite32(0x08, 0x0AFFFFFC); //    beq 1b
            cpu.r[1] = 0x4000;
            cpu.Run(100003);
            steps[plain] = cpu.steps;
            cycles[plain] = cpu.cycles;
            pcs[plain] = cpu.Pc();
            if (!plain && cpu.skipped == 0) {
                printf("FAIL the waiting loop was not seen\n");
                failed++;
            }
        }
        if (steps[0] != steps[1] || cycles[0] != cycles[1] || pcs[0] != pcs[1]) {
            printf("FAIL a waiting loop counted ends at cycle %llu pc %08X, run at cycle %llu pc %08X\n", static_cast<unsigned long long>(cycles[0]), pcs[0],
                   static_cast<unsigned long long>(cycles[1]), pcs[1]);
            failed++;
        }
    }
    printf("arm7-test: %d of %d cases failed\n", failed, cases);
    return failed ? 1 : 0;
}
