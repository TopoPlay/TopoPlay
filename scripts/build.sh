#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
command -v python3 >/dev/null 2>&1 || {
  echo "STOP: Python 3 is missing. On Ubuntu/Debian run: sudo apt update && sudo apt install python3" >&2
  exit 1
}
python3 scripts/check-dependencies.py "$@"
python3 scripts/configure.py "$@"
make -j2 APPNAME=TopoPlay
[ -x bin/TopoPlay ] || { echo 'TopoPlay build did not produce bin/TopoPlay.' >&2; exit 1; }
