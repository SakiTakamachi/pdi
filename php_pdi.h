
#ifndef PHP_PDI_H
#define PHP_PDI_H

#include "zend_API.h"

extern zend_module_entry pdi_module_entry;
#define phpext_pdi_ptr &pdi_module_entry

#define PHP_PDI_VERSION "0.0.0-dev"

PHP_MINIT_FUNCTION(pdi);
PHP_MSHUTDOWN_FUNCTION(pdi);
PHP_MINFO_FUNCTION(pdi);

typedef struct _pdi_args_t {
	const zend_string *name;
    const zend_string *abstract;
} pdi_args_t;

#define PDI_IS_FUNCTION(c)  (c->func != NULL)
#define PDI_IS_SINGLETON(c) (c->is_singleton)
#define PDI_IS_CREATED(c)   (c->is_created)

#define PDI_INIT_CONCRETE(c,ce,fn,s) do { \
    c = emalloc(sizeof(pdi_concrete_t)); \
    c->ce = ce; \
    c->func = fn; \
    c->is_created = false; \
    c->is_singleton = s; \
    c->args_info.count = 0; \
    c->args_info.args = NULL; \
} while(0)

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

#define PDI_FROM_OBJ(obj) (pdi_object_t*)((char*)(obj) - XtOffsetOf(pdi_object_t, std))
#define PDI_FROM_ZVAL(zv) PDI_FROM_OBJ(Z_OBJ_P(zv))

typedef struct _pdi_object_t {
	zend_object std;
	HashTable *concretes;
    HashTable *singletons;
    HashTable *swaps;
    bool has_swap;
} pdi_object_t;

#define PDI_REGISTER_CONCRETE(pdi,a,c) zend_hash_str_add_ptr(pdi->concretes, ZSTR_VAL(a), ZSTR_LEN(a), c)
#define PDI_REGISTER_SINGLETON(pdi,a,c) zend_hash_str_add_ptr(pdi->singletons, ZSTR_VAL(a), ZSTR_LEN(a), c)

#define PDI_GET_CONCRETE(pdi,a) zend_hash_str_find_ptr(pdi->concretes, ZSTR_VAL(a), ZSTR_LEN(a))
#define PDI_GET_SINGLETON(pdi,a) zend_hash_str_find_ptr(pdi->singletons, ZSTR_VAL(a), ZSTR_LEN(a))

#endif