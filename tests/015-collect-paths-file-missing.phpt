--TEST--
With a list file that is not readable start() throws an exception
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths=@{PWD}/fixtures/no-such-list.txt
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
turbocov cannot collect coverage: the list file %sno-such-list.txt of turbocov.collect_paths is not readable
turbocov cannot collect coverage: the list file %sno-such-list.txt of turbocov.collect_paths is not readable
[]
false
