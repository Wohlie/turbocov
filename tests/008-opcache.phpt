--TEST--
With OPcache on for the CLI the probes go into the cached code
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
opcache.jit=disable
opcache.protect_memory=0
opcache.file_update_protection=0
turbocov.collect_paths={PWD}/fixtures
--FILE--
<?php
$file = __DIR__ . '/fixtures/functions.php';
var_dump(\turbocov\get_disabled_reason());
require $file;
var_dump(opcache_is_script_cached($file));

for ($run = 0; $run < 2; $run++) {
    \turbocov\start();
    second(2);

    $covered = array_keys(\turbocov\stop()[$file]);
    sort($covered);
    echo implode(',', $covered), "\n";
}
?>
--EXPECT--
NULL
bool(true)
5,9,14,16,17,20
5,9,14,16,17,20
