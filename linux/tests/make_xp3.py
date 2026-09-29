#!/usr/bin/env python3
"""Create a tiny compressed XP3 archive for native-host regression tests."""

import pathlib
import struct
import sys
import zlib


MAGIC = b"XP3\r\n \n\x1a\x8b\x67\x01"


def chunk(name: bytes, payload: bytes) -> bytes:
    return name + struct.pack("<Q", len(payload)) + payload


def main() -> int:
    if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != "--raw"):
        raise SystemExit("usage: make_xp3.py OUTPUT [--raw]")

    raw = len(sys.argv) == 3
    name = "startup.tjs"
    source = b"\xff\xfe" + "6 * 7;".encode("utf-16le")
    packed_source = source if raw else zlib.compress(source)
    data_offset = len(MAGIC) + 8

    encoded_name = name.encode("utf-16le")
    info = struct.pack(
        "<IQQH", 0, len(source), len(packed_source), len(encoded_name) // 2
    ) + encoded_name
    segment = struct.pack(
        "<IQQQ", 0 if raw else 1, data_offset, len(source), len(packed_source)
    )
    checksum = struct.pack("<I", zlib.adler32(source))
    index = chunk(b"File", chunk(b"info", info) + chunk(b"segm", segment) +
                  chunk(b"adlr", checksum))
    packed_index = index if raw else zlib.compress(index)
    index_offset = data_offset + len(packed_source)

    archive = (
        MAGIC
        + struct.pack("<Q", index_offset)
        + packed_source
        + (b"\x00" + struct.pack("<Q", len(index)) if raw else
           b"\x01" + struct.pack("<QQ", len(packed_index), len(index)))
        + packed_index
    )
    pathlib.Path(sys.argv[1]).write_bytes(archive)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
