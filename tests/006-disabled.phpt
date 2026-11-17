--TEST--
With turbocov.enabled=0 the extension does nothing
--EXTENSIONS--
turbocov
--INI--
turbocov.enabled=0
turbocov.collect_paths={PWD}/fixtures
--FILE--
<?php
var_dump(\turbocov\status());

\turbocov\start();
require __DIR__ . '/fixtures/functions.php';
second(2);

var_dump(\turbocov\stop(), \turbocov\executable_lines([__DIR__ . '/fixtures/functions.php']));
echo json_encode(\turbocov\stats()), "\n";
?>
--EXPECT--
string(21) "turbocov.enabled is 0"
array(0) {
}
array(0) {
}
{"files":0,"probes":0,"fires":0,"hits":0,"strays":0}
