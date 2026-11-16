--TEST--
The extension registers its functions and constants
--EXTENSIONS--
turbocov
--FILE--
<?php
var_dump(extension_loaded('turbocov'));
var_dump(\turbocov\enabled());
var_dump(\turbocov\all, \turbocov\inclusive, \turbocov\exclusive);
var_dump(is_string(\turbocov\version));

foreach (['start', 'stop', 'collect', 'clear', 'waiting', 'memory', 'enabled', 'stats'] as $function) {
    if (!function_exists("turbocov\\$function")) {
        echo "missing $function\n";
    }
}

try {
    \turbocov\collect(7);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(true)
bool(true)
int(0)
int(1)
int(2)
bool(true)
type must be \turbocov\inclusive, \turbocov\exclusive, or \turbocov\all
