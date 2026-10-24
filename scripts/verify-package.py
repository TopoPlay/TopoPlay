#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Verify immutable package files, allowing the operator to save box settings."""
from pathlib import Path
import argparse,hashlib,json,sys
root=Path(__file__).resolve().parents[1]
a=argparse.ArgumentParser(description=__doc__);a.add_argument('--compare-working',type=Path);args=a.parse_args()
p=next((root/n for n in ('RELEASE-SHA256.json','PACKAGE-SHA256.json') if (root/n).is_file()),None)
if p is None:sys.exit('No package checksum manifest found.')
records=json.loads(p.read_text())['files']
def mutable(n):
    return n.startswith(('bin/data/settings/','bin/data/colorMaps/')) or n in ('bin/data/projector-color.json','bin/data/cpu-limit.json','config.local.make','topoplay-build.json')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else None
checked={n:h for n,h in records.items() if not mutable(n)}
failed=[n for n,h in checked.items() if sha(root/n)!=h]
if failed:sys.exit('Package files differ:\n'+'\n'.join(failed[:30]))
print(f'PASS: {len(checked)} immutable files match; saved settings/terrain maps are allowed to change.')
if args.compare_working:
    work=args.compare_working.expanduser().resolve()
    paths={n:h for n,h in checked.items() if n.startswith(('src/','bin/data/shaders/'))}
    failed=[n for n,h in paths.items() if sha(work/n)!=h]
    if failed:sys.exit('Working source differs:\n'+'\n'.join(failed[:30]))
    print('PASS: recorded source and shader files match the working folder.')
