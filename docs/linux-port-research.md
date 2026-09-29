# Kirikiroid2 1.3.9 Linux port research

This note records the source/APK correspondence and the native portability boundary.

## Port milestones

- The native host now compiles and links the original `src/core/tjs2` runtime.
  `--eval` and `--script` provide headless bring-up paths, including Unicode,
  exception and format-string regression coverage.
- Linux portability fixes cover standard headers outside the `TJS` namespace,
  `va_list` copying on x86_64, null-safe allocator cleanup and optional
  Oniguruma support.
- The native host can list raw/zlib XP3 indices, read segmented entries and run
  UTF-8/UTF-16 TJS directly from an archive. Protected entries are detected and
  rejected until their game-specific extraction filter is registered.
- The next playable milestone is a minimal TVP layer: expose the XP3 reader as
  binary/text storage, register the KAG-visible native classes used during
  startup, and dispatch rendering into the existing X11/SDL2 loop. Audio and
  video decoding can remain disabled until that path reaches a static scene.

The Android-first Yuri fork is useful for modern CMake/dependency structure;
the `kirikiroid2_fork3` Linux experiment confirms the required `va_list` and
platform shims. Neither is imported wholesale because their Cocos2d/vendor
layouts are much larger than this repository's incremental native target.

## Source tree

- Repository: `zeas2/Kirikiroid2`.
- Release tag `1.3.9` resolves to commit `384e22ff04bea85832deb660cec19e6b6ab28046` (2018-07-05).
- The checked-out `master` is `d1c2b125` (2024-06-05), a privacy-policy-only commit after the release.
- There is no CMake/Makefile for desktop. The only build files are Android `ndk-build` files.
- The tree contains a small Linux platform shim (`src/core/environ/linux/Platform.cpp`) and an SDL-era source file (`src/core/environ/sdl/tvpsdl.cpp`), but no Linux process/window/audio frontend.
- The Android build depends on large vendor trees that are not in this checkout: Cocos2d-x, FFmpeg, FreeType, libarchive, OpenAL/Opus, etc. Therefore the Android `Android.mk` cannot be reused as a standalone Linux build.

## APK 1.3.9 evidence

- Manifest package is `org.tvp.kirikiri2_free_10309`, version name `1.3.9`; the repository manifest still says package `org.tvp.kirikiri2` and version `1.3.4` because release packaging overrides these values.
- Native libraries exist for `armeabi-v7a` and `arm64-v8a` only (no x86): `libgame.so`, `libSDL2.so`, and `libffmpeg.so`.
- `libgame.so` dynamically needs SDL2, FFmpeg, GLES 1/2, EGL, OpenSLES, Android, log, dl, libc/libm and C++ runtime. Its exported JNI methods match `KR2Activity.java` (`nativeTouches*`, `nativeKeyAction`, IME text callbacks, scroll/hover and message box callbacks), confirming a real native Cocos2d/Kirikiroid build rather than a compatibility wrapper.
- Every APK `assets/img/*.png` file (33 files) has the exact SHA-256 as its source `cocos/kr2/cocosstudio/img/*.png` counterpart. APK `assets/ui/*.csb` files are compiled Cocos Studio versions of the source `.csd` files. Locale files match the 1.3.9 tag apart from line endings and a few storage-warning strings.

## Startup and behavior relevant to a desktop frontend

`AppDelegate::applicationDidFinishLaunching` creates a Cocos GL view, design resolution 960x640 with `SHOW_ALL`, initializes locales/UI and creates `TVPMainScene`. On Android `TVPCheckStartupArg()` returns false, so the first screen is `MainFileSelectorForm`; selecting a game path calls `TVPMainScene::startupFrom`, then `Application::StartApplication(path)`. The desktop frontend should preserve this path selection and pass keyboard, mouse, text/IME, wheel, and controller events into the TVP scene.
