--TEST--
With OPcache on for the CLI the extension turns the collection off
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
turbocov.directory={PWD}/fixtures
--FILE--
<?php
var_dump(\turbocov\enabled());

\turbocov\start();
require __DIR__ . '/fixtures/functions.php';
var_dump(second(2));
\turbocov\stop();

var_dump(\turbocov\collect());
?>
--EXPECTF--
bool(false)

Warning: %s: OPcache is on for this SAPI, thus the coverage collection is off in %s on line %d
int(1)
array(0) {
}
