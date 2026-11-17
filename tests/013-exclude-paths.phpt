--TEST--
Files in turbocov.exclude_paths get no probes
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures
turbocov.exclude_paths={PWD}/fixtures/basic.php
--FILE--
<?php
\turbocov\start();
require __DIR__ . "/fixtures/functions.php";
require __DIR__ . "/fixtures/basic.php";
second(2);
\turbocov\stop();

echo json_encode(array_map("basename", array_keys(\turbocov\collect()))), "\n";
?>
--EXPECTF--
5
["functions.php"]
