# turbocov

Line coverage driver for PHP. Covered code runs at the speed of plain PHP.

- It reports the same line data as [PCOV](https://github.com/krakjoe/pcov).
- It has the same functions and INI settings as PCOV.
- Files outside of the whitelist have no cost at run time.

Status: experimental. Read [Limits](#limits) before you use it.

## How it works

PCOV and Xdebug do work for each instruction that PHP executes. turbocov does work only when a line runs for the first time.

1. PHP compiles a file into opcodes. Each opcode has a pointer to its handler.
2. For a whitelisted file, turbocov replaces the handler of the first opcode of each line with a probe.
3. When a probe fires, it records the line and puts the original handler back. PHP then executes the same opcode again.
4. From that moment the line runs without a probe.
5. `clear()` arms the fired probes again. The cost of a test run is in proportion to the number of lines that the test touched.

turbocov does not replace the instruction loop of PHP. Code without probes runs as if the extension is not loaded.

## Performance

![Time that a coverage driver adds to a test suite](performance.svg)

The test suite of a PHPStan extension (870 tests, PHPUnit 10.5, PHP 8.5, OPcache off), mean of five runs:

| Driver | Suite time | Added time |
|---|---|---|
| No coverage | 8.7 s | |
| turbocov | 9.7 s | +1.0 s (+12 %) |
| PCOV 1.0.12 | 20.8 s | +12.1 s (+140 %) |
| Xdebug, coverage mode | 47.4 s | +38.8 s (+447 %) |

- turbocov adds 12 times less time than PCOV and 37 times less time than Xdebug.
- The clover report of turbocov is identical to the report of PCOV.
- turbocov itself uses 0.08 s of the added 1.0 s: 1.6 million probe hits, the API calls and the compile hook. The coverage library of PHPUnit uses the rest, with each driver.

## Requirements

- PHP 8.4 or 8.5, non-thread-safe build.
- Linux or macOS.
- The CLI SAPI. OPcache can be on or off.

## Build

```sh
phpize
./configure --enable-turbocov
make
make test
```

Load the result with `extension=/path/to/modules/turbocov.so`.

## Settings

| Setting | Default | Description |
|---|---|---|
| `turbocov.enabled` | `1` | Set `0` to turn the extension off. It then has no effect at all. |
| `turbocov.directory` | automatic | Only files below this directory get probes. The default is the first of `src`, `lib`, `app` that exists, else the working directory. |
| `turbocov.exclude` | none | A PCRE pattern. Files with a matching path get no probes. |

The settings apply when PHP compiles a file. Set them in `php.ini` or with `-d`, not with `ini_set()`.

## Functions

```php
\turbocov\start();                 // start the collection
\turbocov\stop();                  // stop the collection
\turbocov\waiting();               // files that have new data
\turbocov\collect(                 // file => line => 1 (executed) or -1 (not executed)
    \turbocov\inclusive,           // or \turbocov\exclusive, \turbocov\all
    ['/absolute/path/File.php'],
);
\turbocov\clear();                 // drop the data, arm the probes again
```

## Use with PHPUnit

PHPUnit selects its PCOV driver when an extension with the name `pcov` is loaded. Build turbocov with that name:

```sh
./configure --enable-turbocov --enable-turbocov-impersonate-pcov
```

The extension then registers as `pcov`. The functions are `\pcov\start()` and so on, and the settings are `pcov.enabled`, `pcov.directory` and `pcov.exclude`. PHPUnit and all other tools that support PCOV work without a change. Do not load the real PCOV at the same time.

## Limits

- OPcache: turbocov puts its probes into the code that OPcache caches. This is not possible with `opcache.protect_memory=1`, with `opcache.file_cache`, with the JIT, and in a SAPI other than the CLI. In these cases turbocov turns the collection off, and the first call of `start()` gives a warning.
- OPcache optimizer: the optimizer can remove code, and a line without code is not executable. PCOV has the same behavior. Set `opcache.optimization_level=0` for line data that does not depend on the optimizer.
- Other extensions that replace the instruction loop (PCOV, phpdbg) make turbocov slow. The results stay correct.
- Xdebug: not tested together.
- No branch coverage, no path coverage, no detection of dead code.
- `clear(true)` does not drop the file list. The argument has no effect.

## Development

The scripts in `dev/` need Docker. They limit each container to 2 GB of memory.

```sh
RUN_TESTS=1 dev/build.sh 8.5                 # build and run the tests
dev/build.sh 8.5 pcov                        # build with the name pcov
dev/compare-with-pcov.sh 8.5 path/File.php   # compare the line data with PCOV
dev/bench.sh 8.5                             # compare the speed with plain PHP and PCOV
```
