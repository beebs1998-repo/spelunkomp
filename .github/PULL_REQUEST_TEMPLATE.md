<!-- What this changes, and why. -->

- [ ] Nothing from a game (ROM files, save states, translated code, recordings, listings, memory dumps, pictures other than the README's), no keys, nothing from the makers' own materials or a leak, no emulator's code beyond what `THIRD_PARTY_NOTICES.md` lists. `make audit` passes.
- [ ] What is about one game is in that game's `games/<id>/` (its `game.cfg` or sources), not the toolkit.
- [ ] If a board, a translator, the picture, the sound or a game's code changed: the checks for the games it touches pass (`tools/check.sh`, `tools/check68k.sh`, `tools/check_video.sh`, `tools/check_tools.sh`: say which were run). With every feature off a game runs as before.
- [ ] Gameplay and control changes are an optional feature, off unless chosen.
- [ ] No added input lag or lost speed (`tools/lag.sh`, if timing, input, sound or presentation changed).
- [ ] Keyboard and gamepad both still work.
- [ ] `recomp-common/` is unchanged (it is a copy: change recomp-common and run `tools/sync-common.sh`).
- [ ] A line in `CHANGELOG.md` under "Unreleased"; the README says what players get, if it is a main feature.
