#!/usr/bin/env python3
import json, struct, sys, os

PACH_VERSION = 2
MAX_TITLE = 48
MAX_DESC = 96
MAX_CODE = 16
MAX_LOGIC = 852
MAGIC = b'PACH'

def pad(s, length):
    b = s.encode('utf-8')[:length - 1]
    return b + b'\x00' * (length - len(b))

def main():
    if len(sys.argv) != 5:
        print("Usage: dump_to_ach.py <input.jsonl> <game_code> <game_id> <output.ach>")
        sys.exit(1)
    inp, code, gid, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
    achs = []
    with open(inp) as f:
        for line in f:
            line = line.strip()
            if not line: continue
            obj = json.loads(line)
            if obj.get('category', 3) != 3: continue
            achs.append(obj)
    print(f"Game: {code} (ID {gid}), {len(achs)} achievements")
    with open(out, 'wb') as f:
        f.write(MAGIC)
        f.write(struct.pack('<i', PACH_VERSION))
        f.write(struct.pack('<i', gid))
        f.write(pad(code, MAX_CODE))
        f.write(struct.pack('<i', len(achs)))
        for a in achs:
            f.write(struct.pack('<i', a['id']))
            f.write(pad(a.get('title',''), MAX_TITLE))
            f.write(pad(a.get('desc',''), MAX_DESC))
            f.write(struct.pack('<i', a.get('points',0)))
            f.write(pad(a.get('definition',''), MAX_LOGIC))
    print(f"Wrote {out} ({os.path.getsize(out)} bytes)")

if __name__ == '__main__':
    main()