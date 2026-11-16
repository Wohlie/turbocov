<?php declare(strict_types=1);

// Runs the call sequence of a test runner (start, test, stop, waiting, collect, clear) and prints each result as JSON.
// The first argument is the function namespace: pcov or turbocov.

[, $namespace, $file] = $argv;
$file = realpath($file);

$call = static fn (string $function, mixed ...$arguments): mixed => ("\\$namespace\\$function")(...$arguments);
$inclusive = constant("\\$namespace\\inclusive");
$exclusive = constant("\\$namespace\\exclusive");
$report = static function (string $label, mixed $value): void {
    if (is_array($value)) {
        foreach ($value as &$lines) {
            if (is_array($lines)) {
                ksort($lines);
            }
        }
    }

    echo $label, ': ', json_encode($value), "\n";
};

require $file;

$report('1 nothing ran, waiting', $call('waiting'));
$report('1 nothing ran, collect', $call('collect', $inclusive, [$file]));

$call('start');
first(1);
$call('stop');
$report('2 first(1), waiting', array_map('basename', $call('waiting')));
$report('2 first(1), collect', $call('collect', $inclusive, [$file])[$file] ?? null);
$call('clear');

second(3);

$call('start');
second(2);
$call('stop');
$report('3 second(2) after a run outside of the collection', $call('collect', $inclusive, [$file])[$file] ?? null);
$call('clear');

$call('start');
$call('stop');
$report('4 empty test, waiting', $call('waiting'));
$report('4 empty test, collect', $call('collect', $inclusive, [$file]));

$call('start');
first(20);
$call('stop');
$call('start');
first(1);
$call('stop');
$report('5 two runs without a clear', $call('collect', $inclusive, [$file])[$file] ?? null);
$report('5 second collect', $call('collect', $inclusive, [$file]));
$call('clear');

$call('start');
first(1);
$call('stop');
$report('6 collect all', array_map(null, array_map('basename', array_keys($call('collect')))));
$call('clear');

$call('start');
first(1);
$call('stop');
$report('7 collect exclusive', array_keys($call('collect', $exclusive, [$file])));
$call('clear');

for ($test = 0; $test < 3; $test++) {
    $call('start');
    second($test);
    $call('stop');
    $report("8 loop $test", $call('collect', $inclusive, [$file])[$file] ?? null);
    $call('clear');
}
