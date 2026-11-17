--TEST--
start(), stop() and clear() give each run its own data
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures
--FILE--
<?php
$file = __DIR__ . '/fixtures/functions.php';
require $file;

function covered(string $file): string
{
    $lines = \turbocov\collect(\turbocov\inclusive, [$file])[$file] ?? [];
    $covered = array_keys(array_filter($lines, static fn (int $status): bool => $status === 1));
    sort($covered);

    return implode(',', $covered);
}

echo "nothing ran: ", json_encode(\turbocov\collect()), "\n";

\turbocov\start();
first(1);
\turbocov\stop();
echo "first(1): ", covered($file), "\n";
\turbocov\clear();

// This call runs outside of the collection. Its lines must be available to the next run.
second(3);

\turbocov\start();
second(2);
\turbocov\stop();
echo "second(2): ", covered($file), "\n";
\turbocov\clear();

\turbocov\start();
\turbocov\stop();
echo "empty run: ", json_encode(\turbocov\waiting()), " ", json_encode(\turbocov\collect()), "\n";

\turbocov\start();
first(20);
\turbocov\stop();
\turbocov\start();
first(1);
\turbocov\stop();
echo "two runs, no clear: ", covered($file), "\n";
echo "second collect: ", json_encode(\turbocov\collect()), "\n";
\turbocov\clear();

\turbocov\start();
first(20);
\turbocov\stop();
echo "after clear: ", covered($file), "\n";
?>
--EXPECT--
nothing ran: []
first(1): 5,9
second(2): 5,9,14,16,17,20
empty run: [] []
two runs, no clear: 5,6,9
second collect: []
after clear: 5,6
