--TEST--
stop() returns the executed lines and get_executable_lines() returns every executable line
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures
--FILE--
<?php
$file = __DIR__ . '/fixtures/functions.php';
require $file;

\turbocov\ensure_can_collect();
var_dump(\turbocov\get_status()['collecting']);

\turbocov\start();
second(2);
$executed = \turbocov\stop();

var_dump(array_keys($executed) === [$file]);

$lines = array_replace(\turbocov\get_executable_lines([$file, __FILE__, 'missing.php'])[$file], $executed[$file]);
ksort($lines);

foreach ($lines as $line => $status) {
    echo $line, ' ', $status === 1 ? 'covered' : 'not covered', "\n";
}

echo json_encode(array_keys(\turbocov\get_executable_lines([$file, __FILE__, 'missing.php'])) === [$file]), "\n";
?>
--EXPECT--
bool(false)
bool(true)
5 covered
6 not covered
9 covered
14 covered
16 covered
17 covered
20 covered
25 not covered
27 not covered
true
