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
    \turbocov\get_executable_lines('file.php');
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(true)
bool(true)
turbocov\get_debug_info
turbocov\get_disabled_reason
turbocov\get_executable_lines
turbocov\get_status
turbocov\start
turbocov\stop
turbocov\get_executable_lines(): Argument #1 ($files) must be of type array, string given
