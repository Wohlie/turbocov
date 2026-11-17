--TEST--
Files outside of the collect paths and files that match the exclude pattern get no probes
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures
turbocov.exclude_pattern=~/functions[.]php~
--FILE--
<?php
\turbocov\start();
require __DIR__ . '/fixtures/functions.php';
require __DIR__ . '/fixtures/basic.php';
second(2);
\turbocov\stop();

echo json_encode(array_map('basename', \turbocov\waiting())), "\n";
echo json_encode(array_map('basename', array_keys(\turbocov\collect()))), "\n";
echo \turbocov\stats()['files'], "\n";
?>
--EXPECT--
5
["basic.php"]
["basic.php"]
1
