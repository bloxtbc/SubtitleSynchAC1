# Changelog

Changes compared with the original SubtitleSynchAC1 by bloxtbc (commit 9e6e93a).

## v5.22

### Audio matching
- Subtitles are found through the game's real audio resource, not by changing the first digit of the event ID.
- Voices that the game picks from random or switch sound events are recognised.
- The same line is shown again when it is played again.
- Hook patterns must match exactly once; the original game call, registers, flags and FPU state are preserved.
- A thread-safe, bounded event queue replaces the previous queue.

### Timing
- Subtitles pause with the pause menu (keyboard, mouse or controller) and continue after a short delay.
- Dialogue that starts at the moment of pausing is kept and shown after resuming.
- Scripted game freezes (for example Animus tutorials) no longer stop the subtitles.
- Subtitles stay on screen 1.5 s after the line ends (`TailExtensionMs`); new dialogue still replaces them at once.

### Layout
- Font size follows the game window height.
- Long lines wrap automatically; every line is centred and has its own background.
- Subtitles move up while the save icon or the "Press any button to interact" prompt is visible.
- Bracketed sound captions (for example `[laughs]`) can be hidden.

### Ambient speech
- New optional secondary database (`subtitles_secondary.json`) for citizens, guards, mission targets and preachers.
- Ambient lines are drawn in a dimmer colour and never replace story dialogue.
- Ambient lines are shown only within range (citizens 15 m, their thanks 20 m, preachers 20 m, others 21 m) and follow the speaker while the voice plays.
- A nearer or higher-priority ambient speaker takes over the subtitle.

### Settings
- F1 window: show/hide subtitles, size slider, sound captions, preview text and Save.
- New INI keys: `ShowSubtitles`, `ShowSoundCaptions`, `MaxWidthPercent`, `BottomMargin`, `TailExtensionMs`, `SecondaryColorR/G/B/A`.
- Removed INI keys (now automatic): `SubtitlePosX/Y`, `AutoPosition`, `PaddingX/Y`, `BackgroundColorR/G/B`.
- Saving keeps other INI sections.
- Optional diagnostic log for bug reports (off by default, not in the INI).

### Subtitle database (English)
- 581 new lines: 39 primary (story, Animus, Tamir) and 542 secondary (ambient speech).
- 106 corrected lines (wording, typos, names, missing words).
- 21 lines use bracketed sound captions.
- Preacher timing matches the real speech length.
- Tamir's "Must... learn... your... PLACE!" is shown from its first word.
- Animus loading tips moved to the secondary database.

### Build
- 32-bit build with MSVC or LLVM-MinGW (toolchain file included).
- Unit tests for pausing, layout, prompts, subtitles, hooks and the diagnostic log.
