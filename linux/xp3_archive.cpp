#include "xp3_archive.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

#include <zlib.h>

namespace krkr2 {
namespace {

constexpr std::array<std::uint8_t, 11> kXp3Magic = {
    {'X', 'P', '3', '\r', '\n', ' ', '\n', 0x1a, 0x8b, 0x67, 0x01}};
constexpr std::uint8_t kMethodMask = 0x07;
constexpr std::uint8_t kMethodRaw = 0;
constexpr std::uint8_t kMethodZlib = 1;
constexpr std::uint8_t kIndexContinue = 0x80;
constexpr std::uint32_t kFileProtected = std::uint32_t{1} << 31;
constexpr std::uint64_t kMaxIndexSize = 256ULL * 1024 * 1024;
constexpr std::uint64_t kMaxReadableEntrySize = 512ULL * 1024 * 1024;

bool checked_add(std::uint64_t a, std::uint64_t b, std::uint64_t &result) {
  if (b > std::numeric_limits<std::uint64_t>::max() - a) return false;
  result = a + b;
  return true;
}

bool read_at(std::ifstream &input, std::uint64_t file_size, std::uint64_t offset,
             void *buffer, std::size_t size) {
  std::uint64_t end = 0;
  if (!checked_add(offset, size, end) || end > file_size ||
      size > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()) ||
      offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
    return false;
  if (size == 0) return true;
  input.clear();
  input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!input) return false;
  input.read(static_cast<char *>(buffer), static_cast<std::streamsize>(size));
  return input.gcount() == static_cast<std::streamsize>(size);
}

std::uint16_t u16(const std::uint8_t *data) {
  return static_cast<std::uint16_t>(data[0]) |
         (static_cast<std::uint16_t>(data[1]) << 8);
}

std::uint32_t u32(const std::uint8_t *data) {
  return static_cast<std::uint32_t>(data[0]) |
         (static_cast<std::uint32_t>(data[1]) << 8) |
         (static_cast<std::uint32_t>(data[2]) << 16) |
         (static_cast<std::uint32_t>(data[3]) << 24);
}

std::uint64_t u64(const std::uint8_t *data) {
  return static_cast<std::uint64_t>(u32(data)) |
         (static_cast<std::uint64_t>(u32(data + 4)) << 32);
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

bool utf16le_to_utf8(const std::uint8_t *data, std::size_t units,
                     std::string &text) {
  text.clear();
  text.reserve(units);
  for (std::size_t i = 0; i < units; ++i) {
    std::uint32_t value = u16(data + i * 2);
    if (value >= 0xd800 && value <= 0xdbff) {
      if (++i >= units) return false;
      const std::uint32_t low = u16(data + i * 2);
      if (low < 0xdc00 || low > 0xdfff) return false;
      value = 0x10000 + ((value - 0xd800) << 10) + (low - 0xdc00);
    } else if (value >= 0xdc00 && value <= 0xdfff) {
      return false;
    }
    append_utf8(text, value);
  }
  return true;
}

std::string normalized_name(std::string name) {
  for (char &ch : name) {
    if (ch == '\\') ch = '/';
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  }
  while (!name.empty() && name.front() == '/') name.erase(name.begin());
  return name;
}

bool find_archive_offset(std::ifstream &input, std::uint64_t file_size,
                         std::uint64_t &offset) {
  std::array<std::uint8_t, 11> header{};
  if (!read_at(input, file_size, 0, header.data(), header.size())) return false;
  if (header == kXp3Magic) {
    offset = 0;
    return true;
  }
  if (header[0] != 'M' || header[1] != 'Z') return false;

  constexpr std::size_t kBlockSize = 256 * 1024;
  std::vector<std::uint8_t> block(kBlockSize);
  for (std::uint64_t base = 16; base < file_size; base += kBlockSize) {
    const std::size_t count = static_cast<std::size_t>(
        std::min<std::uint64_t>(kBlockSize, file_size - base));
    if (!read_at(input, file_size, base, block.data(), count)) return false;
    for (std::size_t pos = 0; pos + kXp3Magic.size() <= count; pos += 16) {
      if (std::memcmp(block.data() + pos, kXp3Magic.data(), kXp3Magic.size()) == 0) {
        offset = base + pos;
        return true;
      }
    }
  }
  return false;
}

struct Chunk {
  std::size_t start = 0;
  std::size_t size = 0;
};

bool find_chunk(const std::vector<std::uint8_t> &data, std::size_t begin,
                std::size_t length, const char name[5], Chunk &chunk) {
  if (begin > data.size() || length > data.size() - begin) return false;
  std::size_t cursor = begin;
  const std::size_t end = begin + length;
  while (cursor < end) {
    if (end - cursor < 12) return false;
    const bool found = std::memcmp(data.data() + cursor, name, 4) == 0;
    const std::uint64_t raw_size = u64(data.data() + cursor + 4);
    cursor += 12;
    if (raw_size > end - cursor) return false;
    if (found) {
      chunk.start = cursor;
      chunk.size = static_cast<std::size_t>(raw_size);
      return true;
    }
    cursor += static_cast<std::size_t>(raw_size);
  }
  return false;
}

bool inflate_data(const std::vector<std::uint8_t> &compressed,
                  std::size_t output_size, std::vector<std::uint8_t> &output) {
  if (output_size > std::numeric_limits<uLongf>::max() ||
      compressed.size() > std::numeric_limits<uLong>::max())
    return false;
  output.resize(output_size);
  uLongf actual = static_cast<uLongf>(output_size);
  const int status = uncompress(output.data(), &actual, compressed.data(),
                                static_cast<uLong>(compressed.size()));
  return status == Z_OK && actual == output_size;
}

} // namespace

bool Xp3Archive::open(const std::string &path, std::string &error) {
  path_.clear();
  archive_offset_ = 0;
  file_size_ = 0;
  entries_.clear();

  std::ifstream input(path, std::ios::binary);
  if (!input) {
    error = "cannot open XP3 archive: " + path;
    return false;
  }
  input.seekg(0, std::ios::end);
  const std::streamoff end = input.tellg();
  if (end < 0) {
    error = "cannot determine XP3 archive size";
    return false;
  }
  file_size_ = static_cast<std::uint64_t>(end);
  if (!find_archive_offset(input, file_size_, archive_offset_)) {
    error = "XP3 signature not found";
    return false;
  }

  std::uint64_t pointer_position = archive_offset_ + kXp3Magic.size();
  std::set<std::uint64_t> visited;
  for (;;) {
    if (!visited.insert(pointer_position).second) {
      error = "XP3 index chain contains a loop";
      return false;
    }
    std::array<std::uint8_t, 8> index_pointer{};
    if (!read_at(input, file_size_, pointer_position, index_pointer.data(),
                 index_pointer.size())) {
      error = "truncated XP3 index pointer";
      return false;
    }
    std::uint64_t index_position = 0;
    if (!checked_add(archive_offset_, u64(index_pointer.data()), index_position)) {
      error = "invalid XP3 index offset";
      return false;
    }

    std::uint8_t index_flags = 0;
    if (!read_at(input, file_size_, index_position, &index_flags, 1)) {
      error = "truncated XP3 index header";
      return false;
    }
    std::uint64_t cursor = index_position + 1;
    std::array<std::uint8_t, 16> sizes{};
    std::uint64_t packed_size = 0;
    std::uint64_t unpacked_size = 0;
    if ((index_flags & kMethodMask) == kMethodZlib) {
      if (!read_at(input, file_size_, cursor, sizes.data(), 16)) {
        error = "truncated compressed XP3 index header";
        return false;
      }
      packed_size = u64(sizes.data());
      unpacked_size = u64(sizes.data() + 8);
      cursor += 16;
    } else if ((index_flags & kMethodMask) == kMethodRaw) {
      if (!read_at(input, file_size_, cursor, sizes.data(), 8)) {
        error = "truncated XP3 index header";
        return false;
      }
      packed_size = unpacked_size = u64(sizes.data());
      cursor += 8;
    } else {
      error = "unsupported XP3 index encoding";
      return false;
    }
    if (packed_size > kMaxIndexSize || unpacked_size > kMaxIndexSize ||
        packed_size > std::numeric_limits<std::size_t>::max() ||
        unpacked_size > std::numeric_limits<std::size_t>::max()) {
      error = "XP3 index is too large for this build";
      return false;
    }
    std::uint64_t index_end = 0;
    if (!checked_add(cursor, packed_size, index_end) || index_end > file_size_) {
      error = "truncated XP3 index data";
      return false;
    }
    std::vector<std::uint8_t> packed(static_cast<std::size_t>(packed_size));
    if (!read_at(input, file_size_, cursor, packed.data(), packed.size())) {
      error = "cannot read XP3 index data";
      return false;
    }
    std::vector<std::uint8_t> index;
    if ((index_flags & kMethodMask) == kMethodZlib) {
      if (!inflate_data(packed, static_cast<std::size_t>(unpacked_size), index)) {
        error = "cannot decompress XP3 index";
        return false;
      }
    } else {
      index = std::move(packed);
    }

    std::size_t position = 0;
    while (position < index.size()) {
      Chunk file;
      if (!find_chunk(index, position, index.size() - position, "File", file)) {
        if (position == 0 && !index.empty()) {
          error = "XP3 index contains no File chunk";
          return false;
        }
        break;
      }
      Chunk info, segments, hash;
      if (!find_chunk(index, file.start, file.size, "info", info) ||
          !find_chunk(index, file.start, file.size, "segm", segments) ||
          !find_chunk(index, file.start, file.size, "adlr", hash) ||
          info.size < 22 || hash.size < 4 || segments.size == 0 ||
          segments.size % 28 != 0) {
        error = "malformed XP3 File chunk";
        return false;
      }

      Xp3Entry entry;
      const std::uint8_t *info_data = index.data() + info.start;
      entry.flags = u32(info_data);
      entry.original_size = u64(info_data + 4);
      entry.archive_size = u64(info_data + 12);
      const std::size_t name_units = u16(info_data + 20);
      if (name_units > (info.size - 22) / 2 ||
          !utf16le_to_utf8(info_data + 22, name_units, entry.name)) {
        error = "invalid UTF-16 name in XP3 index";
        return false;
      }
      entry.hash = u32(index.data() + hash.start);

      std::uint64_t total_original = 0;
      std::uint64_t total_archive = 0;
      for (std::size_t segment = 0; segment < segments.size; segment += 28) {
        const std::uint8_t *source = index.data() + segments.start + segment;
        const std::uint32_t flags = u32(source);
        Xp3Segment parsed;
        if ((flags & kMethodMask) == kMethodRaw) {
          parsed.compressed = false;
        } else if ((flags & kMethodMask) == kMethodZlib) {
          parsed.compressed = true;
        } else {
          error = "unsupported XP3 segment encoding";
          return false;
        }
        const std::uint64_t relative_start = u64(source + 4);
        parsed.original_size = u64(source + 12);
        parsed.archive_size = u64(source + 20);
        if (!checked_add(archive_offset_, relative_start, parsed.start) ||
            !checked_add(total_original, parsed.original_size, total_original) ||
            !checked_add(total_archive, parsed.archive_size, total_archive)) {
          error = "XP3 segment size overflow";
          return false;
        }
        std::uint64_t segment_end = 0;
        if (!checked_add(parsed.start, parsed.archive_size, segment_end) ||
            segment_end > file_size_) {
          error = "XP3 segment points outside the archive";
          return false;
        }
        entry.segments.push_back(parsed);
      }
      if (total_original != entry.original_size || total_archive != entry.archive_size) {
        error = "XP3 segment totals do not match file metadata";
        return false;
      }
      entries_.push_back(std::move(entry));
      position = file.start + file.size;
    }

    if (!(index_flags & kIndexContinue)) break;
    pointer_position = index_end;
  }

  path_ = path;
  std::stable_sort(entries_.begin(), entries_.end(), [](const Xp3Entry &left,
                                                        const Xp3Entry &right) {
    return normalized_name(left.name) < normalized_name(right.name);
  });
  return true;
}

bool Xp3Archive::read(const std::string &name, std::vector<std::uint8_t> &data,
                      std::string &error) const {
  const std::string wanted = normalized_name(name);
  const auto entry = std::find_if(entries_.begin(), entries_.end(),
                                  [&](const Xp3Entry &candidate) {
    return normalized_name(candidate.name) == wanted;
  });
  if (entry == entries_.end()) {
    error = "file not found in XP3 archive: " + name;
    return false;
  }
  if (entry->flags & kFileProtected) {
    error = "protected XP3 entries require an extraction filter: " + entry->name;
    return false;
  }
  if (entry->original_size > kMaxReadableEntrySize ||
      entry->original_size > std::numeric_limits<std::size_t>::max()) {
    error = "XP3 entry is too large for this build";
    return false;
  }

  std::ifstream input(path_, std::ios::binary);
  if (!input) {
    error = "cannot reopen XP3 archive: " + path_;
    return false;
  }
  data.clear();
  data.reserve(static_cast<std::size_t>(entry->original_size));
  for (const Xp3Segment &segment : entry->segments) {
    if (segment.archive_size > std::numeric_limits<std::size_t>::max() ||
        segment.original_size > std::numeric_limits<std::size_t>::max()) {
      error = "XP3 segment is too large for this build";
      return false;
    }
    std::vector<std::uint8_t> packed(static_cast<std::size_t>(segment.archive_size));
    if (!read_at(input, file_size_, segment.start, packed.data(), packed.size())) {
      error = "cannot read XP3 segment for " + entry->name;
      return false;
    }
    if (segment.compressed) {
      std::vector<std::uint8_t> unpacked;
      if (!inflate_data(packed, static_cast<std::size_t>(segment.original_size),
                        unpacked)) {
        error = "cannot decompress XP3 segment for " + entry->name;
        return false;
      }
      data.insert(data.end(), unpacked.begin(), unpacked.end());
    } else {
      if (segment.archive_size != segment.original_size) {
        error = "raw XP3 segment has mismatched sizes";
        return false;
      }
      data.insert(data.end(), packed.begin(), packed.end());
    }
  }
  if (data.size() != entry->original_size) {
    error = "XP3 entry size does not match its metadata";
    return false;
  }
  return true;
}

} // namespace krkr2
