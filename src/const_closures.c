#include "turbocov_internal.h"

/*
 * Since PHP 8.5 a constant expression can contain a closure: a parameter default, an attribute argument,
 * the value of a constant, the default of a property. The op array of such a closure is a node of the constant AST.
 * It is not in the dynamic function definitions of an op array, and no table of the engine lists it.
 */
#if PHP_VERSION_ID >= 80500

#include "zend_ast.h"
#include "zend_attributes.h"
#include "zend_closures.h"

static void tc_visit_ast(zend_ast *ast, tc_op_array_visitor visitor, void *context)
{
	if (!ast) {
		return;
	}

	if (ast->kind == ZEND_AST_OP_ARRAY) {
		visitor(zend_ast_get_op_array(ast)->op_array, context);

		return;
	}

	if (zend_ast_is_special(ast)) {
		return;
	}

	if (zend_ast_is_list(ast)) {
		zend_ast_list *list = zend_ast_get_list(ast);

		for (uint32_t i = 0; i < list->children; i++) {
			tc_visit_ast(list->child[i], visitor, context);
		}

		return;
	}

	for (uint32_t i = 0; i < zend_ast_get_num_children(ast); i++) {
		tc_visit_ast(ast->child[i], visitor, context);
	}
}

static void tc_visit_value(zval *value, tc_op_array_visitor visitor, void *context)
{
	if (Z_TYPE_P(value) == IS_CONSTANT_AST) {
		tc_visit_ast(Z_ASTVAL_P(value), visitor, context);
	} else if (Z_TYPE_P(value) == IS_OBJECT && Z_OBJCE_P(value) == zend_ce_closure) {
		/* The engine replaced the AST of a constant with its value. */
		zend_function *function = (zend_function *) zend_get_closure_method_def(Z_OBJ_P(value));

		if (function->type == ZEND_USER_FUNCTION) {
			visitor(&function->op_array, context);
		}
	}
}

static void tc_visit_attributes(HashTable *attributes, tc_op_array_visitor visitor, void *context)
{
	zend_attribute *attribute;

	if (!attributes) {
		return;
	}

	ZEND_HASH_FOREACH_PTR(attributes, attribute) {
		for (uint32_t i = 0; i < attribute->argc; i++) {
			tc_visit_value(&attribute->args[i].value, visitor, context);
		}
	} ZEND_HASH_FOREACH_END();
}

void tc_visit_op_array_closures(zend_op_array *op_array, tc_op_array_visitor visitor, void *context)
{
	for (uint32_t i = 0; i < (uint32_t) op_array->last_literal; i++) {
		tc_visit_value(&op_array->literals[i], visitor, context);
	}

	if (op_array->static_variables) {
		zval *value;

		ZEND_HASH_FOREACH_VAL(op_array->static_variables, value) {
			tc_visit_value(value, visitor, context);
		} ZEND_HASH_FOREACH_END();
	}

	/* The attributes of the function and of its parameters. */
	tc_visit_attributes(op_array->attributes, visitor, context);

	/* The attributes of a global constant are a pointer in a literal of the opcode that declares the constant. */
	if (op_array->fn_flags & ZEND_ACC_PTR_OPS) {
		for (uint32_t i = 0; i + 1 < op_array->last; i++) {
			zend_op *opline = &op_array->opcodes[i];

			if (opline->opcode == ZEND_DECLARE_ATTRIBUTED_CONST && (opline + 1)->op1_type == IS_CONST) {
				tc_visit_attributes(Z_PTR_P(RT_CONSTANT(opline + 1, (opline + 1)->op1)), visitor, context);
			}
		}
	}
}

void tc_visit_class_closures(zend_class_entry *ce, tc_op_array_visitor visitor, void *context)
{
	zend_class_constant *constant;
	zend_property_info *property;

	tc_visit_attributes(ce->attributes, visitor, context);

	ZEND_HASH_MAP_FOREACH_PTR(&ce->constants_table, constant) {
		tc_visit_value(&constant->value, visitor, context);
		tc_visit_attributes(constant->attributes, visitor, context);
	} ZEND_HASH_FOREACH_END();

	ZEND_HASH_MAP_FOREACH_PTR(&ce->properties_info, property) {
		tc_visit_attributes(property->attributes, visitor, context);
	} ZEND_HASH_FOREACH_END();

	/* The type of the two counts is int before PHP 8.6 and uint32_t since PHP 8.6. */
	for (uint32_t i = 0; i < (uint32_t) ce->default_properties_count; i++) {
		tc_visit_value(&ce->default_properties_table[i], visitor, context);
	}

	for (uint32_t i = 0; i < (uint32_t) ce->default_static_members_count; i++) {
		tc_visit_value(&ce->default_static_members_table[i], visitor, context);
	}
}

#endif
