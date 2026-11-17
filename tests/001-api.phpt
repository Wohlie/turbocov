--TEST--
The extension registers its functions
--EXTENSIONS--
turbocov
--FILE--
<?php
var_dump(extension_loaded('turbocov'));
var_dump(is_string(phpversion('turbocov')));

$functions = get_extension_funcs('turbocov');
sort($functions);
echo implode("\n", $functions), "\n";

try {
    \turbocov\executable_lines('file.php');
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(true)
bool(true)
turbocov\executable_lines
turbocov\start
turbocov\stats
turbocov\status
turbocov\stop
turbocov\executable_lines(): Argument #1 ($files) must be of type array, string given
