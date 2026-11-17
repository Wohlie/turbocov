--TEST--
The build with --enable-turbocov-impersonate-pcov has the functions and the settings of PCOV
--EXTENSIONS--
pcov
--INI--
pcov.directory={PWD}/../fixtures
pcov.exclude=~/basic[.]php~
--FILE--
<?php
$file = dirname(__DIR__) . '/fixtures/functions.php';
require $file;
require dirname(__DIR__) . '/fixtures/basic.php';

var_dump(extension_loaded('pcov'), extension_loaded('turbocov'));
var_dump(\pcov\all, \pcov\inclusive, \pcov\exclusive);

$functions = get_extension_funcs('pcov');
sort($functions);
echo implode(',', $functions), "\n";

\pcov\start();
second(2);
\pcov\stop();

var_dump(\pcov\waiting() === [$file]);

$lines = \pcov\collect(\pcov\inclusive, [$file])[$file];
ksort($lines);
echo json_encode($lines), "\n";
echo "second collect: ", json_encode(\pcov\collect()), "\n";
\pcov\clear();

\pcov\start();
first(20);
\pcov\stop();
echo json_encode(array_keys(array_filter(\pcov\collect()[$file], static fn (int $status): bool => $status === 1))), "\n";
?>
--EXPECT--
5
bool(true)
bool(false)
int(0)
int(1)
int(2)
pcov\clear,pcov\collect,pcov\memory,pcov\start,pcov\stats,pcov\stop,pcov\waiting
bool(true)
{"5":1,"6":-1,"9":1,"14":1,"16":1,"17":1,"20":1,"25":-1,"27":-1}
second collect: []
[5,6]
