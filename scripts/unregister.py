#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Remove this install's menu/registry entry; leave files and settings intact."""
from pathlib import Path
import json,sys
from topoplay_paths import root,registry_path,registrations,desktop_quote
from install import desktop_path
from desktop_shortcut import remove_shortcut

def main():
    app=root();p=desktop_path()
    if p.is_file() and ('Exec=/usr/bin/env -- '+desktop_quote(app/'TopoPlay')) in p.read_text():p.unlink()
    remove_shortcut(app)
    p=registry_path()
    if p.is_file():p.write_text(json.dumps([x for x in registrations() if Path(x).resolve()!=app],indent=2)+'\n')
    print('TopoPlay menu registration removed for '+str(app))
    print('Application files and calibration are retained. Back up bin/data before manually deleting an installation folder.')
if __name__=='__main__':main()
