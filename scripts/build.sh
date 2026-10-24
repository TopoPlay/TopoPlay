#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
/usr/bin/python3 scripts/configure.py "$@"
make -j2 APPNAME=TopoPlay
[ -x bin/TopoPlay ] || { echo 'TopoPlay build did not produce bin/TopoPlay.' >&2; exit 1; }
