# SubtitleSynchAC1

A subtitle overlay for Assassin's Creed (2008). It watches the voice lines the
game plays and shows the matching text from a local subtitle database in an
ImGui overlay.

This is a fork of [bloxtbc/SubtitleSynchAC1](https://github.com/bloxtbc/SubtitleSynchAC1).
See [CHANGELOG.md](CHANGELOG.md) for the changes compared with the original.

## Install

1. Install an ASI loader (for example Ultimate ASI Loader as `dinput8.dll`) in the game folder.
2. Copy these files into the game's `scripts` folder:
   - `SubtitleSynchAC1.asi` (built from this repository)
   - `assets/subtitles.json`, `assets/subtitles_secondary.json`, `assets/subtitles.ttf`
   - `assets/SubtitleSynchAC1.ini`
3. Start the game. F1 opens the settings window.

`subtitles_secondary.json` is optional. It holds ambient speech (citizens,
guards, preachers), which is shown in a dimmer colour and never replaces story
dialogue.

## Build

The mod must be built as a 32-bit Windows DLL.

```
git clone --recurse-submodules https://github.com/markf11/SubtitleSynchAC1.git
cmake -S SubtitleSynchAC1 -B build -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release
```

With LLVM-MinGW, set `LLVM_MINGW_ROOT` and pass
`-G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw-x86.cmake` instead of `-A Win32`.

## License

MIT, see [LICENSE](LICENSE).
