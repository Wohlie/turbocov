--TEST--
With read-only OPcache memory ensure_can_collect() and start() throw an exception
--EXTENSIONS--
turbocov
--SKIPIF--
<?php
if (!extension_loaded('Zend OPcache')) {
    die('skip OPcache is not loaded');
}
?>
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.protect_memory=1
turbocov.collect_paths={PWD}/fixtures
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
ensure_can_collect(): turbocov cannot collect coverage: opcache.protect_memory makes the compiled code read-only
start(): turbocov cannot collect coverage: opcache.protect_memory makes the compiled code read-only
[]
false
