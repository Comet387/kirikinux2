# Current Linux port status

Snapshot date: 2026-09-29

## What works

- CMake Release build with the original TJS2 interpreter.
- GTK3 desktop game library when GTK3 development files are present.
- Add a game folder, select XP3/EXE, drag and drop, remember/remove recent
  games, and show startup errors in the window.
- CLI `--probe`, `--list`, `--cat`, `--run`, `--eval`, and `--script` tools.
- Raw/zlib XP3, embedded XP3, and ordinary segments carrying the protected bit.
- Case-insensitive storage lookup and internal `Storages.addAutoPath` prefixes.
- TJS regular expressions when Oniguruma is available.
- Controlled `Plugins.link("layerExImage.dll")` mapping; unknown DLLs fail.
- Release-mode empty-string handling without the previous null-pointer crash.

## Verified in this snapshot

- Local Release compile: passed.
- CTest: 18/18 passed.
- `weimingtom/kirikiroid2_fork3/_testdata/data.xp3`: executes `Config.tjs` and
  `UpdateConfig.tjs`, enters KAG System, then stops at:

  ```text
  system/LayerEx.tjs:39
  Member "Layer" does not exist
  ```

## What does not work yet

The port cannot display or play the test game. The next hard requirement is a
real native `Layer`/`Window` object model connected to the renderer, followed by
font/image drawing, input dispatch, audio/video, and writable KAG save streams.
State-only stubs are not counted as playable rendering.
