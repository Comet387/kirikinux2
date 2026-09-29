#include "kirkr_host.h"
#include "xp3_archive.h"
#ifdef KRKR2_HAVE_TJS
#include "tjs_runtime.h"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <cerrno>
#include <sys/stat.h>
#include <thread>

#ifdef KRKR2_HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif
#ifdef KRKR2_HAVE_SDL2
#include <SDL.h>
#endif

namespace krkr2 {
namespace {

constexpr std::array<unsigned char, 11> kXp3Magic = {
    {'X', 'P', '3', '\r', '\n', ' ', '\n', 0x1a, 0x8b, 0x67, 0x01}};

bool has_magic(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::array<unsigned char, 11> got{};
  in.read(reinterpret_cast<char *>(got.data()), static_cast<std::streamsize>(got.size()));
  return in.gcount() == static_cast<std::streamsize>(got.size()) && got == kXp3Magic;
}

struct Engine {
  using tick_fn = void (*)(double);
  using shutdown_fn = void (*)();
  void *handle = nullptr;
  tick_fn tick = nullptr;
  shutdown_fn shutdown = nullptr;

  bool open(const std::string &path) {
    if (path.empty()) return true;
    handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
      std::cerr << "kirikiroid2: cannot load engine " << path << ": " << dlerror() << '\n';
      return false;
    }
    // The bridge ABI is intentionally tiny. A complete port can export these
    // symbols without depending on the window backend used by this host.
    tick = reinterpret_cast<tick_fn>(dlsym(handle, "krkr2_linux_tick"));
    shutdown = reinterpret_cast<shutdown_fn>(dlsym(handle, "krkr2_linux_shutdown"));
    return true;
  }
  ~Engine() {
    if (shutdown) shutdown();
    if (handle) dlclose(handle);
  }
};

class Window {
 public:
  virtual ~Window() = default;
  virtual bool open(int width, int height, bool fullscreen) = 0;
  virtual bool pump() = 0;
  virtual void present(const char *status) = 0;
};

#ifdef KRKR2_HAVE_X11
class X11Window final : public Window {
  Display *display_ = nullptr;
  ::Window window_ = 0;
  Atom wm_delete_ = 0;
  GC gc_ = nullptr;
 public:
  ~X11Window() override {
    if (display_) {
      if (gc_) XFreeGC(display_, gc_);
      if (window_) XDestroyWindow(display_, window_);
      XCloseDisplay(display_);
    }
  }
  bool open(int width, int height, bool fullscreen) override {
    (void)fullscreen; // fullscreen transitions are left to the desktop compositor
    display_ = XOpenDisplay(nullptr);
    if (!display_) return false;
    int screen = DefaultScreen(display_);
    window_ = XCreateSimpleWindow(display_, RootWindow(display_, screen), 0, 0,
                                  static_cast<unsigned>(width), static_cast<unsigned>(height),
                                  0, BlackPixel(display_, screen), BlackPixel(display_, screen));
    XSelectInput(display_, window_, ExposureMask | KeyPressMask | StructureNotifyMask);
    wm_delete_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display_, window_, &wm_delete_, 1);
    XStoreName(display_, window_, "Kirikiroid2 (Linux)");
    gc_ = XCreateGC(display_, window_, 0, nullptr);
    XMapWindow(display_, window_);
    XFlush(display_);
    return true;
  }
  bool pump() override {
    while (display_ && XPending(display_)) {
      XEvent event{};
      XNextEvent(display_, &event);
      if (event.type == ClientMessage && static_cast<Atom>(event.xclient.data.l[0]) == wm_delete_)
        return false;
      if (event.type == KeyPress && event.xkey.keycode == 9) return false; // Escape
    }
    return true;
  }
  void present(const char *status) override {
    if (!display_ || !window_) return;
    XClearWindow(display_, window_);
    XSetForeground(display_, gc_, WhitePixel(display_, DefaultScreen(display_)));
    XDrawString(display_, window_, gc_, 24, 32, status, static_cast<int>(std::strlen(status)));
    XFlush(display_);
  }
};
#endif

#ifdef KRKR2_HAVE_SDL2
class SDLWindow final : public Window {
  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
 public:
  ~SDLWindow() override {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
  }
  bool open(int width, int height, bool fullscreen) override {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return false;
    Uint32 flags = SDL_WINDOW_SHOWN | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    window_ = SDL_CreateWindow("Kirikiroid2 (Linux)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               width, height, flags);
    if (!window_) return false;
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    return renderer_ != nullptr;
  }
  bool pump() override {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) return false;
    }
    return true;
  }
  void present(const char *status) override {
    if (!renderer_) return;
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    // Text rendering is intentionally delegated to the Kirikiri engine. Keep a
    // visible frame in the host while an engine is being loaded.
    (void)status;
    SDL_RenderPresent(renderer_);
  }
};
#endif

} // namespace

