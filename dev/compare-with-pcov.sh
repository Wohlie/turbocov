#!/bin/sh
# Runs each given PHP file with PCOV and with turbocov and compares the line data.
# Usage: dev/compare-with-pcov.sh <php-version> <file>...
set -eu

version="${1:?PHP version, for example 8.5}"
shift
root="$(cd "$(dirname "$0")/.." && pwd)"
image="turbocov-dev:$version"

docker image inspect "$image" > /dev/null 2>&1 || docker build -q --build-arg "PHP_VERSION=$version" -t "$image" "$root/dev" > /dev/null
[ -f "$root/build/php-$version/turbocov/turbocov.so" ] || "$root/dev/build.sh" "$version" > /dev/null

status=0

for file in "$@"; do
    dir="$(cd "$(dirname "$file")" && pwd)"
    name="$(basename "$file")"
    run="docker run --rm --memory 2g --memory-swap 2g -v $root:/src:ro -v $dir:/code:ro $image php -d opcache.enable_cli=0"
    expected="$($run -d extension=pcov -d pcov.directory=/code /src/dev/collect.php pcov "/code/$name")"
    actual="$($run -d "extension=/src/build/php-$version/turbocov/turbocov.so" -d turbocov.directory=/code /src/dev/collect.php turbocov "/code/$name")"

    if [ "$expected" = "$actual" ]; then
        echo "same      $file"
    else
        echo "DIFFERENT $file"
        echo "  pcov:     $expected"
        echo "  turbocov: $actual"
        status=1
    fi
done

exit $status
