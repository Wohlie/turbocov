<?php declare(strict_types=1);

function shippingFee(
    int $weight,
    string $zone,
): int {
    if ($weight <= 0) {
        throw new InvalidArgumentException('Weight must be positive');
    }

    switch ($zone) {
        case 'eu':
            $base = 5;
            break;
        case 'us':
            $base = 9;
            break;
        default:
            $base = 20;
    }

    $fee = $weight > 10
        ? $base * 2
        : $base;

    return $fee;

    error_log('never reached');
}

function neverCalled(): string
{
    return str_repeat(
        'fee',
        shippingFee(1, 'eu'),
    );
}

echo shippingFee(3, 'eu'), "\n";
