--TEST--
A closure in a constant expression reports its lines, also when it never runs
--EXTENSIONS--
turbocov
--SKIPIF--
<?php
if (PHP_VERSION_ID < 80500) {
    die('skip PHP before 8.5 has no closures in constant expressions');
}
?>
--INI--
turbocov.collect_paths={PWD}/../fixtures
--FILE--
<?php
$file = dirname(__DIR__) . '/fixtures/const-expr-closures.php';

\turbocov\start();
require $file;
$executed = \turbocov\stop()[$file];

$covered = array_keys($executed);
$uncovered = array_keys(array_diff_key(\turbocov\get_executable_lines([$file])[$file], $executed));
sort($covered);
sort($uncovered);

echo 'covered: ', implode(',', $covered), "\n";
echo 'not covered: ', implode(',', $uncovered), "\n";

$names = implode("\n", array_column(\turbocov\get_debug_info($file)['units'], 'name'));
echo 'name of a closure that ran: ', str_contains($names, '{closure:Modern::format():51}') ? 'found' : 'missing', "\n";
echo 'name of a closure that did not run: ', str_contains($names, '{closure:Modern::notCalled():59}') ? 'found' : 'missing', "\n";

$probes = \turbocov\get_status()['installed_probes'];
echo (Modern::NOT_USED)('LATE'), "\n";
echo (GLOBAL_NOT_USED)(2), "\n";
echo Modern::notCalled(), "\n";
echo 'probes for the late calls: ', \turbocov\get_status()['installed_probes'] - $probes, "\n";
?>
--EXPECT--
1.3
LOUD!
USED
list used
2
3
inner
4
covered: 6,15,24,37,41,52,55,67,68,72,73,74,76,90,92,95,96,99,104,114,124,125,126,127,128,129,130,131,132
not covered: 10,19,27,32,45,60,63,81,85,108,112,118,121
name of a closure that ran: found
name of a closure that did not run: found
late
6
1.00
probes for the late calls: 0
