<?php declare(strict_types=1);

// Prints the line data of one file as JSON. The first argument is the function namespace: pcov or turbocov.

[, $namespace, $file] = $argv;
$file = realpath($file);

ob_start();
("\\$namespace\\start")();
(static function () use ($file): void {
    require $file;
})();
("\\$namespace\\stop")();
ob_end_clean();

$lines = ("\\$namespace\\collect")(constant("\\$namespace\\inclusive"), [$file])[$file] ?? null;

if ($lines === null) {
    fwrite(STDERR, "No data for $file\n");
    exit(1);
}

ksort($lines);
echo json_encode($lines), "\n";
