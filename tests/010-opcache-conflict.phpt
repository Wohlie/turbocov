--TEST--
With read-only OPcache memory the extension turns the collection off and start() gives a warning
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
var_dump(\turbocov\get_disabled_reason());

\turbocov\start();
require __DIR__ . '/fixtures/functions.php';
var_dump(second(2));
var_dump(\turbocov\stop());
\turbocov\start();
?>
--EXPECTF--
string(56) "opcache.protect_memory makes the compiled code read-only"

Warning: %s: the coverage collection is off, because opcache.protect_memory makes the compiled code read-only in %s on line %d
int(1)
array(0) {
}
