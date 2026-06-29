# SubtitleSynchAC1

A runtime subtitle overlay tool that displays real-time subtitles by tracking in-game audio events and mapping them to a local subtitle database.

This project uses a memory hook to detect currently playing audio cues and renders corresponding subtitles through an ImGui-based overlay.


## Features

- Real-time audio event tracking from the game process
- JSON-based subtitle mapping system
- Lightweight ImGui overlay for rendering subtitles
- Supports custom localization packs

---

## How It Works

1. The tool hooks into the game process at runtime
2. It monitors currently playing sound events / sound IDs
3. Each sound ID is matched against a local `subtitles.json` database
4. If a match is found, the corresponding subtitle text is rendered in an ImGui overlay