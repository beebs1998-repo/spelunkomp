// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The deciphering of a PGM game's program ROM (machine/pgm_crypt.cpp).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace spelunk {

// `words` (the program's 16-bit words as the 68000 reads them, from where the enciphered part begins) deciphered in
// place by the named game's rules (game.cfg's `cipher`: ddp3, ket, espgal; `arm_cipher`: ddp2, for the ROM of ARM
// code on DoDonPachi II's cartridge, from its start). The games whose rules take a table of 256 bytes have it worked
// out from `words` themselves and checked against `check`, game.cfg's `cipher_sha1` (the table's SHA-1). False, with
// `error` saying why, for a name it does not know or a table that does not come out as checked (nothing deciphered).
// (env SPELUNK_KEY_LOG=1: how the table came out, on stderr)
bool PgmDecipher(const std::string &scheme, uint16_t *words, size_t count, const std::string &check, std::string &error);

} // namespace spelunk
