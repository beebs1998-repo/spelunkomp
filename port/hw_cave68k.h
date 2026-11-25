// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// What the game program (port/main.cpp) does in the way of Cave's 68000 boards: the picture (the native renderer
// drawing the frame the video hands on, port/gpu68k.h, or the software model's through SDL's renderer), the training
// tools' memory of the game (runtime/moments.h), the EEPROM and the sound chip's samples.
#pragma once

#include "../machine/cave68k.h"
#include "../runtime/game.h"
#include "../runtime/moments.h"
#include "../runtime/state.h"
#include "gpu68k.h"
#include "hud.h"
#include "png_write.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace port {

using namespace spelunk;

struct Hw {
    static constexpr int kWidth = CaveVideo::kWidth, kHeight = CaveVideo::kHeight;
    // How many frames after its code ran a frame's picture is shown (the sprite chip draws the list a frame wrote at the next frame's interrupt): hit boxes are drawn that late
    static constexpr uint64_t kPictureLag = 1;
    // The board's frame: 272 lines at 15,625 a second (57.44 Hz)
    static constexpr std::chrono::nanoseconds kFrameTime{Cave68k::kFrameNanoseconds};
    // Online play: the most frames run on guessed controls before the game waits for the other side (port/netplay.h;
    // a frame run again is cheap: well under a millisecond)
    static constexpr int kNetMostAhead = 8;
    static constexpr const char *kStateHeader = "spelunkomp 68k state 1 ";

    std::unique_ptr<Cave68k> &board; // (the program's: made anew at a reset)
    Gpu68k gpu;
    NativeFrame68k frame;
    bool useGpu = false, checkVideo = false;
    uint32_t picturesBefore = 0, framePictures = 0, frameLatches = 0, maxSprites = 0;
    uint64_t checkedFrames = 0, wrongFrames = 0;
    std::string checkShots = getenv("SPELUNK_CHECK_SHOTS") ? getenv("SPELUNK_CHECK_SHOTS") : "";

    explicit Hw(std::unique_ptr<Cave68k> &board)
        : board(board) {}

    static void StartCpu(Cave68k &b) {
        b.mode = M68K::Mode::Native;
        b.z80.z80Mode = Z80::Mode::Native; // (a board with a Z80 for sound: its translated code where there is some)
        if (ThisGame().start) {
            ThisGame().start(b);
        }
    }
    // The EEPROM (the game's settings and scores), kept in the saves folder
    uint8_t *Saved(size_t &size) {
        size = sizeof(board->eeprom.words);
        return reinterpret_cast<uint8_t *>(board->eeprom.words);
    }
    bool &SavedChanged() {
        return board->eeprom.changed;
    }
    void CarryOver(Cave68k &fresh) {
        std::memcpy(fresh.eeprom.words, board->eeprom.words, sizeof(fresh.eeprom.words));
        fresh.eeprom.changed = board->eeprom.changed;
    }
    uint32_t SampleRate() const {
        return Ymz280b::sampleRate;
    }
    std::vector<int16_t> &Sound() {
        return board->SoundOut();
    }
    void SetTiming(bool) {}

    // The picture: the native renderer, or (softwareOnly, or no GPU that will do) the software model's through SDL's
    // renderer
    bool Init(SDL_Window *window, bool softwareOnly, bool check) {
        checkVideo = check;
        useGpu = !softwareOnly && gpu.Init(window, board->video);
        return useGpu;
    }
    void Attach() {
        board->video.native = useGpu ? &frame : nullptr;
        board->video.rasterize = !useGpu || checkVideo;
        if (useGpu) {
            board->Redraw(); // (the frame as the board is now, for the first picture)
            Render();
        }
    }
    void SelfTest() {}
    // (GPU only) the window in real pixels; and a picture made by the program (the menu) shown
    // (GPU only) the window drawn into from now on; none while the picture goes through a shader (port/shading.h)
    bool SetWindow(SDL_Window *window) {
        return !useGpu || gpu.SetWindow(window);
    }
    void OutputSize(int &width, int &height) const {
        gpu.OutputSize(width, height);
    }
    void PresentPicture(const uint32_t *pixels, int width, int height, const SDL_FRect &to) {
        gpu.PresentPicture(pixels, width, height, to);
    }
    // The screen as the board has it now, as pixels here (from the GPU when that is where the picture is)
    void ReadScreen(uint32_t *out) {
        if (useGpu) {
            gpu.ReadScreen(out);
        } else {
            board->Picture(out);
        }
    }
    // The frame the video handed on, drawn by the GPU when there is something new in it
    void Render() {
        if (!useGpu || (frame.pictures == framePictures && frame.spritesLatched == frameLatches)) {
            return;
        }
        framePictures = frame.pictures;
        frameLatches = frame.spritesLatched;
        gpu.Render(board->video, frame);
        maxSprites = std::max(maxSprites, gpu.spritesDrawn);
    }

