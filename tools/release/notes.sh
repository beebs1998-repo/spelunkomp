#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's.
#
# A release's text: what is in the release (the paragraph a version's section of CHANGELOG.md begins with, before
# its list), what to download, how to install it on each system, and the rest of that section (docs/RELEASING.md).
#   tools/release/notes.sh 0.1.0-beta.1 > notes.md
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VERSION=${1:?usage: notes.sh VERSION}
[ -f "$ROOT/CHANGELOG.md" ] || { echo "error: no CHANGELOG.md" >&2; exit 1; }
changes=$(awk -v v="## $VERSION" '$0 ~ "^## " { on = (index($0, v) == 1); next } on' "$ROOT/CHANGELOG.md")
if [ -z "$changes" ]; then
    echo "error: CHANGELOG.md has no section for $VERSION" >&2
    exit 1
fi
# The section's opening paragraph (everything before its first "- " line), and the list after it
summary=$(printf '%s\n' "$changes" | awk '/^- / { exit } { line[n++] = $0 }
    END { s = 0; while (s < n && line[s] == "") s++; e = n - 1; while (e >= s && line[e] == "") e--
          for (i = s; i <= e; i++) print line[i] }')
list=$(printf '%s\n' "$changes" | awk '/^- / { on = 1 } on')
if [ -z "$summary" ]; then
    echo "error: CHANGELOG.md's section for $VERSION has no paragraph before its list (what is in the release, for" >&2
    echo "the top of the release's page)" >&2
    exit 1
fi
cat <<EOF
$summary

Unofficial native ports of Cave's arcade shooting games (the CV1000, Cave's 68000 boards and IGS's PGM), in one
download with one launcher. **No game is included: you need your own ROM sets**
([which ones](https://github.com/tmcleroy/spelunkomp#what-you-need)).

## Download

| Your system | Download |
| --- | --- |
| macOS 13 or later, Apple silicon or Intel | **Spelunkomp-macOS.zip** |
| Windows 10 or 11, 64-bit | **Spelunkomp-windows-x64.zip** |
| Linux x86_64, Steam Deck, Steam Machine | **Spelunkomp-linux-x86_64.tar.gz** |
| Linux ARM64 (aarch64) | **Spelunkomp-linux-arm64.tar.gz** |
| Android 9 or later, 64-bit ARM, with Vulkan | **Spelunkomp-android.apk** |

\`SHA256SUMS.txt\` has the downloads' checksums. The \`update-*.txt\` files are what the launcher reads to update
itself; you don't need them.

## Install

### macOS
1. Open the zip: it gives **Spelunkomp.app**. Move it to your **Applications** folder (it updates itself, so it
   must be somewhere it can write, not left in Downloads' unzipped folder).
2. The app is not notarized by Apple, so the first time: right-click it and choose **Open**, then **Open**. If
   macOS only offers **Done** or **Move to Trash**, choose **Done**, then open **System Settings → Privacy &
   Security**, scroll down and press **Open Anyway** next to "Spelunkomp".
3. On the **Games** page, press **Choose folder…** and pick the folder with your ROM sets. Press **Play** beside a game.

### Windows
1. Right-click the zip, choose **Extract All…**, and extract it to a folder of your own (for example
   \`Documents\\Games\`), not Program Files: the launcher updates itself and needs to write there.
2. In the \`Spelunkomp\` folder, open **spelunkomp-launcher.exe**. The programs are not signed, so the first time
   Windows may say it protected your PC: press **More info**, then **Run anyway**.
3. On the **Games** page, press **Choose folder…** and pick the folder with your ROM sets. Press **Play** beside a game.

### Linux
1. Unpack it where it can stay: \`tar xzf Spelunkomp-linux-x86_64.tar.gz -C ~/Applications\` (or
   \`Spelunkomp-linux-arm64.tar.gz\` on ARM64; any folder of your own works).
2. Run \`~/Applications/Spelunkomp/spelunkomp-launcher\`. It needs only an X11 or Wayland desktop and the usual
   sound libraries (SDL 3 is built in).
3. On the **Games** page, press **Choose folder…** and pick the folder with your ROM sets. Press **Play** beside a game.

### Steam Deck and Steam Machine
1. Hold the power button and choose **Switch to Desktop**.
2. Download the Linux x86_64 file. In the file manager (Dolphin), right-click it, choose **Extract → Extract
   archive here**, and move the \`Spelunkomp\` folder where it can stay (for example a new \`Applications\` folder
   in your home folder).
3. Put your ROM sets in one folder on the device.
4. In the \`Spelunkomp\` folder, open **spelunkomp-launcher**, press **Choose folder…** on the **Games** page and pick that folder.
5. On each game's page, press **Add to Steam**. Go back with **Return to Gaming Mode**: the games are in your
   Library under **Non-Steam**.

### Android
1. Download **Spelunkomp-android.apk** on the phone or tablet and open it. Android asks to allow installs from the
   browser or file manager the first time: allow it, then press **Install**.
2. Open **Spelunkomp**, press **Add ROM sets…** and pick your ROM sets' zips: they are copied into the app.
3. Press **Play** beside a game. The screen's own controls show until a gamepad is used.

## Update

From this release on, the launcher updates itself to the next ones. It checks for a new version when it opens (the
**Check when the launcher opens** switch on its update card turns that off) and offers it on the update card at the
top of the **Games** page: **Update** downloads it, checks it and starts the new version. On Android, **Update**
downloads the new APK, checks it and opens Android's installer, which asks you to confirm and installs it over the
app (the first time, Android asks you to allow Spelunkomp to install apps).

The manual way still works: download the new release and put it in place of the old app or folder, or on Android
install the new APK over the old app. Your settings and saves are kept either way: they are in your own user folder.

## What's new in $VERSION

$list
EOF
