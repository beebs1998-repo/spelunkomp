// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp: https://github.com/tmcleroy/cps2recomp
// A zip file: its list of files, and one of them unpacked (only read, never changed)
#pragma once

#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace spelunk {

struct Zip {
    struct Entry {
        std::string name;
        uint32_t crc = 0, packed = 0, size = 0, offset = 0;
        uint16_t method = 0;
    };
    std::string path;
    std::vector<Entry> entries;

    static uint32_t Le32(const uint8_t *p) {
        return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }
    static uint16_t Le16(const uint8_t *p) {
        return static_cast<uint16_t>(p[0] | (p[1] << 8));
    }
    bool Open(const std::string &file) {
        path = file;
        FILE *f = fopen(file.c_str(), "rb");
        if (!f) {
            return false;
        }
        // The directory is at the end, after a record that says where
        fseek(f, 0, SEEK_END);
        const long size = ftell(f);
        const long tailSize = std::min<long>(size, 65536 + 22);
        std::vector<uint8_t> tail(static_cast<size_t>(tailSize));
        fseek(f, size - tailSize, SEEK_SET);
        bool ok = fread(tail.data(), 1, tail.size(), f) == tail.size();
        long end = -1;
        for (long at = tailSize - 22; ok && at >= 0; at--) {
            if (Le32(&tail[at]) == 0x06054B50) {
                end = at;
                break;
            }
        }
        if (end < 0) {
            fclose(f);
            return false;
        }
        const uint32_t count = Le16(&tail[end + 10]), dirSize = Le32(&tail[end + 12]), dirAt = Le32(&tail[end + 16]);
        std::vector<uint8_t> dir(dirSize);
        fseek(f, dirAt, SEEK_SET);
        ok = fread(dir.data(), 1, dir.size(), f) == dir.size();
        fclose(f);
        size_t at = 0;
        for (uint32_t n = 0; ok && n < count && at + 46 <= dir.size() && Le32(&dir[at]) == 0x02014B50; n++) {
            Entry e;
            e.method = Le16(&dir[at + 10]);
            e.crc = Le32(&dir[at + 16]);
            e.packed = Le32(&dir[at + 20]);
            e.size = Le32(&dir[at + 24]);
            e.offset = Le32(&dir[at + 42]);
            const size_t nameLen = Le16(&dir[at + 28]);
            if (at + 46 + nameLen > dir.size()) {
                break;
            }
            e.name.assign(reinterpret_cast<const char *>(&dir[at + 46]), nameLen);
            entries.push_back(e);
            at += 46 + nameLen + Le16(&dir[at + 30]) + Le16(&dir[at + 32]);
        }
        return ok;
    }
    bool Read(const Entry &e, std::vector<uint8_t> &data) const {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f) {
            return false;
        }
        uint8_t local[30];
        fseek(f, e.offset, SEEK_SET);
        bool ok = fread(local, 1, 30, f) == 30 && Le32(local) == 0x04034B50;
        std::vector<uint8_t> packed(e.packed);
        if (ok) {
            fseek(f, e.offset + 30 + Le16(&local[26]) + Le16(&local[28]), SEEK_SET);
            ok = fread(packed.data(), 1, packed.size(), f) == packed.size();
        }
        fclose(f);
        if (!ok) {
            return false;
        }
        if (e.method == 0) {
            data = std::move(packed);
            return data.size() == e.size;
        }
        if (e.method != 8) {
            return false;
        }
        data.resize(e.size);
        z_stream z = {};
        if (inflateInit2(&z, -MAX_WBITS) != Z_OK) {
            return false;
        }
        z.next_in = packed.data();
        z.avail_in = static_cast<uInt>(packed.size());
        z.next_out = data.data();
        z.avail_out = static_cast<uInt>(data.size());
        const int result = inflate(&z, Z_FINISH);
        inflateEnd(&z);
        return result == Z_STREAM_END && z.total_out == e.size;
    }
};

inline bool ReadWholeFile(const std::string &path, std::vector<uint8_t> &data) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    data.resize(size > 0 ? static_cast<size_t>(size) : 0);
    const size_t got = fread(data.data(), 1, data.size(), f);
    fclose(f);
    return got == data.size();
}

} // namespace spelunk