    // ---- The training tools: the practice point and rewind, each moment the whole board ----
    void MakeTools() {}
    void PointSet() {
        Keep(m_point);
        m_pointSet = true;
    }
    void PointBack() {
        Put(m_point);
        moments.Clear();
    }
    void ToolsLost() {
        moments.Clear();
    }
    void TimelineKeep() {
        Keep(m_now);
        moments.Keep(m_now);
    }
    bool TimelineStepBack() {
        const std::vector<uint8_t> *then = moments.StepBack();
        if (then) {
            m_now = *then;
            Put(m_now);
        }
        return then != nullptr;
    }
    // (the names the CV1000's tools have, for the program's one loop)
    struct PointView {
        Hw &hw;
        bool IsSet() const { return hw.m_pointSet; }
        void Clear() { hw.m_pointSet = false; }
    } point{*this};
    struct TimelineView {
        Hw &hw;
        bool Begun() const { return hw.moments.Begun(); }
        void Clear() { hw.moments.Clear(); }
        uint32_t Frames() const { return hw.moments.Frames(); }
        size_t Moments() const { return hw.moments.Count(); }
        size_t Bytes() const { return hw.moments.Bytes(); }
    } timeline{*this};
    Moments moments;

    void BeforeSave() {}
    void AfterLoad() {
        board->Redraw();
        Render();
    }

