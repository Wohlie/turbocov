#!/bin/sh
# Builds turbocov in the official PHP image.
# Usage: dev/build.sh <php-version> [turbocov|pcov]
# Output: build/php-<php-version>/<name>/turbocov.so
set -eu

version="${1:?PHP version, for example 8.5}"
name="${2:-turbocov}"
root="$(cd "$(dirname "$0")/.." && pwd)"
out="build/php-$version/$name"
flags="--enable-turbocov"

if [ "$name" = "pcov" ]; then
    flags="$flags --enable-turbocov-as-pcov"
fi

mkdir -p "$root/$out"

docker run --rm --memory 2g --memory-swap 2g -u "$(id -u):$(id -g)" -e "RUN_TESTS=${RUN_TESTS:-0}" -v "$root:/src" "php:$version-cli" sh -euc "
    mkdir /tmp/build
    cd /src
    cp -r config.m4 php_turbocov.h turbocov.c tests /tmp/build/
    cd /tmp/build
    phpize > /dev/null
    ./configure $flags > /dev/null
    make -j\"\$(nproc)\" CFLAGS='-O2 -g -Wall -Wextra -Wno-unused-parameter' 2>&1 | grep -E 'warning|error' || true
    cp modules/turbocov.so /src/$out/turbocov.so
    if [ \"\${RUN_TESTS:-0}\" = 1 ] && [ '$name' = turbocov ]; then
        NO_INTERACTION=1 REPORT_EXIT_STATUS=1 make test TESTS='--show-diff tests'
    fi
"

echo "$out/turbocov.so"
