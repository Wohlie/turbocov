--TEST--
get_skip_reason() tells why a file has no data
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures
turbocov.exclude_paths={PWD}/fixtures/basic.php
turbocov.exclude_pattern=~/constructs[.]php~
--FILE--
<?php
require __DIR__ . '/fixtures/functions.php';

foreach (['fixtures/functions.php', 'fixtures/edge-cases.php', 'fixtures/basic.php', 'fixtures/constructs.php', '016-skip-reason.php'] as $file) {
    echo $file, ': ', json_encode(\turbocov\get_skip_reason(__DIR__ . '/' . $file)), "\n";
}
?>
--EXPECTF--
fixtures/functions.php: null
fixtures/edge-cases.php: "PHP did not compile this file"
fixtures/basic.php: "turbocov.exclude_paths has %sbasic.php"
fixtures/constructs.php: "turbocov.exclude_pattern matches this file"
016-skip-reason.php: "turbocov.collect_paths does not have this file"
