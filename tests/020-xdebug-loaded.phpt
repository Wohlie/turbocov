--TEST--
With Xdebug loaded ensure_can_collect() and start() throw an exception
--EXTENSIONS--
turbocov
--SKIPIF--
<?php
if (!extension_loaded('xdebug')) {
    die('skip Xdebug is not available');
}
?>
--INI--
zend_extension=xdebug
xdebug.mode=off
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
?>
--EXPECT--
ensure_can_collect(): turbocov cannot collect coverage: the Xdebug extension is loaded. Do not load it together with turbocov
start(): turbocov cannot collect coverage: the Xdebug extension is loaded. Do not load it together with turbocov
