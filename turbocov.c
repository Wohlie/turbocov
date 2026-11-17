#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "SAPI.h"
#include "ext/standard/info.h"
#include "ext/pcre/php_pcre.h"
#include "zend_extensions.h"
#include "zend_vm.h"
#include "zend_vm_opcodes.h"
#include "zend_bitset.h"
#include "zend_arena.h"
#include "Zend/Optimizer/zend_cfg.h"

#include "php_turbocov.h"

#if PHP_VERSION_ID < 80400
# error "turbocov needs PHP 8.4 or later"
#endif

#ifdef ZTS
# error "turbocov does not support thread-safe PHP"
#endif

#define TC_FILTER_ALL     0
#define TC_FILTER_INCLUDE 1
#define TC_FILTER_EXCLUDE 2

#define TC_UNCOVERED -1
#define TC_COVERED    1

#define TC_ARMED 0
#define TC_HIT   1
#define TC_STRAY 2

/* Value of the op array slot for code that has real handlers and no probes. */
#define TC_NO_PROBES ((void *) 1)

typedef struct _tc_file {
	zend_string *name;
	HashTable lines; /* executable line => TC_UNCOVERED */
	bool waiting;
} tc_file;

typedef struct _tc_probe {
	const void *handler; /* the handler that the probe replaced */
	uint32_t opnum;
	uint32_t line;
	uint8_t state;
} tc_probe;

/* One unit for each opcode array. Copies of an op array (closures, trait methods) share the opcodes and the unit. */
typedef struct _tc_unit {
	zend_op *opcodes; /* NULL after the engine destroyed the op array */
	tc_file *file;
	uint32_t last;
	uint32_t *index; /* opcode number => probe number + 1, or 0 */
	tc_probe *probes;
	uint32_t count;
	uint32_t pending; /* number of references from tc_hits and tc_strays */
} tc_unit;

typedef struct _tc_ref {
	tc_unit *unit;
	uint32_t probe;
} tc_ref;

typedef struct _tc_refs {
	tc_ref *refs;
	size_t count;
	size_t capacity;
} tc_refs;

/* The INI settings of PCOV take the same values. They are only other names. */
#ifdef TURBOCOV_IMPERSONATE_PCOV
# define TC_INI_COLLECT_PATHS   TURBOCOV_NAME ".directory"
# define TC_INI_EXCLUDE_PATTERN TURBOCOV_NAME ".exclude"
#else
# define TC_INI_COLLECT_PATHS   TURBOCOV_NAME ".collect_paths"
# define TC_INI_EXCLUDE_PATHS   TURBOCOV_NAME ".exclude_paths"
# define TC_INI_EXCLUDE_PATTERN TURBOCOV_NAME ".exclude_pattern"
#endif

typedef struct _tc_paths {
	HashTable files;       /* absolute path => nothing */
	HashTable directories; /* absolute path with a slash at the end => nothing */
} tc_paths;

static bool tc_ini_enabled;
static char tc_start_warning[MAXPATHLEN + 128]; /* the reason why the collection is off, for the next start() */
static bool tc_active;
static bool tc_collecting;
static bool tc_dirty;

static int tc_slot = -1;
static const void *tc_probe_label;
static zend_ulong tc_ignored_opcodes[256 / ZEND_BITSET_ELM_SIZE];

static tc_paths tc_collect_paths;
static tc_paths tc_exclude_paths;
static pcre_cache_entry *tc_exclude_pattern;

static HashTable tc_files; /* file name => tc_file, or NULL for a file outside the whitelist */
static HashTable tc_units; /* opcodes address => tc_unit */
static tc_refs tc_hits;    /* probes that fired while the collection was on */
static tc_refs tc_strays;  /* probes that fired while the collection was off */
static tc_file **tc_waiting;
static size_t tc_waiting_count;
static size_t tc_waiting_capacity;

static zend_ulong tc_stat_probes;
static zend_ulong tc_stat_fires;

/* Handlers that other extensions registered before turbocov. The probe callback forwards to them. */
static user_opcode_handler_t tc_previous_handlers[256];

static zend_op_array *(*tc_orig_compile_file)(zend_file_handle *file_handle, int type);
static zend_op_array *(*tc_orig_compile_string)(zend_string *source_string, const char *filename, zend_compile_position position);

PHP_INI_BEGIN()
	PHP_INI_ENTRY(TURBOCOV_NAME ".enabled", "1", PHP_INI_SYSTEM, NULL)
	PHP_INI_ENTRY(TC_INI_COLLECT_PATHS, "", PHP_INI_SYSTEM | PHP_INI_PERDIR, NULL)
#ifdef TC_INI_EXCLUDE_PATHS
	PHP_INI_ENTRY(TC_INI_EXCLUDE_PATHS, "", PHP_INI_SYSTEM | PHP_INI_PERDIR, NULL)
#endif
	PHP_INI_ENTRY(TC_INI_EXCLUDE_PATTERN, "", PHP_INI_SYSTEM | PHP_INI_PERDIR, NULL)
PHP_INI_END()

