--TEST--
A file with declarations only is reported when it is included during the collection
--EXTENSIONS--
turbocov
--INI--
turbocov.directory={PWD}/fixtures
--FILE--
<?php
$file = __DIR__ . '/fixtures/functions.php';

\turbocov\start();
require $file;
\turbocov\stop();

var_dump(\turbocov\waiting() === [$file]);

$lines = \turbocov\collect(\turbocov\inclusive, [$file])[$file];
ksort($lines);
echo json_encode($lines), "\n";
\turbocov\clear();

\turbocov\start();
first(1);
\turbocov\stop();

$lines = \turbocov\collect(\turbocov\inclusive, [$file])[$file];
ksort($lines);
echo json_encode($lines), "\n";
?>
--EXPECT--
bool(true)
{"5":-1,"6":-1,"9":-1,"14":-1,"16":-1,"17":-1,"20":-1,"25":-1,"27":1}
{"5":1,"6":-1,"9":1,"14":-1,"16":-1,"17":-1,"20":-1,"25":-1,"27":-1}
