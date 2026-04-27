#!/bin/bash

set -euo pipefail

platform="${PLATFORM:-linux}"
exeext=""
if [ "$platform" = "windows" ]; then
    exeext=".exe"
fi

make PLATFORM="$platform" bestiary repl
echo "built ./build/bin/$platform/bestiary$exeext"
echo "built ./build/bin/$platform/bestiary-cli$exeext"

if [ "$platform" = "windows" ]; then
    make PLATFORM="$platform" bundle
    echo "bundled Windows runtime at ./build/dist/windows/bestiary"
fi
