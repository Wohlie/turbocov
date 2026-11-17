--TEST--
With a list file that is not readable the extension collects nothing and start() gives a warning
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths=@{PWD}/fixtures/no-such-list.txt
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

Warning: turbocov: the coverage collection is off, because the list file %sno-such-list.txt of turbocov.collect_paths is not readable in %s on line %d
5
[]
