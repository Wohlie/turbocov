--TEST--
With a path in turbocov.collect_paths that does not exist ensure_can_collect() and start() throw an exception
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures/functions.php:{PWD}/fixtures/missing.php
--FILE--
<?php
foreach (['ensure_can_collect', 'start'] as $function) {
    try {
        ("turbocov\\$function")();
    } catch (\turbocov\CannotCollectException $e) {
        echo $function, '(): ', $e->getMessage(), "\n";
    }
}

require __DIR__ . "/fixtures/functions.php";
second(2);

echo json_encode(\turbocov\stop()), "\n";
echo json_encode(\turbocov\get_status()["collecting"]), "\n";
?>
--EXPECTF--
ensure_can_collect(): turbocov cannot collect coverage: turbocov.collect_paths has a path that does not exist: %smissing.php
start(): turbocov cannot collect coverage: turbocov.collect_paths has a path that does not exist: %smissing.php
[]
false
