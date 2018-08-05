#!/usr/bin/env bash

cd "$(dirname "$0")"

source ./config

if [[ $# -le 1 ]]; then
    >&2 echo "Usage: $0 <execpath>"
    exit 2
fi

executable=$1
shift 1

if [[ ! -x "$executable" ]]; then
    >&2 echo "Executable \"$executable\" not found or not executable"
    exit 2
fi

socketlib=$DCCS_SOCKET_LIBRARY_PATH
if [[ ! -x "$socketlib" ]]; then
    >&2 echo "Socket library \"$socketlib\" not found or not executable"
    exit 2
fi

set -x
LD_PRELOAD=$socketlib $executable "$@"

