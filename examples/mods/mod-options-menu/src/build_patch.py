"""Builds data/9ba626afa44a3aa3.patch_0 from mod_options.lua: the patch archive Bingus Shared Loader (v15+) discovers
addons in, holding one Lua resource named by the seed-zero MurmurHash64A of its path, in the Lua envelope (body length,
version 2, source). The archive format follows Bingus Shared Loader's scripts/archive.py (0BSD).
Usage: python3 src/build_patch.py (from anywhere); run it after every change to mod_options.lua."""
from pathlib import Path
import struct

NAME = 'mods/hellforge/mod_options'
ARCHIVE = '9ba626afa44a3aa3.patch_0'
LUA_TYPE = 0xA14E8DFA2CD117E2
HERE = Path(__file__).resolve().parent


def resource_hash(name):
    data = name.encode('utf-8')
    mask, mix = (1 << 64) - 1, 0xC6A4A7935BD1E995
    value = len(data) * mix & mask
    end = len(data) // 8 * 8
    for (word,) in struct.iter_unpack('<Q', data[:end]):
        word = word * mix & mask
        word ^= word >> 47
        value = (value ^ (word * mix & mask)) * mix & mask
    if data[end:]:
        value = (value ^ int.from_bytes(data[end:], 'little')) * mix & mask
    value ^= value >> 47
    value = value * mix & mask
    return value ^ (value >> 47)


def make_archive(name, resource):
    offset = (104 + 80 + 15) & ~15
    body = bytearray(offset) + resource
    body += b'\0' * (-len(body) % 16)
    entry = struct.pack('<7Q6I', name, LUA_TYPE, offset, 0, 0, 0, 0, len(resource), 0, 0, 16, 16, 0)
    header = struct.pack('<III20sQQ24s', 0xF0000011, 1, 1, b'', len(body), 0, b'')
    types = struct.pack('<IIQIIII', 0, 0, LUA_TYPE, 1, 0, 16, 16)
    body[:104 + 80] = header + types + entry
    return bytes(body)


def main():
    source = (HERE / 'mod_options.lua').read_bytes()
    assert source.startswith(('-- HD2-Addon: ' + NAME + '\n').encode()), 'the first line must declare ' + NAME
    assert not source.startswith(b'\xef\xbb\xbf') and b'\0' not in source and b'\r' not in source
    source.decode('utf-8')
    data = HERE.parent / 'data'
    data.mkdir(exist_ok=True)
    (data / ARCHIVE).write_bytes(make_archive(resource_hash(NAME), struct.pack('<II', len(source), 2) + source))
    for sidecar in ('.stream', '.gpu_resources'):
        (data / (ARCHIVE + sidecar)).write_bytes(b'')
    print('built', data / ARCHIVE)


if __name__ == '__main__':
    main()
