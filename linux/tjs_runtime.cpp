#include "tjs_runtime.h"

#include "tjs.h"
#include "tjsError.h"

#include <fstream>
#include <iterator>

namespace krkr2 {
namespace {

using namespace TJS;

std::string narrow(const ttstr &text) {
  return text.AsStdString();
}

void append_utf8(std::string &out, std::uint32_t value) {
  if (value <= 0x7f) {
    out.push_back(static_cast<char>(value));
  } else if (value <= 0x7ff) {
    out.push_back(static_cast<char>(0xc0 | (value >> 6)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else if (value <= 0xffff) {
    out.push_back(static_cast<char>(0xe0 | (value >> 12)));
    out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else {
    out.push_back(static_cast<char>(0xf0 | (value >> 18)));
    out.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
  }
}

bool decode_utf16(const std::vector<std::uint8_t> &bytes, bool big_endian,
                  std::string &text) {
  if ((bytes.size() - 2) % 2 != 0) return false;
  text.clear();
  for (std::size_t offset = 2; offset < bytes.size(); offset += 2) {
    std::uint32_t value = big_endian
        ? (static_cast<std::uint32_t>(bytes[offset]) << 8) | bytes[offset + 1]
        : static_cast<std::uint32_t>(bytes[offset]) |
              (static_cast<std::uint32_t>(bytes[offset + 1]) << 8);
    if (value >= 0xd800 && value <= 0xdbff) {
      offset += 2;
      if (offset >= bytes.size()) return false;
      const std::uint32_t low = big_endian
          ? (static_cast<std::uint32_t>(bytes[offset]) << 8) | bytes[offset + 1]
          : static_cast<std::uint32_t>(bytes[offset]) |
                (static_cast<std::uint32_t>(bytes[offset + 1]) << 8);
      if (low < 0xdc00 || low > 0xdfff) return false;
      value = 0x10000 + ((value - 0xd800) << 10) + (low - 0xdc00);
    } else if (value >= 0xdc00 && value <= 0xdfff) {
      return false;
    }
    append_utf8(text, value);
  }
  return true;
}

bool decode_source(const std::vector<std::uint8_t> &bytes, std::string &text) {
  if (bytes.empty()) {
    text.clear();
    return true;
  }
  if (bytes.size() >= 2 && bytes[0] == 0xff && bytes[1] == 0xfe)
    return decode_utf16(bytes, false, text);
  if (bytes.size() >= 2 && bytes[0] == 0xfe && bytes[1] == 0xff)
    return decode_utf16(bytes, true, text);
  std::size_t offset = bytes.size() >= 3 && bytes[0] == 0xef &&
      bytes[1] == 0xbb && bytes[2] == 0xbf ? 3 : 0;
  text.assign(reinterpret_cast<const char *>(bytes.data() + offset),
              bytes.size() - offset);
  return text.find('\0') == std::string::npos;
}

TjsRunResult make_value(tTJSVariant &value) {
  TjsRunResult result;
  result.ok = true;
  result.has_value = value.Type() != tvtVoid;
  if (result.has_value) {
    value.ToString();
    result.value = ttstr(value.GetString()).AsStdString();
  }
  return result;
}

template <typename Callback>
TjsRunResult invoke_tjs(tTJS *engine, Callback callback) {
  TjsRunResult result;
  try {
    tTJSVariant value;
    callback(engine, value);
    result = make_value(value);
  } catch (const eTJSScriptError &error) {
    result.error = narrow(error.GetMessage());
    if (error.GetBlockName()) {
      result.error += " at ";
      result.error += narrow(ttstr(error.GetBlockName()));
      const tjs_int line = error.GetSourceLine();
      if (line >= 0) result.error += ":" + std::to_string(line);
    }
  } catch (const eTJS &error) {
    result.error = narrow(error.GetMessage());
  } catch (const std::exception &error) {
    result.error = error.what();
  } catch (...) {
    result.error = "unknown TJS2 error";
  }
  return result;
}

template <typename Callback>
TjsRunResult run_tjs(Callback callback) {
  tTJS *engine = nullptr;
  try {
    engine = new tTJS();
  } catch (const eTJS &error) {
    TjsRunResult result;
    result.error = narrow(error.GetMessage());
    return result;
  } catch (const std::exception &error) {
    TjsRunResult result;
    result.error = error.what();
    return result;
  }

  // Script exceptions retain their source block. invoke_tjs destroys the
  // caught exception before this function releases the owning interpreter.
  TjsRunResult result = invoke_tjs(engine, callback);
  engine->Release();
  return result;
}

} // namespace

TjsRunResult evaluate_tjs(const std::string &expression) {
  return run_tjs([&](tTJS *engine, tTJSVariant &result) {
    const ttstr name(TJS_W("command line"));
    engine->EvalExpression(ttstr(expression), &result, nullptr, &name);
  });
}

TjsRunResult execute_tjs_file(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    TjsRunResult result;
    result.error = "cannot open script: " + path;
    return result;
  }

  std::vector<std::uint8_t> contents{
      std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  if (!input.good() && !input.eof()) {
    TjsRunResult result;
    result.error = "cannot read script: " + path;
    return result;
  }

  return execute_tjs_bytes(contents, path);
}

TjsRunResult execute_tjs_bytes(const std::vector<std::uint8_t> &source,
                               const std::string &source_name) {
  std::string decoded;
  if (!decode_source(source, decoded)) {
    TjsRunResult result;
    result.error = "invalid UTF-16 source or embedded NUL byte: " + source_name;
    return result;
  }
  return run_tjs([&](tTJS *engine, tTJSVariant &result) {
    const ttstr name(source_name);
    engine->ExecScript(ttstr(decoded), &result, nullptr, &name);
  });
}

} // namespace krkr2
