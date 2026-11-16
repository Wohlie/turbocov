<?php declare(strict_types=1);

function first(int $value): int
{
    if ($value > 10) {
        return 10;
    }

    return $value;
}

function second(int $value): int
{
    $result = 0;

    for ($i = 0; $i < $value; $i++) {
        $result += first($i);
    }

    return $result;
}

function third(): string
{
    return 'never called';
}
