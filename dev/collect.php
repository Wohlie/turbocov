<?php declare(strict_types=1);

// Prints the line data of one file as JSON. The first argument is the function namespace: pcov or turbocov.

[, $namespace, $file] = $argv;
$file = realpath($file);

ob_start();
("\\$namespace\\start")();
(static function () use ($file): void {
    require $file;
})();

if ($namespace === 'pcov') {
    \pcov\stop();
    $lines = \pcov\collect(\pcov\inclusive, [$file])[$file] ?? null;
} else {
    $executed = \turbocov\stop()[$file] ?? null;
    $lines = $executed === null ? null : array_replace(\turbocov\executable_lines([$file])[$file], $executed);
}

ob_end_clean();

if ($lines === null) {
    fwrite(STDERR, "No data for $file\n");
    exit(1);
}

ksort($lines);
echo json_encode($lines), "\n";
