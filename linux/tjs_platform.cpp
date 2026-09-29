#include "tjs.h"
#include "TickCount.h"

#include <chrono>
#include <iostream>

namespace TJS {

void TVPConsoleLog(const tjs_char *line) {
  std::cerr << ttstr(line).AsStdString() << '\n';
}

} // namespace TJS

ttstr TVPGetMessageByLocale(const std::string &key) {
  if (key == "err_read_error") return ttstr(TJS_W("Read error"));
  return ttstr(key);
}

tjs_uint32 TVPGetRoughTickCount32() {
  using namespace std::chrono;
  const auto elapsed = duration_cast<milliseconds>(steady_clock::now().time_since_epoch());
  return static_cast<tjs_uint32>(elapsed.count());
}
