#!/usr/bin/env python3
"""Prints the current Steam Guard code for a mobile authenticator's shared_secret (base64), read from
STEAM_SHARED_SECRET. Optional argument: a Unix time instead of now."""
import base64
import hashlib
import hmac
import os
import struct
import sys
import time

ALPHABET = '23456789BCDFGHJKMNPQRTVWXY'


def code(secret: str, at: int) -> str:
    mac = hmac.new(base64.b64decode(secret), struct.pack('>Q', at // 30), hashlib.sha1).digest()
    start = mac[19] & 0x0F
    value = struct.unpack('>I', mac[start:start + 4])[0] & 0x7FFFFFFF
    out = ''
    for _ in range(5):
        out += ALPHABET[value % len(ALPHABET)]
        value //= len(ALPHABET)
    return out


if __name__ == '__main__':
    secret = os.environ.get('STEAM_SHARED_SECRET') or sys.exit('STEAM_SHARED_SECRET is not set')
    print(code(secret, int(sys.argv[1]) if len(sys.argv) > 1 else int(time.time())))
