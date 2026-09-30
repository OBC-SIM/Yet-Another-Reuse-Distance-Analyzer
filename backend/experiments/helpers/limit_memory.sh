#!/bin/sh
set -eu
# Linux virtual-address-space limit; the measurement still reports resident RSS.
ulimit -v "$1"
shift
exec "$@"
