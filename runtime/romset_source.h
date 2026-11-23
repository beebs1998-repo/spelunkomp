// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// What the ROM set loaders share (runtime/romset.cpp, runtime/romset_pgm.cpp): a set's files found by checksum in a
// folder or a zip.
#pragma once

#include "../machine/zip.h"

#include <zlib.h>

#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace spelunk::romset {

// The files of one place (a folder or a zip), read on demand by checksum
struct Source {
    Zip zip;
    bool isZip = false;
    struct Plain {
        std::string path;
        uint64_t size;
        bool hashed = false;
        uint32_t crc = 0;
    };
    std::vector<Plain> files;

    bool Open(const std::string &where) {
        struct stat st;
        if (stat(where.c_str(), &st) != 0) {
            return false;
        }
        if (!S_ISDIR(st.st_mode)) {
            isZip = true;
            return zip.Open(where);
        }
        if (DIR *d = opendir(where.c_str())) {
            while (const dirent *e = readdir(d)) {
                const std::string path = where + "/" + e->d_name;
                if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                    files.push_back({path, static_cast<uint64_t>(st.st_size)});
                }
            }
            closedir(d);
        }
        return true;
    }
    // The file of this size and checksum (some dumps of a flash chip carry 0x100 more bytes after its contents:
    // the checksum is the whole file's, the contents are its first `size` bytes)
    static constexpr uint64_t kExtra = 0x100;
    // The file of this checksum, whatever its size
    bool FindAny(uint32_t crc, std::vector<uint8_t> &data) {
        if (isZip) {
            for (const Zip::Entry &e : zip.entries) {
                if (e.crc == crc) {
                    return zip.Read(e, data);
                }
            }
            return false;
        }
        for (Plain &f : files) {
            std::vector<uint8_t> bytes;
            if (!f.hashed) {
                if (!ReadWholeFile(f.path, bytes)) {
                    continue;
                }
                f.crc = static_cast<uint32_t>(crc32(0, bytes.data(), static_cast<uInt>(bytes.size())));
                f.hashed = true;
            }
            if (f.crc == crc && (!bytes.empty() || ReadWholeFile(f.path, bytes))) {
                data = std::move(bytes);
                return true;
            }
        }
        return false;
    }
    bool Find(uint32_t crc, uint64_t size, std::vector<uint8_t> &data) {
        if (isZip) {
            for (const Zip::Entry &e : zip.entries) {
                if (e.crc == crc && (e.size == size || e.size == size + kExtra)) {
                    if (!zip.Read(e, data)) {
                        return false;
                    }
                    data.resize(size);
                    return true;
                }
            }
            return false;
        }
        for (Plain &f : files) {
            if (f.size != size && f.size != size + kExtra) {
                continue;
            }
            std::vector<uint8_t> bytes;
            if (!ReadWholeFile(f.path, bytes)) {
                continue;
            }
            if (!f.hashed) {
                f.crc = static_cast<uint32_t>(crc32(0, bytes.data(), static_cast<uInt>(bytes.size())));
                f.hashed = true;
            }
            if (f.crc == crc) {
                data = std::move(bytes);
                data.resize(size);
                return true;
            }
        }
        return false;
    }
};

// "name:crc" (the name is for messages only)
inline bool ParseFile(const std::string &text, std::string &name, uint32_t &crc) {
    const size_t colon = text.find(':');
    if (colon == std::string::npos) {
        return false;
    }
    name = text.substr(0, colon);
    crc = static_cast<uint32_t>(strtoul(text.substr(colon + 1).c_str(), nullptr, 16));
    return true;
}

// The dumps hold each 16-bit word low byte first; the board's memory is big-endian
inline void SwapWords(uint8_t *p, size_t size) {
    for (size_t i = 0; i + 1 < size; i += 2) {
        std::swap(p[i], p[i + 1]);
    }
}

} // namespace spelunk::romset
