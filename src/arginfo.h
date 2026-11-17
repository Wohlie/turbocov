#ifndef TURBOCOV_ARGINFO_H
#define TURBOCOV_ARGINFO_H

/* The argument information that the function tables of both builds use. */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(tc_arginfo_void, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(tc_arginfo_array, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(tc_arginfo_get_debug_info, 0, 1, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, file, IS_STRING, 0)
ZEND_END_ARG_INFO()

#endif
