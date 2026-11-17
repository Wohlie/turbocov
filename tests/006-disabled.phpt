--TEST--
With turbocov.enabled=0 the extension does nothing
--EXTENSIONS--
turbocov
--INI--
turbocov.enabled=0
turbocov.collect_paths={PWD}/fixtures
--FILE--
<?php
var_dump(\turbocov\enabled());

\turbocov\start();
require __DIR__ . '/fixtures/functions.php';
second(2);
\turbocov\stop();

var_dump(\turbocov\waiting(), \turbocov\collect());
\turbocov\clear();
echo json_encode(\turbocov\stats()), "\n";
?>
--EXPECT--
bool(false)
array(0) {
}
array(0) {
}
{"files":0,"probes":0,"fires":0,"hits":0,"strays":0}
