#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace krkr2 {

struct TjsRunResult {
  bool ok = false;
  bool has_value = false;
  std::string value;
  std::string error;
};

/** Evaluate one TJS2 expression with the original Kirikiroid2 interpreter. */
TjsRunResult evaluate_tjs(const std::string &expression);

/** Execute a UTF-8 TJS2 source file with the original interpreter. */
TjsRunResult execute_tjs_file(const std::string &path);

/** Execute source bytes loaded from a virtual storage such as an XP3 entry. */
TjsRunResult execute_tjs_bytes(const std::vector<std::uint8_t> &source,
                               const std::string &source_name);

/** Execute one entry with Scripts/Storages bound to a directory or XP3 root. */
TjsRunResult execute_tjs_storage(const std::string &root,
                                 const std::string &entry);

/** Execute startup.tjs, falling back to System/Initialize.tjs. */
TjsRunResult execute_tjs_startup(const std::string &root);

} // namespace krkr2
