<?php declare(strict_types=1);

final class Temperature
{
    public float $celsius = 0.0 {
        get {
            return $this->celsius;
        }
        set(float $value) {
            if ($value < -273.15) {
                throw new InvalidArgumentException('below absolute zero');
            }

            $this->celsius = $value;
        }
    }

    public float $fahrenheit {
        get => $this->celsius * 9 / 5
            + 32;
    }

    public string $unused {
        get {
            return 'never read';
        }
    }
}

$temperature = new Temperature();
$temperature->celsius = 20.0;
$value = $temperature->fahrenheit;
