<?php declare(strict_types=1);

// Micro-benchmark. Arguments: <namespace|none> <inline|calls|tests>
// "tests" runs 20 000 start/stop/collect/clear cycles with 300 calls each.

function work(int $i): int
{
    $a = $i + 1;
    $b = $a * 2;

    return $b - $i;
}

[, $namespace, $kind] = $argv;
$on = $namespace !== 'none';
$call = static fn (string $function, mixed ...$arguments): mixed => ("\\$namespace\\$function")(...$arguments);
$sum = 0;
$start = hrtime(true);

if ($kind === 'tests') {
    $inclusive = $on ? constant("\\$namespace\\inclusive") : 0;

    for ($test = 0; $test < 20_000; $test++) {
        $on && $call('start');

        for ($i = 0; $i < 300; $i++) {
            $sum += work($i);
        }

        if ($on) {
            $call('stop');
            $files = $call('waiting');

            if ($files !== []) {
                $call('collect', $inclusive, $files);
                $call('clear');
            }
        }
    }
} else {
    $on && $call('start');

    if ($kind === 'calls') {
        for ($i = 0; $i < 3_000_000; $i++) {
            $sum += work($i);
        }
    } else {
        for ($i = 0; $i < 3_000_000; $i++) {
            $a = $i + 1;
            $b = $a * 2;
            $sum += $b - $i;
        }
    }
}

printf("%d\n", (hrtime(true) - $start) / 1e6);
