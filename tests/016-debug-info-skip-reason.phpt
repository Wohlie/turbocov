--TEST--
get_debug_info() tells why a file has no data
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures
turbocov.exclude_paths={PWD}/fixtures/basic.php
turbocov.exclude_pattern=~/constructs[.]php~
--FILE--
<?php
require __DIR__ . '/fixtures/functions.php';

foreach (['fixtures/functions.php', 'fixtures/edge-cases.php', 'fixtures/basic.php', 'fixtures/constructs.php', '016-debug-info-skip-reason.php'] as $file) {
    $info = \turbocov\get_debug_info(__DIR__ . '/' . $file);
    echo $file, ': ', json_encode($info['skip_reason']), ' ', count($info['executable_lines']), ' ', count($info['units']), "\n";
}
?>
--EXPECTF--
fixtures/functions.php: null 9 3
fixtures/edge-cases.php: "PHP did not compile this file" 0 0
fixtures/basic.php: "turbocov.exclude_paths has %sbasic.php" 0 0
fixtures/constructs.php: "turbocov.exclude_pattern matches this file" 0 0
016-debug-info-skip-reason.php: "turbocov.collect_paths does not have this file" 0 0
