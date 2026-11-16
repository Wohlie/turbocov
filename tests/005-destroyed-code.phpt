--TEST--
Code that the engine destroys before collect() keeps its lines
--EXTENSIONS--
turbocov
--INI--
turbocov.directory={PWD}
--FILE--
<?php
\turbocov\start();

for ($i = 0; $i < 1000; $i++) {
    $closure = eval('return static function (int $x): int {
        return $x + 1;
    };');
    $closure($i);
    unset($closure);
}

eval('$a = 1;
$b = 2;');
\turbocov\stop();

$total = 0;

foreach (\turbocov\collect() as $name => $lines) {
    if (str_contains($name, "eval()'d code")) {
        $total += count(array_filter($lines, static fn (int $status): bool => $status === 1));
    }
}

var_dump($total > 0);
\turbocov\clear();

for ($run = 0; $run < 100; $run++) {
    \turbocov\start();
    eval('$c = ' . $run . ';');
    \turbocov\stop();
    \turbocov\collect();
    \turbocov\clear();
}

echo "done\n";
?>
--EXPECT--
bool(true)
done
