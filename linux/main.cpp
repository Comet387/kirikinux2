#include "kirkr_host.h"

#include <cstdlib>
#include <cstdio>
#include <iostream>

#ifndef KRKR2_VERSION
#define KRKR2_VERSION "1.3.9"
#endif

using krkr2::HostOptions;

static void usage(const char *name) {
  std::cout << "Kirikiroid2 Linux native host " << KRKR2_VERSION << "\n"
            << "Usage: " << name << " [options] [game-dir|game.xp3]\n"
            << "  --probe             validate path and exit\n"
            << "  --list              list entries in the positional XP3 archive\n"
            << "  --no-window         initialize host without opening X11/SDL2\n"
            << "  --engine FILE       load optional bridge library\n"
            << "  --eval EXPR         evaluate a TJS2 expression and exit\n"
            << "  --script STORAGE    execute FILE or quoted XP3>ENTRY and exit\n"
            << "  --size WxH          set window size (default 960x640)\n"
            << "  --fullscreen        request desktop fullscreen\n"
            << "  -h, --help          show this help\n";
}

int main(int argc, char **argv) {
  HostOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg == "--version" || arg == "-v") {
      std::cout << KRKR2_VERSION << "\n";
      return 0;
    }
    if (arg == "-h" || arg == "--help") {
      usage(argv[0]);
      return 0;
    }
    if (arg == "--probe") {
      options.probe_only = true;
    } else if (arg == "--list") {
      options.list_archive = true;
    } else if (arg == "--no-window") {
      options.no_window = true;
    } else if (arg == "--fullscreen") {
      options.fullscreen = true;
    } else if (arg == "--engine" && i + 1 < argc) {
      options.engine_library = argv[++i];
    } else if (arg == "--eval" && i + 1 < argc) {
      options.expression = argv[++i];
    } else if (arg == "--script" && i + 1 < argc) {
      options.script = argv[++i];
    } else if (arg == "--size" && i + 1 < argc) {
      int width = 0, height = 0;
      if (std::sscanf(argv[++i], "%dx%d", &width, &height) != 2 || width < 64 || height < 64) {
        std::cerr << "Invalid --size value; expected WIDTHxHEIGHT\n";
        return 64;
      }
      options.width = width;
      options.height = height;
    } else if (!arg.empty() && arg[0] != '-' && options.game.empty()) {
      options.game = arg;
    } else {
      std::cerr << "Unknown option: " << arg << '\n';
      usage(argv[0]);
      return 64;
    }
  }
  const int actions = static_cast<int>(options.probe_only) +
      static_cast<int>(options.list_archive) + static_cast<int>(options.expression.has_value()) +
      static_cast<int>(options.script.has_value());
  if (actions > 1) {
    std::cerr << "--probe, --list, --eval and --script cannot be combined\n";
    return 64;
  }
  return krkr2::run_host(options);
}
