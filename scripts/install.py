#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Install or update TopoPlay in a user-selected folder; never enable startup."""
from pathlib import Path
from datetime import datetime
import argparse,hashlib,json,os,re,shutil,subprocess,sys,tempfile
from desktop_shortcut import sync_shortcut
from topoplay_paths import root,executable,environment,record_install,desktop_quote,registrations
VERSION='1.0.1'
MARKER='topoplay-install.json'
MUTABLE=('bin/data/settings','bin/data/colorMaps','bin/data/projector-color.json','bin/data/cpu-limit.json')

def choose_destination():
    initial=Path.home()/'Applications'
    registered=[Path(p) for p in registrations() if Path(p).is_dir()]
    if registered:initial=registered[-1].parent
    parent=None
    try:
        import tkinter as tk
        from tkinter import filedialog
        window=tk.Tk();window.withdraw()
        try:parent=filedialog.askdirectory(title='Choose the folder that will contain TopoPlay',initialdir=str(initial if initial.exists() else Path.home()))
        finally:window.destroy()
        if not parent:raise SystemExit('Installation cancelled.')
    except (ImportError,RuntimeError):pass
    except Exception as e:
        # A headless shell can have tkinter installed but no display server.
        if type(e).__name__!='TclError':raise
    if parent is None and shutil.which('zenity') and (os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')):
        result=subprocess.run(['zenity','--file-selection','--directory','--title=Choose the folder that will contain TopoPlay'],capture_output=True,text=True)
        if result.returncode:raise SystemExit('Installation cancelled.')
        parent=result.stdout.strip()
    if parent is None:
        if not sys.stdin.isatty():raise RuntimeError('No folder picker available. Use --destination /your/chosen/TopoPlay.')
        answer=input('Install TopoPlay in which full folder? ['+str(initial/'TopoPlay')+']: ').strip()
        return Path(answer).expanduser() if answer else initial/'TopoPlay'
    return Path(parent)/'TopoPlay'

def copy_item(source,target):
    if source.is_dir():shutil.copytree(source,target,dirs_exist_ok=True,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
    elif source.is_file():target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)

def dependencies(exe,project):
    result=subprocess.run(['ldd',str(exe)],env=environment(project),capture_output=True,text=True)
    if result.returncode:raise RuntimeError('Cannot inspect Linux runtime dependencies: '+result.stderr.strip())
    missing=re.findall(r'^\s*(\S+)\s+=>\s+not found',result.stdout,re.M)
    if missing:raise RuntimeError('Missing runtime libraries: '+', '.join(missing)+'. Install matching system dependencies or specify FREENECT2_LIB_DIR.')
    return [(name,Path(path)) for name,path in re.findall(r'^\s*(\S+)\s+=>\s+(/.+?)\s+\(0x[0-9a-fA-F]+\)',result.stdout,re.M)]

def make_tree(source,stage):
    source=Path(source).resolve();exe=executable(source)
    if not (source/'bin/data').is_dir():raise RuntimeError('Runtime data folder missing: '+str(source/'bin/data'))
    (stage/'bin').mkdir(parents=True,exist_ok=True);shutil.copy2(exe,stage/'bin/TopoPlay');(stage/'bin/TopoPlay').chmod(0o755)
    for folder in ('src','scripts','docs','licenses','dependencies','bin/data'):
        copy_item(source/folder,stage/folder)
    for name in ('README.md','CREDITS.md','ASSETS.md','COPYING','CHANGELOG.md','Makefile','config.make','addons.make','TopoPlay','RELEASE.json','RELEASE-SHA256.json'):
        copy_item(source/name,stage/name)
    for folder in ('lib','bin/libs'):
        copy_item(source/folder,stage/folder)
    (stage/'lib').mkdir(exist_ok=True)
    for p in (source/'bin').glob('*.so*'):
        if p.is_file():shutil.copy2(p,stage/'lib'/p.name)
    for name,path in dependencies(exe,source):
        # Bundle project/private libraries, including freenect2, but not glibc,
        # the graphics driver or the rest of the operating system's libraries.
        system=any(str(path).startswith(prefix) for prefix in ('/lib/','/lib64/','/usr/lib/','/usr/lib64/'))
        if not system or name.startswith('libfreenect2'):shutil.copy2(path,stage/'lib'/name)
    for name in ('TopoPlay','scripts/run.sh','scripts/build.sh'):
        p=stage/name
        if p.is_file():p.chmod(0o755)
    dependencies(stage/'bin/TopoPlay',stage)
    return stage

def desktop_path():
    return Path(os.environ.get('XDG_DATA_HOME',str(Path.home()/'.local/share')))/'applications/topoplay.desktop'
def register(target,desktop=None):
    p=desktop_path();p.parent.mkdir(parents=True,exist_ok=True)
    text='[Desktop Entry]\nType=Application\nName=TopoPlay\nComment=Interactive terrain sandbox\nExec=/usr/bin/env -- '+desktop_quote(target/'TopoPlay')+'\nIcon=applications-education\nTerminal=false\nCategories=Education;Science;\nStartupNotify=false\n'
    tmp=p.with_suffix('.tmp');tmp.write_text(text);tmp.replace(p);record_install(target)
    if shutil.which('update-desktop-database'):subprocess.run(['update-desktop-database',str(p.parent)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=False)
    # Desktop integration is optional; a trust/desktop-service issue must not
    # undo a successful menu registration or installation.
    try:sync_shortcut(target,desktop)
    except (OSError,RuntimeError) as exc:print('Desktop shortcut could not be created: '+str(exc))

def running_from(directory):
    directory=Path(directory).resolve()/'bin'
    for p in Path('/proc').glob('[0-9]*/exe'):
        try:
            exe=p.resolve(strict=True)
            if exe.parent==directory and (exe.name.startswith('Magic') or exe.name=='TopoPlay'):return True
        except (OSError,RuntimeError):pass
    return False

def install(source,destination,menu=True,desktop=None):
    source=Path(source).resolve();target=Path(destination).expanduser().resolve()
    if running_from(target):raise RuntimeError('Close TopoPlay in the destination folder before updating it.')
    desktop_quote(target) # reject control characters before touching any files
    if ':' in str(target):raise RuntimeError('Choose an installation path without colons; Linux library search paths use colon separators.')
    if target==source or source in target.parents or target in source.parents:raise RuntimeError('Choose an installation folder outside the source/bundle folder.')
    if target.exists() and (not target.is_dir() or (any(target.iterdir()) and not (target/MARKER).is_file())):raise RuntimeError('Destination is not an empty folder or an existing TopoPlay installation. Choose a new TopoPlay subfolder.')
    if (target/MARKER).is_file():
        marker=json.loads((target/MARKER).read_text())
        if marker.get('product')!='TopoPlay':raise RuntimeError('Destination marker does not identify TopoPlay.')
    target.parent.mkdir(parents=True,exist_ok=True)
    stage=Path(tempfile.mkdtemp(prefix='.topoplay-install-',dir=target.parent));backup=None
    try:
        make_tree(source,stage)
        # New binaries/assets can be updated; the user's calibration, chosen
        # terrain profiles, projector colors and performance settings win.
        if target.exists():
            for name in MUTABLE:copy_item(target/name,stage/name)
        marker={'product':'TopoPlay','version':VERSION,'installed_at':datetime.now().isoformat(),'schema':1}
        (stage/MARKER).write_text(json.dumps(marker,indent=2)+'\n')
        if target.exists():
            backup=target.with_name(target.name+'.backup-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'));target.rename(backup)
        stage.rename(target)
        try:
            if menu:register(target,desktop)
        except BaseException:
            # Leave the complete new app available if menu integration fails;
            # report it and retain the previous directory backup for recovery.
            print('TopoPlay files installed, but menu registration failed. Launch '+str(target/'TopoPlay'),file=sys.stderr)
            raise
        print('TopoPlay installed: '+str(target))
        print('Launch: '+str(target/'TopoPlay'))
        if backup:print('Previous installation retained: '+str(backup))
    finally:
        if stage.exists():shutil.rmtree(stage)
    return target

def main():
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('--destination',type=Path);a.add_argument('--source',type=Path,default=root());a.add_argument('--no-menu',action='store_true');a.add_argument('--register-only',action='store_true');choice=a.add_mutually_exclusive_group();choice.add_argument('--desktop-shortcut',dest='desktop',action='store_true',default=None);choice.add_argument('--no-desktop-shortcut',dest='desktop',action='store_false');args=a.parse_args()
    if args.no_menu and args.desktop is True:a.error('--desktop-shortcut cannot be combined with --no-menu')
    if args.register_only:
        target=args.source.expanduser().resolve()
        if not (target/MARKER).is_file():raise RuntimeError('TopoPlay installation marker missing.')
        register(target,args.desktop);print('TopoPlay menu entry updated: '+str(target));return
    install(args.source,args.destination or choose_destination(),not args.no_menu,args.desktop)
if __name__=='__main__':
    try:main()
    except (RuntimeError,OSError,ValueError) as e:sys.exit('STOP: '+str(e))
