// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
//
// The output's lines written off the game's thread. What the port writes now and then in play (the host's time each
// minute, port/host_time.h; online play's report every ten seconds, port/netplay.cpp) goes to logs/game.log, or to a
// pipe the system's log reads: a write there can wait on the disk or the reader (issue #18: a frame of 40 to 70 ms
// once a run on a Steam Deck), and the frame would wait with it. LogLater hands the line, made already, to a thread
// of its own that writes it and flushes; the lines are the same and in the order given. LogFlush waits until all
// given are written (the end of a run, before the lines written straight after it).
#pragma once

#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace port {

class LogWriter {
public:
    static LogWriter &Get() {
        static LogWriter writer;
        return writer;
    }
    void Later(FILE *to, std::string line) {
        std::lock_guard<std::mutex> hold(m_mutex);
        if (!m_thread.joinable()) {
            m_thread = std::thread([this] { Run(); });
        }
        m_lines.emplace_back(to, std::move(line));
        m_wake.notify_one();
    }
    void Flush() {
        std::unique_lock<std::mutex> hold(m_mutex);
        m_done.wait(hold, [this] { return m_lines.empty() && !m_writing; });
    }
    ~LogWriter() {
        {
            std::lock_guard<std::mutex> hold(m_mutex);
            m_stop = true;
            m_wake.notify_one();
        }
        if (m_thread.joinable()) {
            m_thread.join(); // (what was given still written first)
        }
    }

private:
    void Run() {
        std::unique_lock<std::mutex> hold(m_mutex);
        for (;;) {
            m_wake.wait(hold, [this] { return m_stop || !m_lines.empty(); });
            if (m_lines.empty()) {
                return; // (stopping, all written)
            }
            std::pair<FILE *, std::string> line = std::move(m_lines.front());
            m_lines.pop_front();
            m_writing = true;
            hold.unlock();
            fputs(line.second.c_str(), line.first);
            fflush(line.first);
            hold.lock();
            m_writing = false;
            if (m_lines.empty()) {
                m_done.notify_all();
            }
        }
    }
    std::mutex m_mutex;
    std::condition_variable m_wake, m_done;
    std::deque<std::pair<FILE *, std::string>> m_lines;
    bool m_writing = false, m_stop = false;
    std::thread m_thread;
};

// A line (with its \n) to `to`, written by the log's thread
inline void LogLater(FILE *to, std::string line) {
    LogWriter::Get().Later(to, std::move(line));
}

// Until every line given to LogLater is written
inline void LogFlush() {
    LogWriter::Get().Flush();
}

} // namespace port
