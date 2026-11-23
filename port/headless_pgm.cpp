// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// IGS's PGM board with no window: runs a game for a number of frames with scripted input, and writes screenshots,
// RAM dumps, per-frame hashes and the places its code reached (the checks and the translator's input). (The same
// options as port/headless68k.cpp, Cave's 68000 board's.)
//
//   --game games/<id>/game.cfg   --roms FOLDER   --frames N
//   --cpu interpreter|blocks|native|check   how the game's code is run (native: the translations linked in; check: a
//                                      build with -DCHECK68K=ON, each translated block against the interpreter)
//   --shot FRAME:file.png              the picture after that frame, stood up as the player sees it (--raw-shots: as
//                                      the board draws it, 448 x 224)
//   --dump-ram FRAME:file              work RAM after that frame
//   --ram-log FILE                     work RAM (128 KB, as the 68000 sees it) after every frame, one after another,
//                                      as the bus had it at the blank: the format of the FPGA core's harness
//                                      (docs/ACCURACY.md), to compare frame by frame
//   --start-cycle N                    (measurement) the 68000's first instruction N cycles into frame 0 (its reset
//                                      let go then: where a reference's is), the E clock's phase as it would be
//   --dump-program FILE                the program as the CPU sees it (what tools/m68k/recomp.py translates)
//   --press FRAME:COUNT:BITS[:PLAYER]  a player's inputs (machine/inputs.h's kIn bits) held for COUNT frames
//   --system FRAME:COUNT:BITS          the coin, start and service switches (kSys bits)
//   --auto SEED[:FROM]                 made-up play from frame FROM (400): coins, starts, the stick and buttons
//   --poke FROM:ADDRESS:VALUE[:BYTES]  a value written into work RAM before every frame from FROM on; BYTES 1, 2 or
//                                      4 (2 by default). An address from D00000: the RAM shared with the ARM
//   (env SPELUNK_TRACE_AT=ADDRESS,...: a line at each, with the registers: see below)
//   --poke-at FRAME:ADDRESS:VALUE[:BYTES]  the same, written before that frame only
//   --hash-log FILE                    a line per frame: the frame and a hash of what the game can see
//   --timing                           the board's waits on the CPU (the feature "Slowdown as the board")
//   --frame-log FILE                   a line a frame: the frame, 1 if the game had a new picture, the idle loop's
//                                      spins (--spin), the board's waits (PGM)
//   --arm-steps N                      (a game with the ARM run) the ARM brought up to the 68000's time N times a
//                                      frame instead of the board's 64 (tools/check_arm.sh: the game must not care)
//   --arm-plain                        every round of the ARM's waiting loops run, none counted (machine/arm7.h)
//   --ram-hash-log FILE                a line per frame: the frame and a hash of work RAM alone (the game's own
//                                      memory: what two runs that differ only in what stands beside it must share)
//   --spin ADDRESS                     (hex) the instruction of the game's idle loop counted (--cpu blocks or
//                                      interpreter: translated code is not counted)
//   --record-entries FILE              the places the code reached by computed jumps, added to FILE
//   --wav FILE                         the sound, as the chip makes it
//   --dump-z80 FRAME:file              the Z80's RAM after that frame
//   --dump-arm FRAME:file              (a game with the ARM run) after that frame: the RAM the 68000 and the ARM share
//                                      (64 KB, the 68000's D00000; low byte first, as the ARM has it), then the ARM's
//                                      own (64 KB, its 18000000; 1 KB, its 10000000)
//   --z80 interpreter|native           how the Z80 is run (native, the default: the driver's translated code while
//                                      its RAM holds what that was made from, tools/translatez80.sh)
//   --record-z80-entries FILE          the places the Z80's code reached by computed jumps and interrupt vectors,
//                                      added to FILE
//   --z80-entries-from FRAME           noted only from that frame on (a game whose board runs another driver first:
//                                      the PGM's BIOS has one of its own; game.cfg's z80_driver_frame)
//   --save-state FRAME:file   --load-state file
//   --features NAME[=N],...            the game's features (games/<id>/src: the training features) as the game
//                                      program's --features has them; off unless named (free play too)
//   --boxes FRAME:file                 the hit boxes (the feature "Hit boxes") after that frame, a line each
//   SPELUNK_WATCH=lo-hi                (interpreter) who writes to those addresses
//   SPELUNK_FRAME_DUMP=FILE[:DIV]      (measurement) every frame's picture, small and grey (port/frame_dump.h:
//                                      lining the port up against a board's video, tools/boardvideo)
//   SPELUNK_CYCLE_TRACE=FROM-TO:file   (--cpu interpreter) each instruction started in those frames: its address
//                                      and the clock then (measurement, against a reference's bus log)
//   --link REQUESTS:RESPONSES          (measurement: a test ROM in place of the BIOS, as the PGM core's testroms/ have
//                                      them) talks to the ROM's debug link through its RAM ring (the core's simulator
//                                      transport: the block marked "RFIF" in work RAM). Each line of REQUESTS (hex) is
//                                      sent when the last one's answer (6 bytes of header, the last its length) is in,
//                                      each answer written to RESPONSES as a hex line; the run ends after the last
//   SPELUNK_TRACE_ARM=1                (a game with the ARM run: DoDonPachi II) every word put in either latch
#include "../machine/pgm.h"
#include "../machine/zip.h"
#include "../runtime/game.h"
#include "../runtime/native68k.h"
#include "../runtime/romset.h"
#include "../runtime/state.h"
#include "frame_dump.h"
#include "png_write.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>