static void tc_refs_push(tc_refs *list, tc_unit *unit, uint32_t probe)
{
	if (list->count == list->capacity) {
		list->capacity = list->capacity ? list->capacity * 2 : 1024;
		list->refs = perealloc(list->refs, list->capacity * sizeof(tc_ref), 1);
	}

	list->refs[list->count].unit = unit;
	list->refs[list->count].probe = probe;
	list->count++;
	unit->pending++;
}

static void tc_unit_free(tc_unit *unit)
{
	pefree(unit->index, 1);
	pefree(unit->probes, 1);
	pefree(unit, 1);
}

/* Arms the listed probes again and empties the list. */
static void tc_refs_rearm(tc_refs *list)
{
	for (size_t i = 0; i < list->count; i++) {
		tc_unit *unit = list->refs[i].unit;
		tc_probe *probe = &unit->probes[list->refs[i].probe];

		probe->state = TC_ARMED;
		unit->pending--;

		if (unit->opcodes) {
			unit->opcodes[probe->opnum].handler = tc_probe_label;
		} else if (unit->pending == 0) {
			tc_unit_free(unit);
		}
	}

	list->count = 0;
}

static void tc_waiting_reset(void)
{
	for (size_t i = 0; i < tc_waiting_count; i++) {
		tc_waiting[i]->waiting = false;
	}

	tc_waiting_count = 0;
}

/* Records one fired probe. The caller puts the original handler back. */
static zend_always_inline void tc_record(tc_unit *unit, uint32_t index)
{
	tc_probe *probe = &unit->probes[index];

	tc_stat_fires++;

	if (tc_collecting) {
		tc_file *file = unit->file;

		probe->state = TC_HIT;
		tc_refs_push(&tc_hits, unit, index);
		tc_dirty = true;

		if (!file->waiting) {
			file->waiting = true;

			if (tc_waiting_count == tc_waiting_capacity) {
				tc_waiting_capacity = tc_waiting_capacity ? tc_waiting_capacity * 2 : 64;
				tc_waiting = perealloc(tc_waiting, tc_waiting_capacity * sizeof(tc_file *), 1);
			}

			tc_waiting[tc_waiting_count++] = file;
		}
	} else {
		probe->state = TC_STRAY;
		tc_refs_push(&tc_strays, unit, index);
	}
}

static zend_always_inline int tc_forward(zend_execute_data *execute_data)
{
	user_opcode_handler_t previous = tc_previous_handlers[EX(opline)->opcode];

	return previous ? previous(execute_data) : ZEND_USER_OPCODE_DISPATCH;
}

/*
 * The VM calls this function for an opcode that has the ZEND_USER_OPCODE handler.
 * A probe records its line and puts the original handler back.
 * ZEND_USER_OPCODE_CONTINUE makes the VM dispatch the same opcode again, now with the original handler.
 */
static int tc_probe_handler(zend_execute_data *execute_data)
{
	const zend_op *opline = EX(opline);
	zend_function *func = EX(func);
	tc_unit *unit;
	tc_probe *probe;
	uint32_t number;

	if (UNEXPECTED(!func || !ZEND_USER_CODE(func->type))) {
		return tc_forward(execute_data);
	}

	unit = func->op_array.reserved[tc_slot];

	/* EG(exception_op) and op arrays that no compile hook saw are not in a unit. */
	if (UNEXPECTED((uintptr_t) unit <= (uintptr_t) TC_NO_PROBES)
		|| UNEXPECTED(opline < unit->opcodes)
		|| UNEXPECTED(opline >= unit->opcodes + unit->last)) {
		return tc_forward(execute_data);
	}

	number = unit->index[opline - unit->opcodes];

	if (UNEXPECTED(number == 0)) {
		return tc_forward(execute_data);
	}

	probe = &unit->probes[number - 1];

	/*
	 * An extension that replaces zend_execute_ex (PCOV, phpdbg) calls handlers through zend_vm_call_opcode_handler().
	 * That function ignores opline->handler, thus a probe that already fired comes here again.
	 * The same occurs for an opcode that has a handler of a different extension, because its original handler is ZEND_USER_OPCODE too.
	 */
	if (UNEXPECTED(probe->state != TC_ARMED)) {
		return tc_forward(execute_data);
	}

	((zend_op *) opline)->handler = probe->handler;
	tc_record(unit, number - 1);

	return ZEND_USER_OPCODE_CONTINUE;
}

/*
 * The ZEND_USER_OPCODE handler finds the callback through the real opcode of the current instruction.
 * A probe can be on every opcode, thus every opcode needs the callback.
 */
static void tc_register_handlers(void)
{
	for (int opcode = 0; opcode < 256; opcode++) {
		if (opcode != ZEND_USER_OPCODE) {
			tc_previous_handlers[opcode] = zend_get_user_opcode_handler((uint8_t) opcode);
			zend_set_user_opcode_handler((uint8_t) opcode, tc_probe_handler);
		}
	}
}

