#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
echo "Running Doxygen…"
doxygen Doxyfile