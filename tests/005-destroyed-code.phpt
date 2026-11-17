--TEST--
Code that the engine destroys before stop() keeps its lines
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}
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

$total = 0;

foreach (\turbocov\stop() as $name => $lines) {
    if (str_contains($name, "eval()'d code")) {
        $total += count($lines);
    }
}

var_dump($total > 0);

for ($run = 0; $run < 100; $run++) {
    \turbocov\start();
    eval('$c = ' . $run . ';');
    \turbocov\stop();
}

echo "done\n";
?>
--EXPECT--
bool(true)
done
