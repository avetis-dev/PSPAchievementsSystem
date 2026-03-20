#!/usr/bin/env python3
"""
Create game_map.dat binary file.

Usage:
    python3 make_game_map.py <output.dat>

Edit the GAMES list below to add more games.
"""

import struct, sys

MAX_CODE = 16
MAX_FILE = 32

GAMES = [
    ("ULUS10285", 3927, "3927.ach"),   # SH Origins USA
    ("ULES00869", 3927, "3927.ach"),   # SH Origins EUR
]

def pad(s, length):
    b = s.encode('utf-8')[:length - 1]
    return b + b'\x00' * (length - len(b))

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "game_map.dat"

    with open(out, 'wb') as f:
        f.write(struct.pack('<i', len(GAMES)))
        for code, gid, ach in GAMES:
            f.write(pad(code, MAX_CODE))
            f.write(struct.pack('<i', gid))
            f.write(pad(ach, MAX_FILE))

    print(f"Wrote {out} with {len(GAMES)} entries")

if __name__ == '__main__':
    main()
