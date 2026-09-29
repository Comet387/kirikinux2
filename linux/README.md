# Kirikiroid2 Linux host

This directory provides a small native Linux host for the Kirikiroid2 core. It
uses X11 first and SDL2 as a fallback. Both backends are optional at configure
time, so the host can still be built in a minimal CI/container image and used
for archive/path probing (`--probe`) or engine bring-up (`--no-window`). No
Android runtime or Waydroid is required.

## Build

```sh
cmake -S linux -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux
```

Install the development package for either Xlib (`libx11-dev`) or SDL2
(`libsdl2-dev`) to obtain a window. X11 is selected when both are present.
Oniguruma (`libonig-dev`) enables the TJS2 regular-expression class; the rest
of the interpreter builds without it.

```sh
./build-linux/kirikiroid2-linux --probe game.xp3
./build-linux/kirikiroid2-linux --list game.xp3
./build-linux/kirikiroid2-linux --no-window game-directory
./build-linux/kirikiroid2-linux --eval '1 + 2 * 3'
./build-linux/kirikiroid2-linux --script scenario.tjs
./build-linux/kirikiroid2-linux --script 'game.xp3>startup.tjs'
./build-linux/kirikiroid2-linux game.xp3
```

The host validates the XP3 magic (`XP3\\r\\n \\n\\x1a\\x8b\\x67\\x01`) before entering the
event loop. It links the original TJS2 sources directly and can evaluate an
expression, execute UTF-8/BOM-marked UTF-16 source files, list XP3 contents, and execute
an unprotected script directly from a raw/zlib XP3 segment. KAG/TVP bindings,
encrypted-archive filters, rendering and audio are not connected yet. An engine
shared object can be supplied with `--engine`; the optional bridge
exports two C symbols:

```c
void krkr2_linux_tick(double seconds);
void krkr2_linux_shutdown(void);
```

This keeps platform/window code independent from the Cocos2d Android frontend
and gives a future full core port a stable place to attach initialization.
