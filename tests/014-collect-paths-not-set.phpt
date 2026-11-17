--TEST--
Without turbocov.collect_paths start() throws an exception
--EXTENSIONS--
turbocov
--INI--
turbocov.enabled=1
--FILE--
<?php
for ($call = 0; $call < 2; $call++) {
    try {
        \turbocov\start();
    } catch (\turbocov\Exception $e) {
        echo $e->getMessage(), "\n";
    }
}

require __DIR__ . "/fixtures/functions.php";
second(2);

echo json_encode(\turbocov\stop()), "\n";
echo json_encode(\turbocov\get_status()["collecting"]), "\n";
?>
--EXPECTF--
turbocov cannot collect coverage: turbocov.collect_paths is not set
turbocov cannot collect coverage: turbocov.collect_paths is not set
[]
false
