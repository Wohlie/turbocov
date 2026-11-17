--TEST--
get_debug_info() lists the probes of each function with the opcode and the state
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures
--FILE--
<?php
$file = __DIR__ . '/fixtures/functions.php';
require $file;

function dump(string $label, string $file): void
{
    $info = \turbocov\get_debug_info($file);
    echo $label, ': ', implode(',', array_column($info['units'], 'name')), "\n";

    foreach ($info['units'][0]['probes'] as $probe) {
        echo '  line ', $probe['line'], ' opnum ', $probe['opnum'], ' ', $probe['opcode'], ': ', $probe['state'], "\n";
    }
}

echo json_encode(\turbocov\get_debug_info($file)['executable_lines']), "\n";

first(20);
dump('before start()', $file);

\turbocov\start();
first(1);
dump('during the collection', $file);

$global = \turbocov\get_status();
echo json_encode(array_slice($global, 0, 3)), ' ', $global['installed_probes'] > 0 ? 'probes' : '', ' ', $global['fired_probes'] > 0 ? 'fired' : '', "\n";

\turbocov\stop();
dump('after stop()', $file);
?>
--EXPECTF--
[5,6,9,14,16,17,20,25,27]
before start(): first,second,third
  line 5 opnum %d ZEND_%s: fired while the collection was off
  line 6 opnum %d ZEND_%s: fired while the collection was off
  line 9 opnum %d ZEND_%s: armed
during the collection: first,second,third
  line 5 opnum %d ZEND_%s: fired
  line 6 opnum %d ZEND_%s: armed
  line 9 opnum %d ZEND_%s: fired
{"disabled_reason":null,"collecting":true,"collected_files":1} probes fired
after stop(): first,second,third
  line 5 opnum %d ZEND_%s: armed
  line 6 opnum %d ZEND_%s: armed
  line 9 opnum %d ZEND_%s: armed
