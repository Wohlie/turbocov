--TEST--
Line shapes with a known result: where the report differs from what PHP executes, and where it is exact
--EXTENSIONS--
turbocov
--INI--
turbocov.collect_paths={PWD}/fixtures/line-shapes.php
zend.assertions=1
--FILE--
<?php
require __DIR__ . '/fixtures/line-shapes.php';

// Prints each executable line of the function and its result. Line +0 is the line with "function".
$report = static function (string $label, callable $call): void {
    $function = new ReflectionFunction(explode(' ', $label)[0]);
    $file = $function->getFileName();

    ob_start();
    \turbocov\start();

    try {
        $call();
    } catch (Throwable $e) {
    }

    $executed = \turbocov\stop()[$file] ?? [];
    ob_end_clean();

    $lines = [];

    foreach (array_keys(\turbocov\get_executable_lines([$file])[$file]) as $line) {
        if ($line >= $function->getStartLine() && $line <= $function->getEndLine()) {
            $lines[$line] = '+' . ($line - $function->getStartLine()) . (isset($executed[$line]) ? ' executed' : ' not executed');
        }
    }

    ksort($lines);
    echo $label, ': ', implode(', ', $lines), "\n";
};

echo "PHP reaches the line, the report has it as not executed:\n";
$report('return_type_error', static fn () => return_type_error());
$report('missing_return', static fn () => missing_return(1));
$report('return_in_try', static fn () => return_in_try());
$report('break_in_try', static fn () => break_in_try([1]));
$report('empty_try', static fn () => empty_try());
$report('class_declaration', static fn () => class_declaration());
$report('anonymous_class', static fn () => anonymous_class());

echo "PHP cannot reach the line, the report has it as executed or as executable:\n";
$report('duplicate_arm', static fn () => duplicate_arm(1));
$report('after_never', static fn () => after_never());
$report('after_finally', static fn () => after_finally('a'));

echo "PHP starts the line and does not run its main part:\n";
$report('assertion off', static function () {
    ini_set('zend.assertions', '0');
    assertion(false);
    ini_set('zend.assertions', '1');
});
$report('one_line type error', static fn () => one_line('a'));
$report('first_arm third', static fn () => first_arm(0));

echo "The report is exact:\n";
$report('closure_end', static fn () => closure_end());
$report('immediate_call', static fn () => immediate_call());

echo "A line with only a parameter, a constant argument or a global statement is not an executable line:\n";
$report('not_listed', static fn () => not_listed(1));
?>
--EXPECT--
PHP reaches the line, the report has it as not executed:
return_type_error: +1 not executed
missing_return: +1 executed, +2 not executed
return_in_try: +2 not executed, +4 executed
break_in_try: +1 executed, +3 not executed, +5 executed, +8 not executed
empty_try: +2 not executed, +3 executed, +5 not executed
class_declaration: +2 not executed, +3 executed, +4 executed, +6 not executed
anonymous_class: +1 not executed
PHP cannot reach the line, the report has it as executed or as executable:
duplicate_arm: +1 executed, +2 executed, +3 not executed, +4 executed, +6 executed
after_never: +1 executed, +2 not executed
after_finally: +2 executed, +3 not executed, +4 executed, +6 not executed
PHP starts the line and does not run its main part:
assertion off: +1 not executed, +2 executed
one_line type error: +0 not executed
first_arm third: +1 executed, +2 not executed, +3 executed, +4 executed, +5 not executed
The report is exact:
closure_end: +1 executed, +2 executed, +3 executed, +5 not executed
immediate_call: +1 executed, +2 executed, +3 executed
A line with only a parameter, a constant argument or a global statement is not an executable line:
not_listed: +5 executed, +6 executed, +8 executed
