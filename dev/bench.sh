#!/bin/sh
# Compares plain PHP, PCOV and turbocov on dev/bench.php. Prints the best of five runs in milliseconds.
# Usage: dev/bench.sh <php-version>
set -eu

version="${1:?PHP version, for example 8.5}"
root="$(cd "$(dirname "$0")/.." && pwd)"
image="turbocov-dev:$version"
so="/src/build/php-$version/turbocov/turbocov.so"

docker image inspect "$image" > /dev/null 2>&1 || docker build -q --build-arg "PHP_VERSION=$version" -t "$image" "$root/dev" > /dev/null
"$root/dev/build.sh" "$version" > /dev/null

docker run --rm --memory 2g --memory-swap 2g -v "$root:/src:ro" "$image" sh -euc "
    best() { for i in 1 2 3 4 5; do \"\$@\"; done | sort -n | head -1; }
    php='php -d opcache.enable_cli=0'
    printf '%-34s %8s %8s %8s\n' '' inline calls tests
    row() {
        label=\"\$1\"; shift
        printf '%-34s' \"\$label\"
        for kind in inline calls tests; do printf ' %8s' \"\$(best \"\$@\" \$kind)\"; done
        printf '\n'
    }
    row 'plain PHP'                      \$php /src/dev/bench.php none
    row 'PCOV, collecting'               \$php -d extension=pcov -d pcov.directory=/src/dev /src/dev/bench.php pcov
    row 'PCOV, file not listed'          \$php -d extension=pcov -d pcov.directory=/tmp /src/dev/bench.php pcov
    row 'turbocov, collecting'           \$php -d extension=$so -d turbocov.collect_paths=/src/dev /src/dev/bench.php turbocov
    row 'turbocov, file not listed'      \$php -d extension=$so -d turbocov.collect_paths=/tmp /src/dev/bench.php turbocov
"
