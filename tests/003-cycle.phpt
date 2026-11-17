--TEST--
Each start() and stop() pair has its own data
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures
--FILE--
<?php
$file = __DIR__ . '/fixtures/functions.php';
require $file;

function covered(array $executed, string $file): string
{
    $covered = array_keys($executed[$file] ?? []);
    sort($covered);

    return implode(',', $covered);
}

echo "nothing ran: ", json_encode(\turbocov\stop()), "\n";

\turbocov\start();
first(1);
echo "first(1): ", covered(\turbocov\stop(), $file), "\n";

// This call runs outside of the collection. Its lines must be available to the next run.
second(3);

\turbocov\start();
second(2);
echo "second(2): ", covered(\turbocov\stop(), $file), "\n";

\turbocov\start();
echo "empty run: ", json_encode(\turbocov\stop()), "\n";

\turbocov\start();
first(20);
echo "first(20): ", covered(\turbocov\stop(), $file), "\n";
echo "second stop: ", json_encode(\turbocov\stop()), "\n";

\turbocov\start();
first(20);
echo "first(20) again: ", covered(\turbocov\stop(), $file), "\n";
?>
--EXPECT--
nothing ran: []
first(1): 5,9
second(2): 5,9,14,16,17,20
empty run: []
first(20): 5,6
second stop: []
first(20) again: 5,6
