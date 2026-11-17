--TEST--
collect() reports every executable line and marks the executed lines
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures
--FILE--
<?php
$file = __DIR__ . '/fixtures/functions.php';
require $file;

\turbocov\start();
second(2);
\turbocov\stop();

var_dump(\turbocov\waiting() === [$file]);

$lines = \turbocov\collect(\turbocov\inclusive, [$file])[$file];
ksort($lines);

foreach ($lines as $line => $status) {
    echo $line, ' ', $status === 1 ? 'covered' : 'not covered', "\n";
}
?>
--EXPECT--
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