/* Opcodes that have a handler of a different extension keep the ZEND_USER_OPCODE handler, as without turbocov. */
static void tc_unregister_handlers(void)
{
	for (int opcode = 0; opcode < 256; opcode++) {
		if (opcode != ZEND_USER_OPCODE) {
			zend_set_user_opcode_handler((uint8_t) opcode, tc_previous_handlers[opcode]);
		}
	}
}

static bool tc_paths_contain(const tc_paths *paths, zend_string *filename)
{
	zend_string *directory;

	if (zend_hash_exists(&paths->files, filename)) {
		return true;
	}

	ZEND_HASH_MAP_FOREACH_STR_KEY(&paths->directories, directory) {
		if (ZSTR_LEN(filename) > ZSTR_LEN(directory)
			&& memcmp(ZSTR_VAL(filename), ZSTR_VAL(directory), ZSTR_LEN(directory)) == 0) {
			return true;
		}
	} ZEND_HASH_FOREACH_END();

	return false;
}

static bool tc_matches_exclude_pattern(zend_string *filename)
{
	zval match;
	bool excluded;

	ZVAL_UNDEF(&match);

	php_pcre_match_impl(tc_exclude_pattern, filename, &match, NULL, false, 0, 0);

	excluded = zend_is_true(&match);
	zval_ptr_dtor(&match);

	return excluded;
}

/* Returns the record of a whitelisted file, or NULL. */
static tc_file *tc_file_for(zend_string *filename)
{
	zval *known;
	tc_file *file = NULL;

	if (!filename) {
		return NULL;
	}

	known = zend_hash_find(&tc_files, filename);

	if (known) {
		return Z_PTR_P(known);
	}

	if (tc_paths_contain(&tc_collect_paths, filename)
		&& !tc_paths_contain(&tc_exclude_paths, filename)
		&& !(tc_exclude_pattern && tc_matches_exclude_pattern(filename))) {
		file = pecalloc(1, sizeof(tc_file), 1);
		file->name = zend_string_copy(filename);
		zend_hash_init(&file->lines, 64, NULL, NULL, 1);
	}

	zend_hash_add_ptr(&tc_files, filename, file);

	return file;
}

static void tc_files_dtor(zval *zv)
{
	tc_file *file = Z_PTR_P(zv);

	if (file) {
		zend_string_release(file->name);
		zend_hash_destroy(&file->lines);
		pefree(file, 1);
	}
}

static tc_unit *tc_install_probes(zend_op_array *op_array, tc_file *file)
{
	zend_arena *arena = zend_arena_create(64 * 1024);
	zend_cfg cfg;
	zend_op *limit = op_array->opcodes + op_array->last;
	tc_unit *unit = pecalloc(1, sizeof(tc_unit), 1);
	zval uncovered;

	ZVAL_LONG(&uncovered, TC_UNCOVERED);

	unit->opcodes = op_array->opcodes;
	unit->file = file;
	unit->last = op_array->last;
	unit->index = pecalloc(op_array->last, sizeof(uint32_t), 1);
	unit->probes = pecalloc(op_array->last, sizeof(tc_probe), 1);

	memset(&cfg, 0, sizeof(zend_cfg));
	zend_build_cfg(&arena, op_array, 0, &cfg);

	for (int i = 0; i < cfg.blocks_count; i++) {
		zend_basic_block *block = &cfg.blocks[i];
		zend_op *opline = op_array->opcodes + block->start;
		zend_op *end = opline + block->len;
		bool block_entry = true;
		uint32_t probed_line = 0;

		/* PCOV does not report unreachable code, and such code cannot run. */
		if (!(block->flags & ZEND_BB_REACHABLE)) {
			continue;
		}

		for (; opline < end; opline++) {
			if (zend_bitset_in(tc_ignored_opcodes, opline->opcode)) {
				/* A call that passes N arguments starts after the Nth RECV opcode. */
				if (opline->opcode == ZEND_RECV || opline->opcode == ZEND_RECV_INIT || opline->opcode == ZEND_RECV_VARIADIC) {
					block_entry = true;
				}

				continue;
			}

			zend_hash_index_add(&file->lines, opline->lineno, &uncovered);

			if (block_entry || opline->lineno != probed_line) {
				tc_probe *probe = &unit->probes[unit->count];

				probe->handler = opline->handler;
				probe->opnum = (uint32_t) (opline - op_array->opcodes);
				probe->line = opline->lineno;
				probe->state = TC_ARMED;

				unit->index[probe->opnum] = ++unit->count;
				opline->handler = tc_probe_label;

				block_entry = false;
				probed_line = opline->lineno;
				tc_stat_probes++;
			}

			/* PCOV does not count the constructor call of a "new" expression as executable. */
			if (opline->opcode == ZEND_NEW && opline + 1 < end && (opline + 1)->opcode == ZEND_DO_FCALL) {
				opline++;
			}
		}

		if (block == cfg.blocks && opline == limit) {
			break;
		}
	}

	zend_arena_destroy(arena);

	return unit;
}

/*
 * pass_two() gave every opcode the ZEND_USER_OPCODE handler, because all user opcode handlers are registered.
 * This function runs while no handler is registered, thus zend_vm_set_opcode_handler() restores the real handlers.
 */
