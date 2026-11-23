# Releasing

Versions follow `MAJOR.MINOR.PATCH`, with `-beta.N` or `-rc.N` while a version is not final, one for the whole
repository: the toolkit, the launcher and every game. A tag is `v` and the version (`v0.1.0-beta.1`). A release
with a `-` in its version is marked as a pre-release on GitHub. Versions stay beta until every platform's download
has been played by hand.

## What a release is

| File | For |
| --- | --- |
| `Spelunkomp-macOS.zip` | macOS 13 or later, Apple silicon and Intel (one app; signed ad hoc, not notarized) |
| `Spelunkomp-linux-x86_64.tar.gz` | Linux x86_64, Steam Deck, Steam Machine |
| `Spelunkomp-linux-arm64.tar.gz` | Linux ARM64 (aarch64) |
| `Spelunkomp-windows-x64.zip` | Windows 10 and 11, 64-bit |
| `Spelunkomp-android.apk` | Android 9 or later, 64-bit ARM (signed with the project's release key, kept outside the repository: every release must carry the same key) |
| `SHA256SUMS.txt` | The five downloads' checksums (not the manifests', which carry their archives' own) |
| `update-macos.txt`, `update-linux-x86_64.txt`, `update-linux-arm64.txt`, `update-windows-x64.txt` | The launcher's updates: each system's manifest |

Each holds the launcher, every game's program and the licences. No ROM, graphics or sound is in them: the player
supplies their own ROM sets. **Each game's program does hold that game's code translated to C++ and compiled**, as a
build from source does. That means a download can only be built by someone with every game's ROM set, on their own
machine: CI never builds or holds one. `tools/release/check_archive.sh` checks each archive before it is packed (no
game files, no emulator's name and no home path in any program, the licences there).

**The launcher updates itself from these** (`recomp-common/launcher/src/update.h`), from 0.1.0-beta.1 on: when it
opens it reads the repository's releases, takes the newest with a manifest for its system (a beta only while it is
a beta itself), and compares the files the manifest lists with its own. **Update**, on the update card at the top of
the Games page, downloads that system's archive (the same file as a player's download), checks it against the
manifest's SHA-256, unpacks it, checks each file that changed, and only then puts them in place. Only a copy whose
launcher is where the manifest has it updates itself (the app's `Contents/MacOS/spelunkomp-launcher`, or
`spelunkomp-launcher` at the unpacked folder's top); a build from source says to download the release instead. Each
release script writes its system's manifest after its archive (`tools/release/lib.sh`, `update_files`), and
`tools/release/all.sh` checks that each names its archive as it is and this version. A release without them is not
offered to the launchers, and a release whose `version` line is not its tag is refused by them. The Android app has
no manifest: it takes the newest release's APK, checks it against `SHA256SUMS.txt` and hands it to Android's
installer, which asks the player and installs it over the app (the same release key: settings, saves and ROM sets
stay). So `SHA256SUMS.txt` must list the APK.

The release's text is `tools/release/notes.sh`'s: the paragraph the version's section of `CHANGELOG.md` begins
with (the script stops if there is none), the downloads, how to install on each system, and the section's list.

## Steps

1. **In a pull request:** set `VERSION`, move what is under "Unreleased" in `CHANGELOG.md` to a new `## <version>`
   section that begins with a paragraph saying what is in the release. `make check` passes, and on a machine with
   the ROM sets the checks of CONTRIBUTING.md pass for the games the release changes (`tools/gate.sh ROMS` for all
   of them). Merge.
2. **Tag** the merged commit on `main` and push the tag:

   ```sh
   git switch main && git pull
   git tag -a "v$(cat VERSION)" -m "Spelunkomp $(cat VERSION)"
   git push origin "v$(cat VERSION)"
   ```

   The `Release` workflow (`.github/workflows/release.yml`) checks that the tag is `VERSION`, runs the audit, and
   opens a **draft** release whose notes are the changelog's section. It fails if the changelog has no such
   section. It builds nothing.
3. **Build the downloads** from the tagged commit, on a Mac with Docker, every game's translated code in
   `work/<id>/gen` and the ROM sets (`ROMS=folder`):

   ```sh
   git status --short            # must print nothing: build exactly what was tagged
   tools/release/all.sh          # macOS, Linux x86_64 and ARM64, Windows, Android, then SHA256SUMS.txt and the
                                 # update-<system>.txt manifests, in build/release/
   ```

4. **Try each download** before anyone else does: unpack it somewhere else, start the launcher, choose the ROM
   folder, play a game of each board with a gamepad and with the keyboard (the picture, the sound, a save state, the
   training tools). macOS on a Mac, the Linux one on a Steam Deck or Steam Machine, the Windows one on a Windows PC,
   the Android app on a phone or tablet.
5. **Upload and publish:**

   ```sh
   cd build/release
   gh release upload "v$(cat ../../VERSION)" Spelunkomp-macOS.zip Spelunkomp-linux-x86_64.tar.gz \
       Spelunkomp-linux-arm64.tar.gz Spelunkomp-windows-x64.zip Spelunkomp-android.apk SHA256SUMS.txt \
       update-macos.txt update-linux-x86_64.txt update-linux-arm64.txt update-windows-x64.txt
   gh release edit "v$(cat ../../VERSION)" --draft=false
   ```

6. Add an empty "Unreleased" section to `CHANGELOG.md` for what comes next.

## If something is wrong after publishing

Don't replace the files of a published release. Fix it, raise the version (`-beta.N+1`, or the patch number) and
release again; say in the changelog what was wrong.

## Source for the binaries

The GPL-3.0 asks that the source of a binary be offered with it. The tag is that source: GitHub attaches it to
every release. Build only from a tagged, clean commit so that this stays true. The translated code is not part of
that source: it is made from the player's own ROM sets by the tools that are.
