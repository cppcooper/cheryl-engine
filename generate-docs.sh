#!/usr/bin/env bash
set -euo pipefail

script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

if [[ -n "${CHERYL_DOCS_PYTHON:-}" ]]; then
    docs_python="$CHERYL_DOCS_PYTHON"
elif command -v python3 >/dev/null 2>&1; then
    docs_python=python3
elif command -v python >/dev/null 2>&1; then
    docs_python=python
else
    printf '%s\n' 'Documentation generation requires Python 3.11 or newer.' >&2
    exit 127
fi

exec "$docs_python" "$script_directory/docs/site/generation/build.py" "$@"
