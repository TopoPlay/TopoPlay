#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Create a complete local TopoPlay application/source bundle for Linux."""
from pathlib import Path
import argparse,hashlib,json,platform,shutil,sys,tarfile,tempfile
from install import make_tree,VERSION
from topoplay_paths import root
PRIVATE=('bin/data/settings/calibration.xml','bin/data/settings/kinectProjectorSettings.xml','bin/data/settings/box-startup.json','bin/data/projector-color.json','bin/data/cpu-limit.json')
def main():
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('--output',type=Path,required=True);a.add_argument('--include-box-settings',action='store_true',help='For a private backup on the same sandbox only.');args=a.parse_args();source=root();output=args.output.expanduser().resolve()
    if output.exists():raise RuntimeError('Output already exists; choose a new filename.')
    with tempfile.TemporaryDirectory(prefix='topoplay-release-') as temp:
        tree=Path(temp)/'TopoPlay';tree.mkdir();make_tree(source,tree)
        if not args.include_box_settings:
            for name in PRIVATE:(tree/name).unlink(missing_ok=True)
        for pattern in ('*.log','perf*.data*'):
            for p in (tree/'bin/data').glob(pattern):p.unlink(missing_ok=True)
        (tree/'RELEASE.json').write_text(json.dumps({'product':'TopoPlay','version':VERSION,'platform':'Linux','architecture':platform.machine(),'private_box_settings':args.include_box_settings,'libc':platform.libc_ver(),'build_system':platform.freedesktop_os_release() if hasattr(platform,'freedesktop_os_release') else {}},indent=2)+'\n')
        hashes={p.relative_to(tree).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(tree.rglob('*')) if p.is_file() and p.name!='RELEASE-SHA256.json'}
        (tree/'RELEASE-SHA256.json').write_text(json.dumps({'files':hashes},indent=2)+'\n')
        output.parent.mkdir(parents=True,exist_ok=True)
        with tarfile.open(output,'w:gz') as archive:archive.add(tree,arcname='TopoPlay')
    print('Created '+str(output))
    print('Test this bundle on a clean matching Linux system before publishing. System drivers/USB permissions and system-library ABI remain prerequisites.')
if __name__=='__main__':
    try:main()
    except (RuntimeError,OSError) as e:sys.exit('STOP: '+str(e))