using namespace spelunk;

namespace {
struct Held {
    uint64_t from, count;
    uint32_t bits;
    int player; // (-1: the system switches)
};

// The picture turned `degrees` clockwise (0, 90, 180 or 270)
std::vector<uint32_t> Turned(const uint32_t *in, int w, int h, int degrees, int &outW, int &outH) {
    outW = degrees % 180 ? h : w;
    outH = degrees % 180 ? w : h;
    std::vector<uint32_t> out(static_cast<size_t>(outW) * outH);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int ox = x, oy = y;
            switch (degrees) {
            case 90: ox = h - 1 - y; oy = x; break;
            case 180: ox = w - 1 - x; oy = h - 1 - y; break;
            case 270: ox = y; oy = w - 1 - x; break;
            default: break;
            }
            out[static_cast<size_t>(oy) * outW + ox] = in[static_cast<size_t>(y) * w + x];
        }
    }
    return out;
}
} // namespace

int main(int argc, char **argv) {
    std::string configPath, romFolder = "rom", cpuMode = "native", z80Mode = "native", z80EntriesPath, hashLog, entriesPath, loadState, wavPath, programPath;
    std::vector<int16_t> wav;
    uint64_t frames = 600, autoSeed = 0, autoFrom = 400, z80EntriesFrom = 0;
    bool autoPlay = false, rawShots = false;
    std::map<uint64_t, std::string> shots, dumps, z80Dumps, armDumps, saveStates, boxDumps;
    std::string featureList;
    std::string frameLog;
    bool timingOn = false;
    unsigned armSteps = 0;
    bool armPlain = false;
    std::string ramHashLog, ramLog;
    uint64_t startCycle = 0;
    uint32_t spinAt = 1;
    std::vector<Held> held;
    struct Poke {
        uint64_t from;
        uint32_t address, value, bytes;
        bool once = false;
    };
    std::vector<Poke> pokes;
    std::string linkRequests, linkResponses;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        auto frameAnd = [&](std::map<uint64_t, std::string> &into) {
            const std::string v = next();
            const size_t colon = v.find(':');
            into[strtoull(v.substr(0, colon).c_str(), nullptr, 0)] = v.substr(colon + 1);
        };
        if (a == "--poke" || a == "--poke-at") {
            const std::string v = next();
            Poke poke{0, 0, 0, 2, a == "--poke-at"};
            char *end = nullptr;
            poke.from = strtoull(v.c_str(), &end, 0);
            poke.address = static_cast<uint32_t>(strtoul(*end ? end + 1 : end, &end, 0));
            poke.value = static_cast<uint32_t>(strtoul(*end ? end + 1 : end, &end, 0));
            if (*end) {
                poke.bytes = static_cast<uint32_t>(strtoul(end + 1, nullptr, 0));
            }
            pokes.push_back(poke);
        } else if (a == "--game") {
            configPath = next();
        } else if (a == "--roms") {
            romFolder = next();
        } else if (a == "--frames") {
            frames = strtoull(next().c_str(), nullptr, 0);
        } else if (a == "--cpu") {
            cpuMode = next();
        } else if (a == "--shot") {
            frameAnd(shots);
        } else if (a == "--raw-shots") {
            rawShots = true;
        } else if (a == "--dump-ram") {
            frameAnd(dumps);
        } else if (a == "--features") {
            featureList = next();
        } else if (a == "--boxes") {
            frameAnd(boxDumps);
        } else if (a == "--dump-z80") {
            frameAnd(z80Dumps);
        } else if (a == "--dump-arm") {
            frameAnd(armDumps);
        } else if (a == "--dump-program") {
            programPath = next();
        } else if (a == "--save-state") {
            frameAnd(saveStates);
        } else if (a == "--load-state") {
            loadState = next();
        } else if (a == "--hash-log") {
            hashLog = next();
        } else if (a == "--timing") {
            timingOn = true;
        } else if (a == "--frame-log") {
            frameLog = next();
        } else if (a == "--arm-plain") {
            armPlain = true;
        } else if (a == "--arm-steps") {
            armSteps = static_cast<unsigned>(strtoul(next().c_str(), nullptr, 0));
        } else if (a == "--link") {
            const std::string v = next();
            const size_t colon = v.find(':');
            linkRequests = v.substr(0, colon);
            linkResponses = colon == std::string::npos ? std::string() : v.substr(colon + 1);
        } else if (a == "--ram-log") {
            ramLog = next();
        } else if (a == "--start-cycle") {
            startCycle = strtoull(next().c_str(), nullptr, 0);
        } else if (a == "--ram-hash-log") {
            ramHashLog = next();
        } else if (a == "--spin") {
            spinAt = static_cast<uint32_t>(strtoul(next().c_str(), nullptr, 16));
        } else if (a == "--wav") {
            wavPath = next();
        } else if (a == "--record-entries") {
            entriesPath = next();
        } else if (a == "--z80") {
            z80Mode = next();
        } else if (a == "--record-z80-entries") {
            z80EntriesPath = next();
        } else if (a == "--z80-entries-from") {
            z80EntriesFrom = strtoull(next().c_str(), nullptr, 0);
        } else if (a == "--auto") {
            const std::string v = next();
            autoPlay = true;
            autoSeed = strtoull(v.c_str(), nullptr, 0);
            if (const size_t colon = v.find(':'); colon != std::string::npos) {
                autoFrom = strtoull(v.c_str() + colon + 1, nullptr, 0);
            }
        } else if (a == "--press" || a == "--system") {
            const std::string v = next();
            Held h{0, 1, 0, a == "--system" ? -1 : 0};
            unsigned long long from = 0, count = 1;
            unsigned bits = 0;
            int player = 1;
            if (sscanf(v.c_str(), "%llu:%llu:%i:%d", &from, &count, &bits, &player) < 3) {
                fprintf(stderr, "%s wants FRAME:COUNT:BITS\n", a.c_str());
                return 2;
            }
            h.from = from;
            h.count = count;
            h.bits = bits;
            if (h.player >= 0) {
                h.player = player == 2 ? 1 : 0;
            }
            held.push_back(h);
        } else {
            fprintf(stderr, "unknown argument %s\n", a.c_str());
            return 2;
        }
    }

    GameConfig config;
    std::string error;
    if (configPath.empty()) {
        ParseGameConfig(BuiltInGameConfig(), config);
    } else if (!ReadGameConfig(configPath, config, error)) {
        fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    auto board = std::make_unique<Pgm>();
    if (!LoadSet(romFolder, config, *board, error)) {
        fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    if (!programPath.empty()) {
        if (FILE *f = fopen(programPath.c_str(), "wb")) {
            fwrite(board->program.data(), 1, board->program.size(), f);
            fclose(f);
        }
        return 0;
    }
    const int rotate = atoi(config["rotate"].c_str());
    board->mode = cpuMode == "interpreter" ? M68K::Mode::Interpreter : cpuMode == "blocks" ? M68K::Mode::Blocks : cpuMode == "check" ? M68K::Mode::Check
                                                                                                                  : M68K::Mode::Native;
    // The game's features, when asked for (none: the board alone, as the checks run it)
    std::vector<int> features(AllFeatures().size(), 0);
    const bool withGame = !featureList.empty() || !boxDumps.empty();
    for (size_t from = 0; !featureList.empty() && from <= featureList.size();) {
        const size_t comma = featureList.find(',', from);
        std::string name = featureList.substr(from, comma == std::string::npos ? std::string::npos : comma - from);
        int value = 1;
        if (const size_t equals = name.find('='); equals != std::string::npos) {
            value = atoi(name.c_str() + equals + 1);
            name.resize(equals);
        }
        if (const size_t at = FeatureIndex(name); at < features.size()) {
            features[at] = value;
        } else {
            fprintf(stderr, "no feature %s\n", name.c_str());
            return 2;
        }
        if (comma == std::string::npos) {
            break;
        }
        from = comma + 1;
    }
    if (withGame && ThisGame().start) {
        ThisGame().start(*board);
    }
    // SPELUNK_TRACE_AT=0CA0B8,... (with --cpu blocks: the places need not be ones the translated code stops by at): a
    // line on stdout each time the game's code comes to one of the places, with the frame, the registers, the eight
    // longs on top of the stack and the longs at SPELUNK_TRACE_MEM's places (after the game's own hooks: its start
    // clears the hooks)
    if (const char *places = getenv("SPELUNK_TRACE_AT")) {
        static Pgm *traced;
        traced = board.get();
        for (const char *at = places; *at;) {
            char *end = nullptr;
            AddHook(static_cast<uint32_t>(strtoul(at, &end, 16)), "trace", [](M68K &c) {
                printf("%llu %06X", static_cast<unsigned long long>(traced->frame), c.pc);
                for (uint32_t r : c.d) {
                    printf(" %08X", r);
                }
                for (uint32_t r : c.a) {
                    printf(" %08X", r);
                }
                for (uint32_t i = 0; i < 8; i++) {
                    printf(" %08X", native::R32(c, c.a[7] + i * 4));
                }
                if (const char *mem = getenv("SPELUNK_TRACE_MEM")) {
                    for (const char *m = mem; *m;) {
                        char *end = nullptr;
                        printf(" %08X", native::R32(c, static_cast<uint32_t>(strtoul(m, &end, 16))));
                        m = *end ? end + 1 : end;
                    }
                }
                printf("\n");
                return false;
            });
            at = *end ? end + 1 : end;
        }
    }
    board->cpu.recording = !entriesPath.empty();
    board->z80Mode = z80Mode == "interpreter" ? Z80::Mode::Interpreter : Z80::Mode::Native;
    board->z80.recording = !z80EntriesPath.empty();
    if (const char *watch = getenv("SPELUNK_WATCH")) {
        unsigned lo = 0, hi = 0;
        if (sscanf(watch, "%x-%x", &lo, &hi) == 2) {
            board->cpu.watchLow = lo;
            board->cpu.watchHigh = hi;
        }
    }
    board->cpu.keepHistory = getenv("SPELUNK_PC_HISTORY") != nullptr;
    board->cpu.spinAt = spinAt;
    board->cpu.noteWrites = !ramLog.empty();
    if (startCycle > 0 && loadState.empty()) {
        // (the 68000's reset let go later; its E clock as it was, running from power on)
        board->cpu.cycles += startCycle;
        board->cpu.eclockPhase = (board->cpu.eclockPhase + 10 - startCycle % 10) % 10;
    }
    board->SetBoardTiming(timingOn);
    board->armTrace = getenv("SPELUNK_TRACE_ARM") != nullptr;
    board->arm.skipWaits = !armPlain;
    if (armSteps) {
        board->armSteps = armSteps;
    }
    board->armTimed = board->hasArm;
    if (!loadState.empty()) {
        std::vector<uint8_t> data;
        if (!ReadWholeFile(loadState, data)) {
            fprintf(stderr, "cannot read %s\n", loadState.c_str());
            return 1;
        }
        Archive ar(data, false);
        board->Serialize(ar);
        if (!ar.Ok()) {
            fprintf(stderr, "%s is not a state of this game and build\n", loadState.c_str());
            return 1;
        }
    }

    FILE *hashes = hashLog.empty() ? nullptr : fopen(hashLog.c_str(), "w");
    FILE *frameOut = frameLog.empty() ? nullptr : fopen(frameLog.c_str(), "w");
    FILE *ramHashes = ramHashLog.empty() ? nullptr : fopen(ramHashLog.c_str(), "w");
    FILE *ramOut = ramLog.empty() ? nullptr : fopen(ramLog.c_str(), "wb");
    uint64_t picturesBefore = board->pictures, spinsBefore = 0, waitsBefore = 0;
    std::vector<uint32_t> picture(Igs023::kWidth * Igs023::kHeight);
    FrameDump frameDump; // (SPELUNK_FRAME_DUMP: every frame, small and grey; port/frame_dump.h)
    const auto began = std::chrono::steady_clock::now();
    const uint64_t first = board->frame;
    board->sound.trace = getenv("SPELUNK_TRACE_SOUND") != nullptr;
    uint64_t random = autoSeed * 0x9E3779B97F4A7C15ull + 0x1234567;
    uint32_t autoStick = 0, autoLeft = 0;
    // The test ROM's debug link (--link): its RAM ring, the requests still to send and the answer being read
    std::vector<std::vector<uint8_t>> linkToSend;
    std::vector<uint8_t> linkAnswer;
    FILE *linkOut = nullptr;
    bool linkWaiting = false;
    uint32_t linkAt = 0;
    if (!linkRequests.empty()) {
        if (FILE *in = fopen(linkRequests.c_str(), "r")) {
            char line[2048];
            while (fgets(line, sizeof line, in)) {
                std::vector<uint8_t> bytes;
                for (const char *c = line; c[0] && c[1] && isxdigit(static_cast<unsigned char>(c[0])); c += 2) {
                    bytes.push_back(static_cast<uint8_t>(strtoul(std::string(c, 2).c_str(), nullptr, 16)));
                }
                if (!bytes.empty()) {
                    linkToSend.push_back(bytes);
                }
            }
            fclose(in);
        }
        std::reverse(linkToSend.begin(), linkToSend.end());
        linkOut = fopen(linkResponses.c_str(), "w");
    }
    auto linkStep = [&]() {
        auto &ram = board->ram;
        auto word = [&](uint32_t at) { return static_cast<uint16_t>(ram[at] << 8 | ram[at + 1]); };
        auto setWord = [&](uint32_t at, uint16_t v) {
            ram[at] = static_cast<uint8_t>(v >> 8);
            ram[at + 1] = static_cast<uint8_t>(v);
        };
        if (!linkAt) {
            for (uint32_t at = 0; at + 4 <= ram.size(); at += 2) {
                if (ram[at] == 'R' && ram[at + 1] == 'F' && ram[at + 2] == 'I' && ram[at + 3] == 'F') {
                    linkAt = at;
                    ram[at + 4] = 1; // (sim_active)
                    break;
                }
            }
            if (!linkAt) {
                return;
            }
        }
        // (magic[4] sim_active target_ready in_head in_tail out_head out_tail in_buf[512] out_buf[512])
        const uint32_t inHead = linkAt + 6, outHead = linkAt + 10, outTail = linkAt + 12;
        const uint32_t inBuf = linkAt + 14, outBuf = inBuf + 512;
        // (the heads and tails run on through 16 bits; a byte's place is the low 9)
        for (uint16_t t = word(outTail); t != word(outHead); t++) {
            linkAnswer.push_back(ram[outBuf + (t & 511)]);
            setWord(outTail, static_cast<uint16_t>(t + 1));
        }
        if (linkWaiting && linkAnswer.size() >= 6 && linkAnswer.size() >= 6u + linkAnswer[5]) {
            for (size_t n = 0; n < 6u + linkAnswer[5]; n++) {
                fprintf(linkOut, "%02X", linkAnswer[n]);
            }
            fprintf(linkOut, "\n");
            fflush(linkOut);
            linkAnswer.erase(linkAnswer.begin(), linkAnswer.begin() + 6 + linkAnswer[5]);
            linkWaiting = false;
        }
        if (!linkWaiting && !linkToSend.empty()) {
            uint16_t h = word(inHead);
            for (uint8_t b : linkToSend.back()) {
                ram[inBuf + (h & 511)] = b;
                h++;
            }
            setWord(inHead, h);
            linkToSend.pop_back();
            linkWaiting = true;
        }
    };
    while (board->frame < frames) {
        const uint64_t f = board->frame;
        if (linkOut) {
            linkStep();
            if (!linkWaiting && linkToSend.empty()) {
                break;
            }
        }
        board->z80.recording = !z80EntriesPath.empty() && f >= z80EntriesFrom;
        board->inputs[0] = board->inputs[1] = board->system = 0;
        for (const Held &h : held) {
            if (f >= h.from && f < h.from + h.count) {
                (h.player < 0 ? board->system : board->inputs[h.player]) |= h.bits;
            }
        }
        if (autoPlay && f >= autoFrom) {
            // (a coin and a start now and then; the stick held a while each way; the shot mostly held, a bomb seldom)
            auto roll = [&]() {
                random ^= random << 13;
                random ^= random >> 7;
                random ^= random << 17;
                return random;
            };
            if (autoLeft-- == 0) {
                static constexpr uint32_t kWays[] = {0, kInUp, kInDown, kInLeft, kInRight, kInUp | kInLeft, kInUp | kInRight, kInDown | kInLeft, kInDown | kInRight};
                autoStick = kWays[roll() % 9];
                autoLeft = 5 + roll() % 40;
            }
            board->inputs[0] |= autoStick | (roll() % 8 ? kInButton1 : 0) | (roll() % 600 == 0 ? kInButton2 : 0) | (roll() % 5 == 0 ? kInButton3 : 0);
            const uint64_t phase = (f - autoFrom) % 240;
            board->system |= phase < 6 ? kSysCoin1 : 0;
            board->system |= phase >= 30 && phase < 34 ? kSysStart1 : 0;
        }
        for (const Poke &poke : pokes) {
            if (poke.once ? f == poke.from : f >= poke.from) {
                for (uint32_t i = 0; i < poke.bytes; i++) {
                    const uint8_t byte = static_cast<uint8_t>(poke.value >> ((poke.bytes - 1 - i) * 8));
                    if (board->hasArm && ((poke.address + i) & 0xFF0000) == 0xD00000) {
                        // (the RAM shared with the ARM, as the 68000 addresses it: D00000)
                        board->armShared[((poke.address + i) & 0xFFFF) ^ 1] = byte;
                    } else {
                        board->ram[(poke.address + i) & 0x1FFFF] = byte;
                    }
                }
            }
        }
        if (withGame) {
            // (a game run unseen to where a mode begins: the controls are the game's, as in the game program)
            if (const int bits = ThisGame().unseen ? ThisGame().unseen(*board, features) : -1; bits >= 0) {
                board->inputs[0] = static_cast<uint32_t>(bits);
                board->inputs[1] = board->system = 0;
            }
            if (ThisGame().beforeFrame) {
                ThisGame().beforeFrame(*board, features);
            }
        }
        static unsigned long long traceFrom = 0, traceTo = 0;
        static FILE *traceOut = [] {
            const char *v = getenv("SPELUNK_CYCLE_TRACE");
            char path[1024] = {};
            return v && sscanf(v, "%llu-%llu:%1023s", &traceFrom, &traceTo, path) == 3 ? fopen(path, "w") : nullptr;
        }();
        if (traceOut) {
            board->cpu.cycleTrace = f >= traceFrom && f <= traceTo ? traceOut : nullptr;
            board->cpu.traceWrites = getenv("SPELUNK_CYCLE_TRACE_WRITES") != nullptr;
            if (board->cpu.cycleTrace) {
                fprintf(traceOut, "frame %llu %llu\n", static_cast<unsigned long long>(f), static_cast<unsigned long long>(board->cpu.cycles));
            }
        }
        board->RunFrame();
        const uint64_t done = board->frame;
        if (withGame && ThisGame().afterFrame) {
            ThisGame().afterFrame(*board, features);
        }
        if (const auto it = boxDumps.find(done); it != boxDumps.end() && ThisGame().hitBoxes) {
            std::vector<HitBox> boxes;
            ThisGame().hitBoxes(*board, boxes);
            if (FILE *out = fopen(it->second.c_str(), "w")) {
                for (const HitBox &b : boxes) {
                    fprintf(out, "%d %.3f %.3f %.3f %.3f\n", static_cast<int>(b.kind), b.x0, b.y0, b.x1, b.y1);
                }
                fclose(out);
            }
        }
        if (!wavPath.empty()) {
            wav.insert(wav.end(), board->sound.out.begin(), board->sound.out.end());
        }
        board->sound.out.clear();
        if (frameOut) {
            // (a line a frame: the frame, 1 if the game had a new picture ready, the idle loop's spins, the board's waits)
            fprintf(frameOut, "%llu %u %llu %llu\n", static_cast<unsigned long long>(done), board->pictures != picturesBefore ? 1u : 0u,
                    static_cast<unsigned long long>(board->cpu.spins - spinsBefore), static_cast<unsigned long long>(board->cpu.waitsCounted - waitsBefore));
            picturesBefore = board->pictures;
            spinsBefore = board->cpu.spins;
            waitsBefore = board->cpu.waitsCounted;
        }
        if (ramOut) {
            // (as the bus had it at the blank: an instruction it fell inside has not made its late writes yet)
            const std::vector<uint8_t> ram = board->cpu.RamAt(done * Pgm::kFrameCycles);
            fwrite(ram.data(), 1, ram.size(), ramOut);
        }
        if (ramHashes) {
            uint64_t h = 0xCBF29CE484222325ull;
            for (uint8_t b : board->ram) {
                h = (h ^ b) * 0x100000001B3ull;
            }
            fprintf(ramHashes, "%llu %016llx\n", static_cast<unsigned long long>(done), static_cast<unsigned long long>(h));
        }
        if (hashes) {
            fprintf(hashes, "%llu %016llx\n", static_cast<unsigned long long>(done), static_cast<unsigned long long>(board->StateHash()));
        }
        if (frameDump.on()) {
            board->Picture(picture.data());
            int w = Igs023::kWidth, h = Igs023::kHeight;
            const std::vector<uint32_t> out = Turned(picture.data(), w, h, rotate, w, h);
            frameDump.Write(out.data(), w, h);
        }
        if (const auto it = shots.find(done); it != shots.end()) {
            board->Picture(picture.data());
            int w = Igs023::kWidth, h = Igs023::kHeight;
            const std::vector<uint32_t> out = Turned(picture.data(), w, h, rawShots ? 0 : rotate, w, h);
            WritePng(it->second.c_str(), out.data(), w, h);
        }
        if (const auto it = dumps.find(done); it != dumps.end()) {
            if (FILE *out = fopen(it->second.c_str(), "wb")) {
                fwrite(board->ram.data(), 1, board->ram.size(), out);
                fclose(out);
            }
        }
        if (const auto it = z80Dumps.find(done); it != z80Dumps.end()) {
            if (FILE *out = fopen(it->second.c_str(), "wb")) {
                fwrite(board->z80Ram.data(), 1, board->z80Ram.size(), out);
                fclose(out);
            }
        }
        if (const auto it = armDumps.find(done); it != armDumps.end() && board->hasArm) {
            if (FILE *out = fopen(it->second.c_str(), "wb")) {
                fwrite(board->armShared.data(), 1, board->armShared.size(), out);
                fwrite(board->armRam.data(), 1, board->armRam.size(), out);
                fwrite(board->armRamLow.data(), 1, board->armRamLow.size(), out);
                fclose(out);
            }
        }
        if (const auto it = saveStates.find(done); it != saveStates.end()) {
            std::vector<uint8_t> data;
            Archive ar(data, true);
            board->Serialize(ar);
            if (FILE *out = fopen(it->second.c_str(), "wb")) {
                fwrite(data.data(), 1, data.size(), out);
                fclose(out);
            }
        }
    }
    if (hashes) {
        fclose(hashes);
    }
    if (ramOut) {
        fclose(ramOut);
    }
    if (frameOut) {
        fclose(frameOut);
    }
    if (ramHashes) {
        fclose(ramHashes);
    }
    if (!wavPath.empty()) {
        if (FILE *f = fopen(wavPath.c_str(), "wb")) {
            const uint32_t rate = Ics2115::sampleRate, bytes = static_cast<uint32_t>(wav.size() * 2), riff = 36 + bytes, fmt = 16, byteRate = rate * 2;
            const uint16_t pcm = 1, channels = 1, align = 2, bits = 16;
            fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f);
            fwrite(&pcm, 2, 1, f); fwrite(&channels, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&byteRate, 4, 1, f);
            fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
            fwrite(wav.data(), 2, wav.size(), f);
            fclose(f);
        }
        long long peak = 0, nonzero = 0;
        for (int16_t v : wav) { peak = std::max<long long>(peak, v < 0 ? -v : v); nonzero += v != 0; }
        fprintf(stderr, "sound: %zu samples at %u Hz, peak %lld, %lld not silent\n", wav.size(), Ics2115::sampleRate, peak, nonzero);
    }
    fprintf(stderr, "z80: %s, pc %04X, %llu cycles; %llu writes to the ICS2115, %llu commands from the 68000\n", board->Z80Running() ? "running" : "held",
            board->z80.pc, static_cast<unsigned long long>(board->z80.cycles), static_cast<unsigned long long>(board->soundWrites),
            static_cast<unsigned long long>(board->latchWrites));
    {
        const uint64_t native = board->z80.nativeCycles, interpreted = board->z80.interpretedCycles;
        fprintf(stderr, "z80 code: %s; %llu cycles run translated, %llu interpreted (%.3f%% interpreted)%s\n",
                !HasZ80Code() ? "none translated" : board->z80.codeDiffers ? "translated, but not what the RAM holds now" : "translated, the RAM holds it",
                static_cast<unsigned long long>(native), static_cast<unsigned long long>(interpreted),
                native + interpreted ? 100.0 * interpreted / (native + interpreted) : 0.0, z80Mode == "interpreter" ? " (--z80 interpreter)" : "");
    }
    if (!z80EntriesPath.empty()) {
        std::set<uint32_t> entries(board->z80.newEntries.begin(), board->z80.newEntries.end());
        if (FILE *in = fopen(z80EntriesPath.c_str(), "r")) {
            char line[64];
            while (fgets(line, sizeof line, in)) {
                if (line[0] != '#' && line[0] != '\n') {
                    entries.insert(static_cast<uint32_t>(strtoul(line, nullptr, 16)));
                }
            }
            fclose(in);
        }
        if (FILE *out = fopen(z80EntriesPath.c_str(), "w")) {
            fprintf(out, "# Places the Z80's code was seen to reach in ways the translator cannot follow\n");
            for (uint32_t e : entries) {
                fprintf(out, "%04X\n", e);
            }
            fclose(out);
        }
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    const uint64_t ran = board->frame - first;
    if (board->hasArm) {
        // (the ARM beside the 68000: how much it ran and what share of the run's time on this machine that took)
        fprintf(stderr, "arm: %llu instructions in %llu cycles, %.1f%% of them waiting in a loop (counted, not run); %.2f s of the run's time (%.1f%%); pc %08X%s\n",
                static_cast<unsigned long long>(board->arm.steps), static_cast<unsigned long long>(board->arm.cycles),
                board->arm.cycles ? 100.0 * static_cast<double>(board->arm.skipped) / static_cast<double>(board->arm.cycles) : 0.0, board->armNanoseconds / 1e9,
                seconds > 0 ? board->armNanoseconds / 1e7 / seconds : 0.0, board->arm.Pc(), board->arm.Thumb() ? " (Thumb)" : "");
        fprintf(stderr, "arm: %llu writes of the 68000's came while the ARM was ahead of it\n", static_cast<unsigned long long>(board->armLateWrites));
    }
    fprintf(stderr, "%llu frames in %.2f s (%.0f fps); %u with a new sprite list; pc %06X; %llu instructions interpreted; %llu blocks checked",
            static_cast<unsigned long long>(ran), seconds, seconds > 0 ? ran / seconds : 0.0, board->pictures, board->cpu.pc,
            static_cast<unsigned long long>(board->cpu.interpreted), static_cast<unsigned long long>(BlocksChecked()));
    if (BlocksUnsure()) {
        // (blocks that wrote to a chip, or to the RAM shared with the ARM, and read it back: run, but not comparable)
        fprintf(stderr, " (%llu more not comparable)", static_cast<unsigned long long>(BlocksUnsure()));
    }
    fprintf(stderr, "\n");
    if (!entriesPath.empty()) {
        // (added to what the file has)
        std::set<uint32_t> entries(board->cpu.newEntries.begin(), board->cpu.newEntries.end());
        if (FILE *in = fopen(entriesPath.c_str(), "r")) {
            char line[64];
            while (fgets(line, sizeof line, in)) {
                if (line[0] != '#' && line[0] != '\n') {
                    entries.insert(static_cast<uint32_t>(strtoul(line, nullptr, 16)));
                }
            }
            fclose(in);
        }
        if (FILE *out = fopen(entriesPath.c_str(), "w")) {
            fprintf(out, "# Places the game's code was seen to reach in ways the translator cannot follow\n");
            for (uint32_t e : entries) {
                fprintf(out, "%06X\n", e);
            }
            fclose(out);
        }
    }
    return 0;
}
