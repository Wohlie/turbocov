--TEST--
turbocov.collect_paths takes a list of files and directories
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures/functions.php:{PWD}/fixtures/missing.php
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