static void tc_process_op_array(zend_op_array *op_array)
{
	tc_file *file;
	tc_unit *unit = NULL;

	if (!ZEND_USER_CODE(op_array->type) || !op_array->opcodes || !(op_array->fn_flags & ZEND_ACC_DONE_PASS_TWO)) {
		return;
	}

	if (op_array->reserved[tc_slot]) {
		return;
	}

	file = tc_file_for(op_array->filename);

	if (file) {
		unit = zend_hash_index_find_ptr(&tc_units, (zend_ulong) (uintptr_t) op_array->opcodes);
	}

	if (!unit) {
		for (uint32_t i = 0; i < op_array->last; i++) {
			zend_vm_set_opcode_handler(&op_array->opcodes[i]);
		}

		if (file && !(op_array->fn_flags & ZEND_ACC_ABSTRACT)) {
			unit = tc_install_probes(op_array, file);
			zend_hash_index_add_ptr(&tc_units, (zend_ulong) (uintptr_t) op_array->opcodes, unit);
		}
	}

	op_array->reserved[tc_slot] = unit ? (void *) unit : TC_NO_PROBES;

	for (uint32_t i = 0; i < op_array->num_dynamic_func_defs; i++) {
		tc_process_op_array(op_array->dynamic_func_defs[i]);
	}
}

static void tc_process_class(zend_class_entry *ce)
{
	zend_function *function;

	if (ce->type != ZEND_USER_CLASS) {
		return;
	}

	ZEND_HASH_MAP_FOREACH_PTR(&ce->function_table, function) {
		if (function->type == ZEND_USER_FUNCTION) {
			tc_process_op_array(&function->op_array);
		}
	} ZEND_HASH_FOREACH_END();

	if (ce->num_hooked_props > 0) {
		zend_property_info *property;

		ZEND_HASH_MAP_FOREACH_PTR(&ce->properties_info, property) {
			if (!property->hooks) {
				continue;
			}

			for (uint32_t i = 0; i < ZEND_PROPERTY_HOOK_COUNT; i++) {
				if (property->hooks[i] && property->hooks[i]->type == ZEND_USER_FUNCTION) {
					tc_process_op_array(&property->hooks[i]->op_array);
				}
			}
		} ZEND_HASH_FOREACH_END();
	}
}

/*
 * ZEND_INCLUDE_OR_EVAL does not execute a file that has only the final "return" opcode, which is the case for a file with declarations only.
 * It does this only with the stock zend_execute_ex. PCOV replaces zend_execute_ex, thus PCOV sees that opcode run and reports the file.
 */
static void tc_record_skipped_return(zend_op_array *op_array)
{
	zend_execute_data *caller = EG(current_execute_data);
	tc_unit *unit = op_array->reserved[tc_slot];
	uint32_t number;

	if ((uintptr_t) unit <= (uintptr_t) TC_NO_PROBES
		|| op_array->last != 1
		|| op_array->opcodes[0].opcode != ZEND_RETURN
		|| op_array->opcodes[0].op1_type != IS_CONST
		|| zend_execute_ex != execute_ex) {
		return;
	}

	if (!caller || !caller->func || !ZEND_USER_CODE(caller->func->type) || !caller->opline || caller->opline->opcode != ZEND_INCLUDE_OR_EVAL) {
		return;
	}

	number = unit->index[0];

	if (number == 0 || unit->probes[number - 1].state != TC_ARMED) {
		return;
	}

	op_array->opcodes[0].handler = unit->probes[number - 1].handler;
	tc_record(unit, number - 1);
}

static void tc_after_compile(zend_op_array *main_op_array, uint32_t first_function, uint32_t first_class)
{
	zend_function *function;
	zend_class_entry *ce;

	if (!tc_active) {
		return;
	}

	tc_unregister_handlers();

	if (main_op_array) {
		tc_process_op_array(main_op_array);
	}

	ZEND_HASH_MAP_FOREACH_PTR_FROM(CG(function_table), function, first_function) {
		if (function->type == ZEND_USER_FUNCTION) {
			tc_process_op_array(&function->op_array);
		}
	} ZEND_HASH_FOREACH_END();

	ZEND_HASH_MAP_FOREACH_PTR_FROM(CG(class_table), ce, first_class) {
		tc_process_class(ce);
	} ZEND_HASH_FOREACH_END();

	tc_register_handlers();

	if (main_op_array) {
		tc_record_skipped_return(main_op_array);
	}
}

static zend_op_array *tc_compile_file(zend_file_handle *file_handle, int type)
{
	uint32_t first_function = CG(function_table)->nNumUsed;
	uint32_t first_class = CG(class_table)->nNumUsed;
	zend_op_array *op_array = tc_orig_compile_file(file_handle, type);

	tc_after_compile(op_array, first_function, first_class);

	return op_array;
}

