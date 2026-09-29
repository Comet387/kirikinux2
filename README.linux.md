# Linux native port status

The `linux/` target is the native desktop host for the 1.3.9 source line. It does not use Waydroid or any Android emulator.

It currently provides:

- the repository's original TJS2 interpreter, built natively on Linux, with
  command-line expression evaluation and UTF-8/BOM-marked UTF-16 script execution;
- X11 window/input backend, with SDL2 fallback when Xlib is unavailable;
- command-line game directory and XP3 selection plus XP3 magic validation;
- read-only XP3 index/segment support (`--list`), including compressed indices
  and direct UTF-8/BOM-marked UTF-16 TJS execution via `archive.xp3>entry.tjs`;
- directory/XP3 startup execution (`--run`) with minimal compatible `Scripts`
  and `Storages` objects for nested script loading and path queries;
- drag-and-drop selection in the SDL2 backend, fullscreen toggle, Escape/close handling;
- a small `dlopen` bridge (`krkr2_linux_tick` / `krkr2_linux_shutdown`) for the
  remaining renderer/audio integration;
- CMake + CPack Debian packaging and a GitHub Actions workflow that produces `.deb` and AppImage artifacts.

Quick checks:

```sh
./build-linux/kirikiroid2-linux --eval '1 + 2 * 3'
./build-linux/kirikiroid2-linux --script scenario.tjs
./build-linux/kirikiroid2-linux --list game.xp3
./build-linux/kirikiroid2-linux --script 'game.xp3>startup.tjs'
./build-linux/kirikiroid2-linux --run game.xp3
ctest --test-dir build-linux --output-on-failure
```

The Android APK also bundles an ARM-only `libgame.so` and vendor libraries that
are not present in the public repository. TJS2 is now native, but a playable
port still needs KAGParser and the window/layer/system TVP objects,
protected-archive filters, renderer, input dispatch and audio backend. This
target does not substitute a different Kirikiri engine or an emulator. See
`docs/apk-1.3.9-analysis.md` and `docs/linux-port-research.md`.
