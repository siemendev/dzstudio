#!/usr/bin/env python3
"""Packs a DayZ mod folder into a PBO and signs it, like AddonBuilder -packonly and DSSignFile do. No dependencies.

  dzpbo.py pack SRC OUT.pbo [--prefix P] [--exclude NAME ...]
  dzpbo.py sign KEY.biprivatekey FILE.pbo        writes FILE.pbo.<authority>.bisign (version 3)
  dzpbo.py verify KEY.bikey FILE.pbo [SIG.bisign]
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

# Extensions whose contents go into the third signature hash: the Arma 3 list plus Enforce Script.
HASHED = {'bikb', 'c', 'cfg', 'ext', 'fsm', 'h', 'hpp', 'inc', 'sqf', 'sqfc', 'sqm', 'sqs'}


def pack(src: Path, out: Path, prefix: str | None, exclude: set[str]) -> None:
    if prefix is None:
        pf = src / '$PBOPREFIX$'
        prefix = pf.read_text().strip() if pf.is_file() else src.name
    files = []
    for p in src.rglob('*'):
        if p.is_file() and not (set(p.relative_to(src).parts) & exclude):
            files.append(('\\'.join(p.relative_to(src).parts), p))
    files.sort(key=lambda f: f[0].lower())
    head = bytearray(b'\0' + struct.pack('<5I', 0x56657273, 0, 0, 0, 0))
    for k, v in (('product', 'dayz ugc'), ('prefix', prefix)):
        head += k.encode() + b'\0' + v.encode() + b'\0'
    head += b'\0'
    blobs = []
    for name, p in files:
        data = p.read_bytes()
        head += name.encode() + b'\0' + struct.pack('<5I', 0, 0, 0, 0, len(data))
        blobs.append(data)
    head += b'\0' + struct.pack('<5I', 0, 0, 0, 0, 0)
    sha = hashlib.sha1(head)
    with out.open('wb') as f:
        f.write(head)
        for b in blobs:
            sha.update(b)
            f.write(b)
        f.write(b'\0' + sha.digest())
    print(f'{out}: {len(files)} files, prefix {prefix}')


def read_pbo(path: Path):
    d = path.read_bytes()
    o = 0

    def cstr() -> bytes:
        nonlocal o
        e = d.index(b'\0', o)
        s = d[o:e]
        o = e + 1
        return s

    props, ents = {}, []
    while True:
        name = cstr()
        mime, _orig, _res, _ts, size = struct.unpack_from('<5I', d, o)
        o += 20
        if name == b'' and mime == 0x56657273:
            while (k := cstr()):
                props[k] = cstr()
            continue
        if name == b'':
            break
        if mime != 0:
            sys.exit(f'{path}: compressed or unknown entry {name!r}')
        ents.append([name, size, 0])
    for e in ents:
        e[2] = o
        o += e[1]
    if hashlib.sha1(d[:o]).digest() != d[o + 1:o + 21]:
        sys.exit(f'{path}: checksum does not match')
    return d, ents, props, d[o + 1:o + 21]


def hashes(path: Path) -> list[bytes]:
    d, ents, props, h1 = read_pbo(path)
    ents.sort(key=lambda e: e[0].lower())
    names = hashlib.sha1(b''.join(e[0].replace(b'/', b'\\').lower() for e in ents if e[1] > 0)).digest()
    prefix = props.get(b'prefix', b'')
    if prefix and not prefix.endswith(b'\\'):
        prefix += b'\\'
    files = hashlib.sha1()
    found = False
    for name, size, off in ents:
        base = name.split(b'\\')[-1]
        if b'.' in base and base.rsplit(b'.', 1)[1].lower().decode() in HASHED:
            found = True
            files.update(d[off:off + size])
    if not found:
        files.update(b'gnihton')
    return [h1, hashlib.sha1(h1 + names + prefix).digest(), hashlib.sha1(files.digest() + names + prefix).digest()]


def pad(h: bytes, size: int) -> int:
    return int.from_bytes(b'\0\1' + b'\xff' * (size - 38) + bytes.fromhex('003021300906052b0e03021a05000414') + h, 'big')


def read_key(path: Path, private: bool):
    b = path.read_bytes()
    o = b.index(b'\0')
    authority = b[:o].decode()
    o += 1 + 4 + 8
    magic = b[o:o + 4]
    if magic != (b'RSA2' if private else b'RSA1'):
        sys.exit(f'{path}: not a {"private" if private else "public"} key')
    bits, e = struct.unpack_from('<II', b, o + 4)
    o += 12
    n = int.from_bytes(b[o:o + bits // 8], 'little')
    o += bits // 8
    if private:
        o += 5 * (bits // 16)
        return authority, bits, e, n, int.from_bytes(b[o:o + bits // 8], 'little')
    return authority, bits, e, n, None


def sign(key: Path, pbo: Path) -> None:
    authority, bits, e, n, priv = read_key(key, True)
    size = bits // 8
    sigs = [pow(pad(h, size), priv, n).to_bytes(size, 'little') for h in hashes(pbo)]
    out = bytearray(authority.encode() + b'\0' + struct.pack('<I', 20 + size))
    out += b'\x06\x02\x00\x00\x00\x24\x00\x00RSA1' + struct.pack('<II', bits, e) + n.to_bytes(size, 'little')
    out += struct.pack('<I', size) + sigs[0] + struct.pack('<I', 3)
    out += struct.pack('<I', size) + sigs[1] + struct.pack('<I', size) + sigs[2]
    dest = pbo.with_name(f'{pbo.name}.{authority}.bisign')
    dest.write_bytes(out)
    print(dest)


def verify(key: Path, pbo: Path, sig: Path | None) -> None:
    authority, bits, e, n, _ = read_key(key, False)
    sig = sig or pbo.with_name(f'{pbo.name}.{authority}.bisign')
    b = sig.read_bytes()
    o = b.index(b'\0') + 1 + 4 + 20 + bits // 8
    got = []
    for i in range(3):
        length = struct.unpack_from('<I', b, o)[0]
        got.append(int.from_bytes(b[o + 4:o + 4 + length], 'little'))
        o += 4 + length + (4 if i == 0 else 0)
    want = [pad(h, bits // 8) for h in hashes(pbo)]
    if [pow(s, e, n) for s in got] != want:
        sys.exit(f'{sig}: signature does not match {pbo} and {key}')
    print(f'{sig}: ok')


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('pack')
    p.add_argument('src', type=Path)
    p.add_argument('out', type=Path)
    p.add_argument('--prefix')
    p.add_argument('--exclude', nargs='*', default=[])
    s = sub.add_parser('sign')
    s.add_argument('key', type=Path)
    s.add_argument('pbo', type=Path)
    v = sub.add_parser('verify')
    v.add_argument('key', type=Path)
    v.add_argument('pbo', type=Path)
    v.add_argument('sig', type=Path, nargs='?')
    a = ap.parse_args()
    if a.cmd == 'pack':
        pack(a.src, a.out, a.prefix, set(a.exclude))
    elif a.cmd == 'sign':
        sign(a.key, a.pbo)
    else:
        verify(a.key, a.pbo, a.sig)


if __name__ == '__main__':
    main()
