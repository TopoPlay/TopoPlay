#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""TopoPlay addition — 2026-10-24: stable user-level launcher icon."""
from pathlib import Path
import os,shutil,tempfile

def icon_path():
    return Path(os.environ.get('XDG_DATA_HOME',str(Path.home()/'.local/share')))/'icons/topoplay.png'

def desktop_string(value):
    return str(value).replace('\\','\\\\').replace('\n','\\n').replace('\r','\\r').replace('\t','\\t')

def install_icon(source):
    source=Path(source);target=icon_path();target.parent.mkdir(parents=True,exist_ok=True)
    fd,tmp=tempfile.mkstemp(prefix='.topoplay-icon-',suffix='.png',dir=target.parent);os.close(fd)
    temporary=Path(tmp)
    try:shutil.copyfile(source,temporary);temporary.chmod(0o644);temporary.replace(target)
    finally:temporary.unlink(missing_ok=True)
    return target

def icon_value(target):
    source=Path(target)/'bin/data/icons/topoplay.png'
    if source.is_file():install_icon(source)
    icon=icon_path()
    return desktop_string(icon) if icon.is_file() else 'applications-education'
