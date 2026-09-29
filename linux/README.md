# Kirikiroid2 Linux host

This directory provides the native Linux host for the Kirikiroid2 core. A GTK3
game-library window is the default entry point. It lets users add a game
directory, choose an XP3/EXE archive, retain recent games, remove entries, drag
files in, and run the selected startup script. The lower-level X11/SDL2 host
and command-line tools remain available. No Android runtime or Waydroid is
required.

## Build

```sh
cmake -S linux -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux
```

Install GTK3 development files (`libgtk-3-dev`) for the game library and either
Xlib (`libx11-dev`) or SDL2 (`libsdl2-dev`) for the low-level engine window.
The project still builds as a CLI-only host when these optional packages are
absent.
Oniguruma (`libonig-dev`) enables the TJS2 regular-expression class; the rest
of the interpreter builds without it.

```sh
./build-linux/kirikiroid2-linux --probe game.xp3
./build-linux/kirikiroid2-linux --list game.xp3
./build-linux/kirikiroid2-linux --no-window game-directory
./build-linux/kirikiroid2-linux --eval '1 + 2 * 3'
./build-linux/kirikiroid2-linux --script scenario.tjs
./build-linux/kirikiroid2-linux --script 'game.xp3>startup.tjs'
./build-linux/kirikiroid2-linux --cat 'game.xp3>Config.tjs'
./build-linux/kirikiroid2-linux --run game-directory
./build-linux/kirikiroid2-linux --run game.xp3
./build-linux/kirikiroid2-linux game.xp3
```

The host recognizes raw XP3 files and XP3 data embedded in Windows executables.
It links the original TJS2 sources directly and can evaluate expressions,
execute UTF-8/BOM-marked UTF-16 source, inspect archive entries, and execute
raw/zlib segments. The protected flag is treated like upstream TVP: it requests
an extraction filter when one is installed, but does not by itself reject a
plain segment. `--run` provides:
`Scripts.execStorage`, `Scripts.evalStorage`, `Scripts.exec`, `Scripts.eval`,
real `Storages.addAutoPath` prefix lookup, common System/Debug compatibility,
and a controlled built-in mapping for `layerExImage.dll`. Unknown Windows DLLs
still fail explicitly. An engine
shared object can be supplied with `--engine`; the optional bridge
exports two C symbols:

```c
void krkr2_linux_tick(double seconds);
void krkr2_linux_shutdown(void);
```

The fork3 `_testdata/data.xp3` fixture currently executes `Config.tjs` and
`UpdateConfig.tjs`, enters KAG System, and stops at `system/LayerEx.tjs:39`
because the native `Layer` class/rendering path is not implemented yet. This is
therefore not a playable build: it has a real game picker and script startup,
but does not display game graphics or play audio yet.