static zend_op_array *tc_compile_string(zend_string *source_string, const char *filename, zend_compile_position position)
{
	uint32_t first_function = CG(function_table)->nNumUsed;
	uint32_t first_class = CG(class_table)->nNumUsed;
	zend_op_array *op_array = tc_orig_compile_string(source_string, filename, position);

	tc_after_compile(op_array, first_function, first_class);

	return op_array;
}

/* The engine calls this function after it freed the opcodes. Only the address is still valid. */
static void tc_op_array_dtor(zend_op_array *op_array)
{
	tc_unit *unit = op_array->reserved[tc_slot];

	if ((uintptr_t) unit <= (uintptr_t) TC_NO_PROBES) {
		return;
	}

	op_array->reserved[tc_slot] = NULL;

	if (tc_active) {
		zend_hash_index_del(&tc_units, (zend_ulong) (uintptr_t) unit->opcodes);
	}

	unit->opcodes = NULL;

	/* A unit with recorded hits stays until the next clear, because collect needs its lines. */
	if (unit->pending == 0) {
		tc_unit_free(unit);
	}
}

static zend_extension tc_zend_extension;

static void tc_paths_init(tc_paths *paths)
{
	zend_hash_init(&paths->files, 16, NULL, NULL, 1);
	zend_hash_init(&paths->directories, 4, NULL, NULL, 1);
}

static void tc_paths_destroy(tc_paths *paths)
{
	zend_hash_destroy(&paths->files);
	zend_hash_destroy(&paths->directories);
}

/* A path that does not exist is not an error: PHP cannot compile such a file. */
static void tc_paths_add(tc_paths *paths, const char *entry, size_t length)
{
	char path[MAXPATHLEN];
	char resolved[MAXPATHLEN];
	zend_stat_t status;
	size_t resolved_length;

	while (length > 0 && isspace((unsigned char) *entry)) {
		entry++;
		length--;
	}

	while (length > 0 && isspace((unsigned char) entry[length - 1])) {
		length--;
	}

	if (length == 0 || length >= MAXPATHLEN) {
		return;
	}

	memcpy(path, entry, length);
	path[length] = '\0';

	if (!VCWD_REALPATH(path, resolved) || VCWD_STAT(resolved, &status) != 0) {
		return;
	}

	resolved_length = strlen(resolved);

	if (!S_ISDIR(status.st_mode)) {
		zend_hash_str_add_empty_element(&paths->files, resolved, resolved_length);

		return;
	}

	if (resolved[resolved_length - 1] != DEFAULT_SLASH) {
		if (resolved_length + 1 >= MAXPATHLEN) {
			return;
		}

		resolved[resolved_length++] = DEFAULT_SLASH;
		resolved[resolved_length] = '\0';
	}

	zend_hash_str_add_empty_element(&paths->directories, resolved, resolved_length);
}

/* The value is a list of paths, or "@" and the name of a file that has one path on each line. */
static bool tc_paths_load(tc_paths *paths, const char *value)
{
	if (!value || !*value) {
		return true;
	}

	if (*value == '@') {
		char line[MAXPATHLEN + 2];
		FILE *list = VCWD_FOPEN(value + 1, "r");

		if (!list) {
			return false;
		}

		while (fgets(line, sizeof(line), list)) {
			tc_paths_add(paths, line, strlen(line));
		}

		fclose(list);

		return true;
	}

	while (*value) {
		const char *end = strchr(value, ZEND_PATHS_SEPARATOR);
		size_t length = end ? (size_t) (end - value) : strlen(value);

		tc_paths_add(paths, value, length);
		value += length + (end ? 1 : 0);
	}

	return true;
}

static void tc_setup_paths(void)
{
	const char *collect = INI_STR(TC_INI_COLLECT_PATHS);

	tc_paths_init(&tc_collect_paths);
	tc_paths_init(&tc_exclude_paths);

	if (!collect || !*collect) {
		snprintf(tc_start_warning, sizeof(tc_start_warning), TC_INI_COLLECT_PATHS " is not set");
	} else if (!tc_paths_load(&tc_collect_paths, collect)) {
		snprintf(tc_start_warning, sizeof(tc_start_warning), "the list file %s of " TC_INI_COLLECT_PATHS " is not readable", collect + 1);
	}

#ifdef TC_INI_EXCLUDE_PATHS
	if (!tc_paths_load(&tc_exclude_paths, INI_STR(TC_INI_EXCLUDE_PATHS))) {
		snprintf(tc_start_warning, sizeof(tc_start_warning), "the list file %s of " TC_INI_EXCLUDE_PATHS " is not readable", INI_STR(TC_INI_EXCLUDE_PATHS) + 1);
		zend_hash_clean(&tc_collect_paths.files);
		zend_hash_clean(&tc_collect_paths.directories);
	}
#endif
}

static void tc_setup_exclude_pattern(void)
{
	const char *exclude = INI_STR(TC_INI_EXCLUDE_PATTERN);
	zend_string *pattern;

	tc_exclude_pattern = NULL;

	if (!exclude || !*exclude) {
		return;
	}

	pattern = zend_string_init(exclude, strlen(exclude), 0);
	tc_exclude_pattern = pcre_get_compiled_regex_cache(pattern);

	if (tc_exclude_pattern) {
		php_pcre_pce_incref(tc_exclude_pattern);
	}

	zend_string_release(pattern);
}

