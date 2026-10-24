#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Optional desktop launcher; no autostart or desktop-extension changes."""
from pathlib import Path
import os,shutil,subprocess,sys
from desktop_icon import icon_value
from topoplay_paths import desktop_quote
TAG='X-TopoPlay-Shortcut=true'

def shortcut_path():
    desktop=None
    if shutil.which('xdg-user-dir'):
        r=subprocess.run(['xdg-user-dir','DESKTOP'],capture_output=True,text=True,check=False)
        if r.returncode==0 and r.stdout.strip():desktop=Path(r.stdout.strip()).expanduser()
    if desktop is None:desktop=Path.home()/'Desktop'
    # xdg-user-dir returns HOME when the desktop directory is disabled.
    if not desktop.is_absolute() or desktop.resolve()==Path.home().resolve():return None
    return desktop/'TopoPlay.desktop'

def owned(path):
    return path.is_file() and TAG in path.read_text().splitlines()

def ask(existing=False):
    prompt=('Update the existing TopoPlay desktop shortcut to this installation?' if existing else 'Create a TopoPlay shortcut on your desktop?')
    if os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY'):
        try:
            import tkinter as tk
            from tkinter import messagebox
            win=tk.Tk();win.withdraw()
            try:return messagebox.askyesno('TopoPlay installation',prompt,parent=win)
            finally:win.destroy()
        except ImportError:pass
        except Exception as exc:
            if type(exc).__name__!='TclError':raise
        if shutil.which('zenity'):
            r=subprocess.run(['zenity','--question','--title=TopoPlay installation','--text='+prompt],check=False)
            if r.returncode in (0,1):return r.returncode==0
    if sys.stdin.isatty():return input(prompt+' [y/N]: ').strip().lower() in ('y','yes')
    print('Desktop shortcut skipped (no interactive prompt). Use --desktop-shortcut to request one.')
    return False

def sync_shortcut(target,choice=None):
    if choice is False:return False
    path=shortcut_path()
    if path is None:
        print('Desktop directory is disabled; the application-menu entry remains available.');return False
    if choice is None:choice=ask(owned(path))
    if not choice:return False
    if path.exists() and not owned(path):
        print('Desktop shortcut skipped: '+str(path)+' already exists and is not managed by this installer.');return False
    launcher=Path(target).resolve()/'TopoPlay'
    if not launcher.is_file():raise RuntimeError('TopoPlay launcher is missing: '+str(launcher))
    content='[Desktop Entry]\nType=Application\nName=TopoPlay\nComment=Interactive terrain sandbox\nExec=/usr/bin/env -- '+desktop_quote(launcher)+'\nIcon='+icon_value(target)+'\nTerminal=false\nCategories=Education;Science;\nStartupNotify=false\n'+TAG+'\n'
    path.parent.mkdir(parents=True,exist_ok=True)
    temp=path.with_suffix('.desktop.tmp');temp.write_text(content);temp.chmod(0o755);temp.replace(path)
    trusted=False
    if shutil.which('gio'):
        r=subprocess.run(['gio','set',str(path),'metadata::trusted','true'],capture_output=True,text=True,check=False);trusted=r.returncode==0
    print('Desktop shortcut: '+str(path))
    if not trusted:print('If Ubuntu asks, right-click the shortcut and select Allow Launching.')
    return True

def remove_shortcut(target):
    path=shortcut_path()
    if path and owned(path) and ('Exec=/usr/bin/env -- '+desktop_quote(Path(target).resolve()/'TopoPlay')) in path.read_text().splitlines():
        path.unlink();return True
    return False
