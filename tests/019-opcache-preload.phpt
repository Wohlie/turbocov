--TEST--
With opcache.preload ensure_can_collect() and start() throw an exception
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
opcache.optimization_level=0
opcache.protect_memory=0
opcache.preload={PWD}/fixtures/functions.php
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
ensure_can_collect(): turbocov cannot collect coverage: opcache.preload is set. Code that OPcache preloads gets no probes
start(): turbocov cannot collect coverage: opcache.preload is set. Code that OPcache preloads gets no probes
