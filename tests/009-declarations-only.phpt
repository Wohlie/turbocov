--TEST--
A file with declarations only is reported when it is included during the collection
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures
--FILE--
<?php
$file = __DIR__ . '/fixtures/functions.php';

\turbocov\start();
require $file;
$executed = \turbocov\stop();

var_dump(array_keys($executed) === [$file]);
echo json_encode($executed[$file]), "\n";

$lines = \turbocov\get_executable_lines([$file])[$file];
ksort($lines);
echo json_encode($lines), "\n";

\turbocov\start();
first(1);
echo json_encode(\turbocov\stop()[$file]), "\n";
?>
--EXPECT--
bool(true)
{"27":1}
{"5":-1,"6":-1,"9":-1,"14":-1,"16":-1,"17":-1,"20":-1,"25":-1,"27":-1}
{"5":1,"9":1}