    // ---- A frame ----
    void BeforeFrame() {
        picturesBefore = board->pictures;
    }
    // After the board's frame: its picture drawn by the GPU. True: the game gave the sprite chip no new list this frame
    // (it was late)
    bool AfterFrame(bool rewindOn) {
        if (rewindOn) {
            moments.Note();
        }
        Render();
        return board->pictures == picturesBefore;
    }
    // --check-video: the GPU's picture against the software model's, pixel for pixel
    void CheckFrame(uint64_t frameCount) {
        if (!checkVideo || !useGpu) {
            return;
        }
        std::vector<uint32_t> soft(static_cast<size_t>(kWidth) * kHeight), hard(soft.size());
        board->Picture(soft.data());
        gpu.ReadScreen(hard.data());
        size_t wrong = 0;
        for (size_t i = 0; i < soft.size(); i++) {
            wrong += soft[i] != hard[i];
        }
        checkedFrames++;
        if (wrong) {
            wrongFrames++;
            if (wrongFrames <= 5) {
                fprintf(stderr, "check-video: frame %llu: %zu pixels differ (%zu sprites)\n", static_cast<unsigned long long>(frameCount + 1), wrong,
                        frame.sprites.size());
                if (!checkShots.empty()) {
                    WritePng((checkShots + "-" + std::to_string(frameCount + 1) + "-soft.png").c_str(), soft.data(), kWidth, kHeight);
                    WritePng((checkShots + "-" + std::to_string(frameCount + 1) + "-gpu.png").c_str(), hard.data(), kWidth, kHeight);
                }
            }
        }
    }
    void SetOverlay(const Hud &hud) {
        if (useGpu) {
            gpu.SetOverlay(hud.Any() ? hud.pixels.data() : nullptr, hud.width, hud.height);
        }
    }
    // Run-ahead: the board kept, frames run on for their picture only (drawn by the GPU as each is made), and the board
    // put back. The GPU keeps nothing of a frame but its picture, so nothing of it needs putting back
    void BeginAhead() {
        board->KeepNow();
        if (useGpu) {
            gpu.AheadWork(true);
        }
    }
    void AheadFrame() {
        board->RunFrame();
        Render();
    }
    void EndAhead() {
        if (useGpu) {
            gpu.AheadWork(false);
        }
        board->PutBack();
    }
    // The input lag readout's looks (port/main.cpp), spread over the frames shown: the board as it is now set aside
    // twice (LookKeep: the moment both of a look's runs start from), then, a frame at a time between the game's own,
    // the board and one of them changing places (LookIn: with what the video last handed on, which the next picture
    // is drawn from), a frame run unseen with the sound chips held and its picture drawn and read (LookFrame), and the
    // two changing back (LookOut), the game's own picture drawn again. Nothing of a look's frames is left in the
    // game's state, nor in what is shown.
    struct Look {
        Cave68k::Aside board;
        NativeFrame68k frame;
        uint32_t framePictures = 0, frameLatches = 0;
    };
    void LookKeep() {
        board->SetAside(m_looks[0].board);
        m_looks[0].frame = frame;
        m_looks[0].framePictures = framePictures;
        m_looks[0].frameLatches = frameLatches;
        m_lookCopy = true; // (the second copy at the first frame of the look: the two copies not both in one frame)
    }
    void LookIn(int which) {
        if (m_lookCopy) {
            m_looks[1] = m_looks[0];
            m_lookCopy = false;
        }
        Trade(m_looks[which], true);
    }
    void LookFrame(int read) {
        board->RunFrame();
        if (useGpu) {
            // (drawn whether or not the frame made a new picture: the one drawn last may be another moment's)
            framePictures = frame.pictures;
            frameLatches = frame.spritesLatched;
            gpu.Render(board->video, frame);
        }
        LookRead(read);
    }
    // (the picture of a look's frame, read without waiting for the GPU: asked for a frame on, by then long done;
    // `read`: one of two at once)
    bool LookReady(int read) {
        return !useGpu || gpu.ReadDone(read);
    }
    bool LookScreen(int read, uint32_t *out) {
        if (useGpu) {
            return gpu.EndScreenRead(read, out);
        }
        std::memcpy(out, m_lookScreens[read].data(), m_lookScreens[read].size() * 4);
        return true;
    }
    std::vector<uint32_t> m_lookScreens[2];
    void LookRead(int read) {
        if (useGpu) {
            gpu.StartScreenRead(read);
        } else {
            m_lookScreens[read].resize(static_cast<size_t>(kWidth) * kHeight);
            board->Picture(m_lookScreens[read].data());
        }
    }
    void LookOut(int which) {
        Trade(m_looks[which], false);
        if (useGpu) {
            gpu.Render(board->video, frame);
        }
    }
    void LookDrop() {
        m_looks[0] = {};
        m_looks[1] = {};
    }
    Look m_looks[2];
    bool m_lookCopy = false;
    void Trade(Look &look, bool hold) {
        board->Trade(look.board, hold);
        std::swap(frame, look.frame);
        std::swap(framePictures, look.framePictures);
        std::swap(frameLatches, look.frameLatches);
    }
    // Online play's rollback (port/netplay.h): the whole board kept exactly (the sound chips with it) and put back,
    // and a frame run unseen (no picture made: the next shown frame makes its own)
    // (two moments at once, `which`: the anchor and the next one)
    void NetKeep(int which) {
        board->KeepNow(true, which);
    }
    void NetDrop(int) {}
    void NetBack(int which) {
        board->PutBack(which);
        framePictures = ~0u; // (the next shown frame drawn whatever it says)
    }
    void NetFrame() {
        board->noPicture = true;
        board->RunFrame();
        board->noPicture = false;
    }
    // (GPU only) the window's own drawing of the screen, at three times the picture's size, into a file
    // (`w` x `h`: the picture's shape as shown, port/main.cpp's canvasSize)
    void WindowShot(const std::string &path, int degrees, int w, int h) {
        std::vector<uint32_t> out;
        gpu.ScreenToPixels(degrees / 90, w, h, out);
        WritePng(path.c_str(), out.data(), static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    }
    void PresentScreen(int degrees, const SDL_FRect &to) {
        gpu.PresentScreen(degrees / 90, to);
    }
    int Report() {
        if (checkVideo && useGpu) {
            fprintf(stderr, "check-video: %llu of %llu frames differed; most sprites in a frame: %u\n", static_cast<unsigned long long>(wrongFrames),
                    static_cast<unsigned long long>(checkedFrames), maxSprites);
        }
        if (getenv("SPELUNK_REPORT_STATE")) {
            fprintf(stderr, "state: frame %llu hash %016llx vram %016llx\n", static_cast<unsigned long long>(board->frame),
                    static_cast<unsigned long long>(board->StateHash()), 0ull);
        }
        return checkVideo && wrongFrames ? 1 : 0;
    }

private:
    bool m_pointSet = false;
    std::vector<uint8_t> m_point, m_now;
    void Keep(std::vector<uint8_t> &into) {
        into.clear();
        Archive ar(into, true);
        board->Serialize(ar);
    }
    void Put(std::vector<uint8_t> &from) {
        Archive ar(from, false);
        board->Serialize(ar);
        board->Redraw();
        Render();
        board->SoundOut().clear();
    }
};

} // namespace port
