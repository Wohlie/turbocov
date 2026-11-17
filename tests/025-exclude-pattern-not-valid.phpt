--TEST--
With a pattern in turbocov.exclude_pattern that is not valid ensure_can_collect() and start() throw an exception
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures
turbocov.exclude_pattern=functions
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
ensure_can_collect(): turbocov cannot collect coverage: turbocov.exclude_pattern is not a valid pattern: functions
start(): turbocov cannot collect coverage: turbocov.exclude_pattern is not a valid pattern: functions
[]
false