bool resolve_game(const std::string &path, GamePath &result, std::string &error) {
  result = {};
  result.input = path;
  if (path.empty()) {
    error = "no game path supplied";
    return false;
  }
  struct stat st {};
  if (::stat(path.c_str(), &st) != 0) {
    error = std::strerror(errno);
    return false;
  }
  result.is_directory = S_ISDIR(st.st_mode);
  result.root = path;
  result.is_xp3 = !result.is_directory && has_magic(path);
  if (!result.is_directory && !result.is_xp3) {
    error = "file is not an XP3 archive (expected XP3 magic)";
    return false;
  }
  return true;
}

int run_host(const HostOptions &options) {
#ifdef KRKR2_HAVE_TJS
  if (options.expression || options.script) {
    TjsRunResult result;
    if (options.script) {
      const std::size_t delimiter = options.script->find('>');
      if (delimiter == std::string::npos) {
        result = execute_tjs_file(*options.script);
      } else if (delimiter == 0 || delimiter + 1 == options.script->size()) {
        result.error = "XP3 script name must be ARCHIVE>ENTRY";
      } else {
        Xp3Archive archive;
        std::string error;
        std::vector<std::uint8_t> source;
        const std::string archive_name = options.script->substr(0, delimiter);
        const std::string entry_name = options.script->substr(delimiter + 1);
        if (!archive.open(archive_name, error) ||
            !archive.read(entry_name, source, error)) {
          result.error = error;
        } else {
          result = execute_tjs_bytes(source, *options.script);
        }
      }
    } else {
      result = evaluate_tjs(*options.expression);
    }
    if (!result.ok) {
      std::cerr << "kirikiroid2: TJS2: " << result.error << '\n';
      return 5;
    }
    if (result.has_value) std::cout << result.value << '\n';
    return 0;
  }
#else
  if (options.expression || options.script) {
    std::cerr << "kirikiroid2: this build does not include the TJS2 interpreter\n";
    return 5;
  }
#endif

  if (options.list_archive) {
    if (options.game.empty()) {
      std::cerr << "kirikiroid2: --list requires an XP3 archive path\n";
      return 64;
    }
    Xp3Archive archive;
    std::string error;
    if (!archive.open(options.game, error)) {
      std::cerr << "kirikiroid2: " << error << '\n';
      return 6;
    }
    for (const Xp3Entry &entry : archive.entries()) {
      std::cout << entry.original_size << '\t' << entry.archive_size << '\t'
                << ((entry.flags & (std::uint32_t{1} << 31)) ? "protected\t" : "-\t")
                << entry.name << '\n';
    }
    return 0;
  }

  GamePath game;
  std::string error;
  if (!options.game.empty() && !resolve_game(options.game, game, error)) {
    std::cerr << "kirikiroid2: " << error << ": " << options.game << '\n';
    return 2;
  }
  if (options.probe_only) {
    if (!options.game.empty())
      std::cout << (game.is_xp3 ? "XP3 archive" : "game directory") << ": " << game.root << '\n';
    return 0;
  }

  Engine engine;
  if (!engine.open(options.engine_library)) return 3;
  if (options.no_window) {
    std::cout << "kirikiroid2: headless host ready" << (options.game.empty() ? "\n" : " for " + game.root + "\n");
    return 0;
  }

  std::unique_ptr<Window> window;
#ifdef KRKR2_HAVE_X11
  window = std::make_unique<X11Window>();
  if (!window->open(options.width, options.height, options.fullscreen)) window.reset();
#endif
#ifdef KRKR2_HAVE_SDL2
  if (!window) {
    window = std::make_unique<SDLWindow>();
    if (!window->open(options.width, options.height, options.fullscreen)) window.reset();
  }
#endif
  if (!window) {
    std::cerr << "kirikiroid2: no window backend available (use --no-window or install X11/SDL2)\n";
    return 4;
  }

  const auto start = std::chrono::steady_clock::now();
  auto previous = start;
  for (;;) {
    if (!window->pump()) break;
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - previous;
    previous = now;
    if (engine.tick) engine.tick(elapsed.count());
    window->present(game.is_xp3 ? "XP3 game loaded" : "Kirikiri game directory loaded");
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
  }
  return 0;
}

} // namespace krkr2
