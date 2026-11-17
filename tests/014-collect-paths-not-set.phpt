--TEST--
Without turbocov.collect_paths the extension collects nothing and start() gives a warning
--EXTENSIONS--
turbocov
--INI--
turbocov.enabled=1
--FILE--
<?php
\turbocov\start();
require __DIR__ . "/fixtures/functions.php";
require __DIR__ . "/fixtures/basic.php";
second(2);
echo json_encode(array_map("basename", array_keys(\turbocov\stop()))), "\n";
echo \turbocov\get_skip_reason(__DIR__ . "/fixtures/functions.php"), "\n";
?>
--EXPECTF--

Warning: turbocov: the coverage collection is off, because turbocov.collect_paths is not set in %s on line %d
5
[]
turbocov.collect_paths is not set
