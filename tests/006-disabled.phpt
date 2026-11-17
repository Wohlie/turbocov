--TEST--
With turbocov.enabled=0 the extension does nothing
--EXTENSIONS--
turbocov
--INI--
turbocov.enabled=0
turbocov.collect_paths={PWD}/fixtures
--FILE--
<?php
var_dump(\turbocov\get_disabled_reason());

\turbocov\start();
require __DIR__ . '/fixtures/functions.php';
second(2);

var_dump(\turbocov\stop(), \turbocov\get_executable_lines([__DIR__ . '/fixtures/functions.php']));
echo json_encode(\turbocov\get_status()), "\n";
echo json_encode(\turbocov\get_debug_info(__DIR__ . '/fixtures/functions.php')), "\n";
?>
--EXPECT--
string(21) "turbocov.enabled is 0"
array(0) {
}
array(0) {
}
{"disabled_reason":"turbocov.enabled is 0","collecting":false,"collected_files":0,"installed_probes":0,"fired_probes":0}
{"skip_reason":"turbocov.enabled is 0","executable_lines":[],"units":[]}
