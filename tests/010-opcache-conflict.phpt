--TEST--
With read-only OPcache memory start() throws an exception
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
turbocov cannot collect coverage: opcache.protect_memory makes the compiled code read-only
turbocov cannot collect coverage: opcache.protect_memory makes the compiled code read-only
[]
false
