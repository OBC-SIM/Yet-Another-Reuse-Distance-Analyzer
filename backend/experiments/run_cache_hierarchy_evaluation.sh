#!/bin/sh
set -eu
if [ "$#" -lt 2 ] || [ "$#" -gt 4 ]; then
    echo "usage: sh $0 BUILD_DIR NEW_OUTPUT_DIR [MANIFEST] [REPETITIONS]" >&2
    exit 2
fi
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec cmake "-DYARDA_BUILD_DIR=$1" "-DYARDA_OUTPUT=$2" \
    "-DYARDA_MANIFEST=${3:-$script_dir/cache_hierarchy_manifest.json}" \
    "-DYARDA_REPETITIONS=${4:-10}" -P "$script_dir/helpers/evaluate.cmake"
