
#ifndef PHP_PDI_H
#define PHP_PDI_H

#include "zend_API.h"

extern zend_module_entry pdi_module_entry;
#define phpext_pdi_ptr &pdi_module_entry

#define PHP_PDI_VERSION "0.0.0-dev"

PHP_MINIT_FUNCTION(pdi);
PHP_MSHUTDOWN_FUNCTION(pdi);
PHP_MINFO_FUNCTION(pdi);

#define PDI_FROM_OBJ(obj) (pdi_object_t*)((char*)(obj) - XtOffsetOf(pdi_object_t, std))
#define PDI_FROM_ZVAL(zv) PDI_FROM_OBJ(Z_OBJ_P(zv))

#define PDI_IS_FUNCTION(c)   (c->func != NULL)
#define PDI_IS_SINGLETON(c)  (c->is_singleton)
#define PDI_IS_CREATED(c)	(c->is_created)
#define PDI_IS_FIRST_TIME(c) (!PDI_IS_CREATED(c))

#define PDI_GET_CONCRETE(pdi,a) zend_hash_str_find_ptr(pdi->concretes, ZSTR_VAL(a), ZSTR_LEN(a))

#define PDI_IS_BOUND_ABSTRACT(pdi,a) zend_hash_str_exists(pdi->concretes, ZSTR_VAL(a), ZSTR_LEN(a))

#define PDI_THROW_ERROR_HAS_ARGS_WHEN_NO_CTOR(ce) do { \
	zend_throw_exception_ex( \
		pdi_exception_ce, 0, "Class %s does not have a constructor, so you cannot pass any constructor arguments", ZSTR_VAL(ce->name)); \
} while(0)

typedef struct _pdi_args_t {
	zend_string *name;
	zend_string *abstract;
} pdi_args_t;

typedef struct _pdi_concrete_t {
	zend_class_entry *ce;
	zend_object *func;
	struct {
		uint32_t count;
		pdi_args_t *args;
	} args_info;
	bool is_created;
	bool is_singleton;
} pdi_concrete_t;

typedef struct _pdi_object_t {
	zend_object std;
	HashTable *concretes;
	HashTable *singletons;
	HashTable *swaps;
	bool has_swap;
} pdi_object_t;

#endif