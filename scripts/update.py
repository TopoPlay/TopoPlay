#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Install an extracted TopoPlay release into a recorded installation folder."""
from pathlib import Path
import argparse,sys
from install import install,choose_destination
from topoplay_paths import root,registrations

def main():
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('--destination',type=Path);args=a.parse_args()
    targets=[Path(p) for p in registrations() if (Path(p)/'topoplay-install.json').is_file()]
    target=args.destination or (targets[0] if len(targets)==1 else choose_destination())
    install(root(),target)
if __name__=='__main__':
    try:main()
    except (RuntimeError,OSError) as e:sys.exit('STOP: '+str(e))