static void tc_add_file(zval *result, tc_file *file)
{
	zval lines;

	ZVAL_ARR(&lines, zend_array_dup(&file->lines));
	zend_hash_update(Z_ARRVAL_P(result), file->name, &lines);
}

/* {{{ array collect(int $type = all, array $filter = []) */
PHP_NAMED_FUNCTION(tc_function_collect)
{
	zend_long type = TC_FILTER_ALL;
	HashTable *filter = NULL;
	zval *entry;
	zval *known;

	ZEND_PARSE_PARAMETERS_START(0, 2)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(type)
		Z_PARAM_ARRAY_HT(filter)
	ZEND_PARSE_PARAMETERS_END();

	if (type != TC_FILTER_ALL && type != TC_FILTER_INCLUDE && type != TC_FILTER_EXCLUDE) {
		zend_type_error("type must be \\" TURBOCOV_NAME "\\inclusive, \\" TURBOCOV_NAME "\\exclusive, or \\" TURBOCOV_NAME "\\all");
		RETURN_THROWS();
	}

	array_init(return_value);

	if (!tc_active || !tc_dirty) {
		return;
	}

	tc_dirty = false;

	if (type == TC_FILTER_INCLUDE) {
		if (filter) {
			ZEND_HASH_FOREACH_VAL(filter, entry) {
				if (Z_TYPE_P(entry) == IS_STRING && (known = zend_hash_find(&tc_files, Z_STR_P(entry))) && Z_PTR_P(known)) {
					tc_add_file(return_value, Z_PTR_P(known));
				}
			} ZEND_HASH_FOREACH_END();
		}
	} else {
		ZEND_HASH_MAP_FOREACH_VAL(&tc_files, known) {
			tc_file *file = Z_PTR_P(known);
			bool skip = false;

			if (!file) {
				continue;
			}

			if (type == TC_FILTER_EXCLUDE && filter) {
				ZEND_HASH_FOREACH_VAL(filter, entry) {
					if (Z_TYPE_P(entry) == IS_STRING && zend_string_equals(file->name, Z_STR_P(entry))) {
						skip = true;
						break;
					}
				} ZEND_HASH_FOREACH_END();
			}

			if (!skip) {
				tc_add_file(return_value, file);
			}
		} ZEND_HASH_FOREACH_END();
	}

	for (size_t i = 0; i < tc_hits.count; i++) {
		tc_unit *unit = tc_hits.refs[i].unit;
		zval *lines = zend_hash_find(Z_ARRVAL_P(return_value), unit->file->name);
		zval *line;

		if (lines && (line = zend_hash_index_find(Z_ARRVAL_P(lines), unit->probes[tc_hits.refs[i].probe].line))) {
			ZVAL_LONG(line, TC_COVERED);
		}
	}
}
/* }}} */

/* {{{ void start() */
PHP_NAMED_FUNCTION(tc_function_start)
{
	ZEND_PARSE_PARAMETERS_NONE();

	if (*tc_start_warning) {
		zend_error(E_WARNING, TURBOCOV_NAME ": the coverage collection is off, because %s", tc_start_warning);
		*tc_start_warning = '\0';
	}

	if (!tc_active) {
		return;
	}

	tc_refs_rearm(&tc_strays);
	tc_collecting = true;
}
/* }}} */

/* {{{ void stop() */
PHP_NAMED_FUNCTION(tc_function_stop)
{
	ZEND_PARSE_PARAMETERS_NONE();

	tc_collecting = false;
}
/* }}} */

/* {{{ void clear(bool $files = false) */
PHP_NAMED_FUNCTION(tc_function_clear)
{
	bool files = false;

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(files)
	ZEND_PARSE_PARAMETERS_END();

	if (!tc_active) {
		return;
	}

	tc_refs_rearm(&tc_hits);
	tc_waiting_reset();
	tc_dirty = false;
}
/* }}} */

/* {{{ array waiting() */
PHP_NAMED_FUNCTION(tc_function_waiting)
{
	ZEND_PARSE_PARAMETERS_NONE();

	array_init(return_value);

	for (size_t i = 0; i < tc_waiting_count; i++) {
		add_next_index_str(return_value, zend_string_copy(tc_waiting[i]->name));
	}
}
/* }}} */

/* {{{ int memory() */
PHP_NAMED_FUNCTION(tc_function_memory)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_LONG((zend_long) ((tc_hits.capacity + tc_strays.capacity) * sizeof(tc_ref)));
}
/* }}} */

/* {{{ bool enabled() */
PHP_NAMED_FUNCTION(tc_function_enabled)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_BOOL(tc_ini_enabled);
}
/* }}} */

