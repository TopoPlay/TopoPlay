#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Read-only checks before building TopoPlay; never installs packages."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

from configure import framework, resolve_dependencies

ROOT = Path(__file__).resolve().parents[1]
ADDONS = {
    'ofxCv': 'https://github.com/kylemcdonald/ofxCv',
    'ofxDatGui': 'https://github.com/thomwolf/ofxDatGui',
    'ofxModal': 'https://github.com/braitsch/ofxModal',
    'ofxParagraph': 'https://github.com/braitsch/ofxParagraph',
}
# These common prerequisites are intentionally not a claim to cover every
# release-specific openFrameworks dependency. Its own install script remains
# authoritative for the complete package list.
LIBRARIES = [
    (('opencv4', 'opencv'), 'OpenCV development files', 'libopencv-dev'),
    (('gl',), 'OpenGL development files', 'libgl1-mesa-dev'),
    (('glu',), 'GLU development files', 'libglu1-mesa-dev'),
    (('glew',), 'GLEW development files', 'libglew-dev'),
    (('glfw3',), 'GLFW development files', 'libglfw3-dev'),
    (('libusb-1.0',), 'USB development files', 'libusb-1.0-0-dev'),
    (('freetype2',), 'FreeType development files', 'libfreetype6-dev'),
    (('harfbuzz',), 'HarfBuzz development files', 'libharfbuzz-dev'),
    (('x11',), 'X11 development files', 'libx11-dev'),
    (('xrandr',), 'Xrandr development files', 'libxrandr-dev'),
    (('xxf86vm',), 'Xxf86vm development files', 'libxxf86vm-dev'),
    (('xi',), 'XInput development files', 'libxi-dev'),
    (('xcursor',), 'Xcursor development files', 'libxcursor-dev'),
    (('xinerama',), 'Xinerama development files', 'libxinerama-dev'),
]


def run(args):
    try:
        return subprocess.run(args, capture_output=True, text=True, timeout=15)
    except (OSError, subprocess.TimeoutExpired):
        return None


def pkg_exists(name):
    result = run(['pkg-config', '--exists', name])
    return result is not None and result.returncode == 0


def check(project, paths):
    project = Path(project)
    errors, notes, packages = [], [], set()
    tools = [('make', 'make'), ('g++', 'g++'), ('pkg-config', 'pkg-config')]
    for command, package in tools:
        if not shutil.which(command):
            errors.append('Missing build tool: ' + command)
            packages.add(package)
    values = None
    try:
        values = resolve_dependencies(project, **paths)
    except (RuntimeError, OSError, ValueError) as exc:
        errors.append(str(exc))
        notes += [
            'openFrameworks: download the Linux 0.12.1 package from https://openframeworks.cc/download/',
            'Kinect v2 driver: follow https://github.com/OpenKinect/libfreenect2#linux',
            'Then pass --of-root /path/to/openFrameworks and, if needed, --freenect2-root /path/to/libfreenect2.',
        ]
    # Even if Kinect discovery failed, report missing addons in an explicitly
    # selected, saved, or ancestor framework rather than hiding them.
    of = Path(values['of_root']) if values else None
    if of is None:
        try:
            saved = json.loads((project/'topoplay-build.json').read_text()) if (project/'topoplay-build.json').is_file() else {}
            requested = paths.get('of_root') or os.environ.get('OF_ROOT') or saved.get('of_root')
            of = Path(requested).expanduser().resolve() if requested else next((p for p in project.parents if framework(p)), None)
        except (OSError, ValueError):
            pass
    if of is not None and framework(of):
        for line in (project/'addons.make').read_text().splitlines():
            name = line.split('#', 1)[0].strip()
            if not name:
                continue
            addon = of/'addons'/name
            if not (addon/'src').is_dir() or not any((addon/'src').rglob('*.*')):
                errors.append('Missing or incomplete addon: ' + name + ' (expected ' + str(addon) + ')')
                if name in ADDONS:
                    notes.append('Obtain ' + name + ' from ' + ADDONS[name] + '; use the recorded revision/patch instructions in dependencies/README.md.')
                else:
                    notes.append(name + ' must come from the matching openFrameworks 0.12.1 package.')
        install = of/'scripts/linux/ubuntu/install_dependencies.sh'
        if install.is_file():
            notes.append('Complete openFrameworks Ubuntu prerequisites: sudo bash ' + shlex.quote(str(install)))
        notes.append('After installing framework prerequisites, run: bash ' + shlex.quote(str(of/'scripts/linux/compileOF.sh')))
    if shutil.which('pkg-config'):
        for alternatives, label, package in LIBRARIES:
            if not any(pkg_exists(name) for name in alternatives):
                errors.append('Missing ' + label + ' (pkg-config: ' + ' or '.join(alternatives) + ')')
                packages.add(package)
    else:
        notes.append('System-library checks could not run without pkg-config; install it and rerun this check.')
    if values and 'freenect2_lib_dir' in values:
        libraries = sorted(Path(values['freenect2_lib_dir']).glob('libfreenect2.so*'))
        if libraries and shutil.which('ldd'):
            result = run(['ldd', str(libraries[0])])
            if result and 'not found' in result.stdout:
                missing = [line.strip() for line in result.stdout.splitlines() if 'not found' in line]
                errors.append('libfreenect2 has missing runtime libraries: ' + '; '.join(missing))
                notes.append('Install the libfreenect2 runtime prerequisites from its Linux guide, then rerun this check.')
    print('TopoPlay build dependency check')
    if errors:
        for error in errors:
            print('MISSING / NEEDS ATTENTION: ' + error)
        if packages:
            print('\nUbuntu/Debian package command (review before running):')
            print('  sudo apt update && sudo apt install ' + ' '.join(sorted(packages)))
        for note in dict.fromkeys(notes):
            print('  ' + note)
        print('\nSTOP: dependencies need attention. No build started and no packages were installed.')
        print('Full instructions: docs/BUILD_LINUX.md and dependencies/README.md')
        return 1
    print('PASS: common build tools, framework/Kinect paths, six addons, and common development libraries detected.')
    print('This is not a clean-machine certification or a graphics/Kinect hardware test.')
    print('Run the framework dependency/compile scripts if later build errors name additional libraries.')
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('of-root', 'freenect2-root', 'freenect2-build', 'freenect2-lib-dir'):
        parser.add_argument('--' + name)
    args = parser.parse_args()
    paths = dict(of_root=args.of_root, freenect_root=args.freenect2_root,
                 freenect_build=args.freenect2_build, freenect_lib=args.freenect2_lib_dir)
    return check(ROOT, paths)


if __name__ == '__main__':
    sys.exit(main())
