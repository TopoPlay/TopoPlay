#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Paths follow the launcher, never a particular home/Downloads layout."""
from pathlib import Path
import json,os,subprocess

def root():return Path(__file__).resolve().parents[1]
def read_build(project):
    p=Path(project)/'topoplay-build.json'
    return json.loads(p.read_text()) if p.is_file() else {}
def executable(project):
    p=Path(project)/'bin/TopoPlay'
    if not p.is_file() or not os.access(p,os.X_OK):raise RuntimeError('TopoPlay executable missing: '+str(p)+'. Build with bash scripts/build.sh first.')
    return p
def environment(project,base=None):
    project=Path(project).resolve();env=dict(os.environ if base is None else base)
    env.pop('WAYLAND_DISPLAY',None);env['XDG_SESSION_TYPE']='x11'
    dirs=[project/'lib',project/'bin',project/'bin/libs']
    explicit=env.get('FREENECT2_LIB_DIR')
    if explicit:dirs.append(Path(explicit).expanduser())
    elif env.get('FREENECT2_BUILD'):dirs.append(Path(env['FREENECT2_BUILD']).expanduser()/'lib')
    elif env.get('FREENECT2_ROOT'):dirs.append(Path(env['FREENECT2_ROOT']).expanduser()/'build/lib')
    else:
        lib=read_build(project).get('freenect2_lib_dir')
        if lib:dirs.append(Path(lib))
    values=[]
    for d in dirs:
        if d.is_dir() and str(d) not in values:values.append(str(d))
    values += [p for p in env.get('LD_LIBRARY_PATH','').split(':') if p and p not in values]
    if values:env['LD_LIBRARY_PATH']=':'.join(values)
    else:env.pop('LD_LIBRARY_PATH',None)
    return env

def registry_path():
    return Path(os.environ.get('XDG_CONFIG_HOME',str(Path.home()/'.config')))/'topoplay/installations.json'
def registrations():
    p=registry_path()
    return json.loads(p.read_text()) if p.is_file() else []
def record_install(project):
    project=str(Path(project).resolve());p=registry_path();items=[x for x in registrations() if x!=project]+[project]
    p.parent.mkdir(parents=True,exist_ok=True);temp=p.with_suffix('.tmp');temp.write_text(json.dumps(items,indent=2)+'\n');temp.replace(p)

def desktop_quote(value):
    # Desktop Entry Exec is not shell text. Escape Exec field codes as well.
    value=str(value)
    if any(ord(c)<32 for c in value):raise RuntimeError('Control characters are not supported in installation paths.')
    escaped=''.join(('\\'+c) if c in '\\"`$' else c for c in value)
    # General Desktop Entry string escaping is decoded before Exec quoting.
    escaped=escaped.replace('\\','\\\\').replace('%','%%')
    return '"'+escaped+'"'