/* {{{ array stats() */
PHP_NAMED_FUNCTION(tc_function_stats)
{
	zval *known;
	zend_long files = 0;

	ZEND_PARSE_PARAMETERS_NONE();

	if (tc_active) {
		ZEND_HASH_MAP_FOREACH_VAL(&tc_files, known) {
			if (Z_PTR_P(known)) {
				files++;
			}
		} ZEND_HASH_FOREACH_END();
	}

	array_init(return_value);
	add_assoc_long(return_value, "files", files);
	add_assoc_long(return_value, "probes", (zend_long) tc_stat_probes);
	add_assoc_long(return_value, "fires", (zend_long) tc_stat_fires);
	add_assoc_long(return_value, "hits", (zend_long) tc_hits.count);
	add_assoc_long(return_value, "strays", (zend_long) tc_strays.count);
}
/* }}} */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(tc_arginfo_collect, 0, 0, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, type, IS_LONG, 0, "0")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, filter, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(tc_arginfo_clear, 0, 0, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, files, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(tc_arginfo_void, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(tc_arginfo_array, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(tc_arginfo_int, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(tc_arginfo_bool, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry tc_functions[] = {
	ZEND_NS_FENTRY(TURBOCOV_NAME, start, tc_function_start, tc_arginfo_void, 0)
	ZEND_NS_FENTRY(TURBOCOV_NAME, stop, tc_function_stop, tc_arginfo_void, 0)
	ZEND_NS_FENTRY(TURBOCOV_NAME, collect, tc_function_collect, tc_arginfo_collect, 0)
	ZEND_NS_FENTRY(TURBOCOV_NAME, clear, tc_function_clear, tc_arginfo_clear, 0)
	ZEND_NS_FENTRY(TURBOCOV_NAME, waiting, tc_function_waiting, tc_arginfo_array, 0)
	ZEND_NS_FENTRY(TURBOCOV_NAME, memory, tc_function_memory, tc_arginfo_int, 0)
	ZEND_NS_FENTRY(TURBOCOV_NAME, enabled, tc_function_enabled, tc_arginfo_bool, 0)
	ZEND_NS_FENTRY(TURBOCOV_NAME, stats, tc_function_stats, tc_arginfo_array, 0)
	PHP_FE_END
};

static void tc_fill_ignored_opcodes(void)
{
	/* The list of PCOV, so that both drivers report the same executable lines. */
	static const uint8_t ignored[] = {
		ZEND_NOP, ZEND_OP_DATA, ZEND_FE_FREE, ZEND_FREE, ZEND_ASSERT_CHECK, ZEND_VERIFY_RETURN_TYPE,
		ZEND_RECV, ZEND_RECV_INIT, ZEND_RECV_VARIADIC, ZEND_SEND_VAL, ZEND_SEND_VAR_EX, ZEND_SEND_REF,
		ZEND_SEND_UNPACK, ZEND_DECLARE_CONST, ZEND_DECLARE_CLASS, ZEND_DECLARE_CLASS_DELAYED,
		ZEND_DECLARE_FUNCTION, ZEND_DECLARE_ANON_CLASS, ZEND_FAST_RET, ZEND_FAST_CALL, ZEND_TICKS,
		ZEND_EXT_STMT, ZEND_EXT_FCALL_BEGIN, ZEND_EXT_FCALL_END, ZEND_EXT_NOP, ZEND_BIND_GLOBAL,
	};

	for (size_t i = 0; i < sizeof(ignored); i++) {
		zend_bitset_incl(tc_ignored_opcodes, ignored[i]);
	}
}

static PHP_MINIT_FUNCTION(turbocov)
{
	REGISTER_NS_LONG_CONSTANT(TURBOCOV_NAME, "all", TC_FILTER_ALL, CONST_CS | CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT(TURBOCOV_NAME, "inclusive", TC_FILTER_INCLUDE, CONST_CS | CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT(TURBOCOV_NAME, "exclusive", TC_FILTER_EXCLUDE, CONST_CS | CONST_PERSISTENT);
	REGISTER_NS_STRING_CONSTANT(TURBOCOV_NAME, "version", PHP_TURBOCOV_VERSION, CONST_CS | CONST_PERSISTENT);

	REGISTER_INI_ENTRIES();

	tc_ini_enabled = INI_BOOL(TURBOCOV_NAME ".enabled");

	if (!tc_ini_enabled) {
		return SUCCESS;
	}

	tc_slot = zend_get_resource_handle(TURBOCOV_NAME);

	if (tc_slot < 0) {
		zend_error(E_CORE_WARNING, TURBOCOV_NAME ": no free op array slot, the extension is off");
		tc_ini_enabled = false;

		return SUCCESS;
	}

	tc_fill_ignored_opcodes();

	memset(&tc_zend_extension, 0, sizeof(zend_extension));
	tc_zend_extension.name = (char *) TURBOCOV_NAME " op array tracker";
	tc_zend_extension.version = (char *) PHP_TURBOCOV_VERSION;
	tc_zend_extension.author = (char *) "";
	tc_zend_extension.URL = (char *) "";
	tc_zend_extension.copyright = (char *) "";
	tc_zend_extension.op_array_dtor = tc_op_array_dtor;
	tc_zend_extension.resource_number = -1;
	zend_register_extension(&tc_zend_extension, NULL);

	return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(turbocov)
{
	if (tc_orig_compile_file) {
		tc_unregister_handlers();
	}

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

/*
 * A probe changes opcodes in place. With OPcache the opcodes of cached files are in its shared memory.
 * In a CLI process that memory is private and writable, thus probes work there.
 * Returns the reason when probes cannot work, else NULL.
 */
static const char *tc_find_opcache_conflict(void)
{
	bool is_cli = strcmp(sapi_module.name, "cli") == 0 || strcmp(sapi_module.name, "phpdbg") == 0;
	const char *value;

	if (!INI_BOOL("opcache.enable") || (strcmp(sapi_module.name, "cli") == 0 && !INI_BOOL("opcache.enable_cli"))) {
		return NULL;
	}

	if (!is_cli) {
		return "OPcache shares the compiled code between the processes of this SAPI";
	}

	if (INI_BOOL("opcache.protect_memory")) {
		return "opcache.protect_memory makes the compiled code read-only";
	}

	value = INI_STR("opcache.file_cache");

	if (value && *value) {
		return "opcache.file_cache stores the compiled code for other processes";
	}

	value = INI_STR("opcache.jit");

	if (value && *value && INI_INT("opcache.jit_buffer_size") > 0
		&& strcmp(value, "disable") != 0 && strcmp(value, "off") != 0 && strcmp(value, "0") != 0) {
		return "the JIT of OPcache is on";
	}

	return NULL;
}

/*
 * This function runs at the first request and not in MINIT.
 * Thus the hooks wrap the hooks of the extensions that started before, and code that OPcache preloads keeps its handlers.
 */
static void tc_install(void)
{
	zend_op probe_op;

	tc_register_handlers();

	/* While the handlers are registered, every opcode gets the ZEND_USER_OPCODE handler. This is the value that a probe needs. */
	memset(&probe_op, 0, sizeof(zend_op));
	probe_op.opcode = ZEND_NOP;
	zend_vm_set_opcode_handler(&probe_op);
	tc_probe_label = probe_op.handler;

	tc_orig_compile_file = zend_compile_file;
	zend_compile_file = tc_compile_file;
	tc_orig_compile_string = zend_compile_string;
	zend_compile_string = tc_compile_string;
}

static PHP_RINIT_FUNCTION(turbocov)
{
	if (!tc_ini_enabled) {
		return SUCCESS;
	}

	if (!tc_orig_compile_file) {
		const char *conflict = tc_find_opcache_conflict();

		if (conflict) {
			/* No message here: most processes do not collect coverage, and a message at startup can break their output. */
			snprintf(tc_start_warning, sizeof(tc_start_warning), "%s", conflict);
			tc_ini_enabled = false;

			return SUCCESS;
		}

		tc_install();
	}

	zend_hash_init(&tc_files, 256, NULL, tc_files_dtor, 1);
	zend_hash_init(&tc_units, 256, NULL, NULL, 1);

	*tc_start_warning = '\0';
	tc_setup_paths();
	tc_setup_exclude_pattern();

	/* A switch with a jump table does not run the comparison of each case. PCOV turns jump tables off too. */
	CG(compiler_options) |= ZEND_COMPILE_NO_JUMPTABLES;

	tc_collecting = false;
	tc_dirty = false;
	tc_active = true;

	return SUCCESS;
}

static PHP_RSHUTDOWN_FUNCTION(turbocov)
{
	if (!tc_active) {
		return SUCCESS;
	}

	/* The engine destroys the op arrays after this point. The op array destructor then frees each unit. */
	tc_active = false;
	tc_collecting = false;

	tc_refs_rearm(&tc_hits);
	tc_refs_rearm(&tc_strays);
	tc_waiting_reset();

	zend_hash_destroy(&tc_units);
	zend_hash_destroy(&tc_files);

	tc_paths_destroy(&tc_collect_paths);
	tc_paths_destroy(&tc_exclude_paths);

	if (tc_exclude_pattern) {
		php_pcre_pce_decref(tc_exclude_pattern);
		tc_exclude_pattern = NULL;
	}

	return SUCCESS;
}

static PHP_MINFO_FUNCTION(turbocov)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "turbocov support", tc_ini_enabled ? "enabled" : "disabled");
	php_info_print_table_row(2, "turbocov version", PHP_TURBOCOV_VERSION);
	php_info_print_table_row(2, "Registered as", TURBOCOV_NAME);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

static const zend_module_dep tc_module_deps[] = {
	ZEND_MOD_REQUIRED("pcre")
	ZEND_MOD_END
};

zend_module_entry turbocov_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	tc_module_deps,
	TURBOCOV_NAME,
	tc_functions,
	PHP_MINIT(turbocov),
	PHP_MSHUTDOWN(turbocov),
	PHP_RINIT(turbocov),
	PHP_RSHUTDOWN(turbocov),
	PHP_MINFO(turbocov),
	PHP_TURBOCOV_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_TURBOCOV
ZEND_GET_MODULE(turbocov)
#endif
