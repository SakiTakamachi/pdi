
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_closures.h"

#ifdef HAVE_PDI

#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_pdi.h"
#include "pdi_arginfo.h"
#include "zend_exceptions.h"
#include "ext/spl/spl_exceptions.h"

static zend_class_entry *pdi_ce, *pdi_exception_ce;
static zend_object_handlers pdi_obj_handlers;

/* {{{ PHP_INI */
PHP_INI_BEGIN()
PHP_INI_END()
/* }}} */

zend_module_entry pdi_module_entry = {
	STANDARD_MODULE_HEADER,
	"pdi",
	NULL,
	PHP_MINIT(pdi),
	PHP_MSHUTDOWN(pdi),
	NULL,
	NULL,
	PHP_MINFO(pdi),
	PHP_PDI_VERSION,
	STANDARD_MODULE_PROPERTIES
};

static void pdi_register_class(void);

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(pdi)
{
	REGISTER_INI_ENTRIES();
	pdi_register_class();
	pdi_exception_ce = register_class_PdiException(spl_ce_RuntimeException);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(pdi)
{
	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(pdi)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "pdi support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

static void pdi_concretes_dtor(zval *zv)
{
	pdi_concrete_t *concrete = (pdi_concrete_t*) Z_PTR_P(zv);
	if (concrete->args_info.args) {
		efree(concrete->args_info.args);
	}
	efree(concrete);
}

static zend_object *pdi_create_object(zend_class_entry *ce)
{
	pdi_object_t *pdi;

	pdi = zend_object_alloc(sizeof(pdi_object_t), ce);
	zend_object_std_init(&pdi->std, ce);
	object_properties_init(&pdi->std, ce);
	rebuild_object_properties(&pdi->std);

	ALLOC_HASHTABLE(pdi->concretes);
	ALLOC_HASHTABLE(pdi->singletons);
	//ALLOC_HASHTABLE(pdi->swaps);

	zend_hash_init(pdi->concretes, 0, NULL, pdi_concretes_dtor, false);
	zend_hash_init(pdi->singletons, 0, NULL, ZVAL_PTR_DTOR, false);
	//zend_hash_init(pdi->swaps, 0, NULL, ZVAL_PTR_DTOR, false);

	pdi->has_swap = false;

	return &pdi->std;
}

static void pdi_free_object(zend_object *object)
{
	pdi_object_t *pdi = PDI_FROM_OBJ(object);
	zval *zv;

	if (pdi->concretes) {
		zend_hash_destroy(pdi->concretes);
		FREE_HASHTABLE(pdi->concretes);
	}

	if (pdi->singletons) {
		zend_hash_destroy(pdi->singletons);
		FREE_HASHTABLE(pdi->singletons);
	}

	if (pdi->swaps) {
		FREE_HASHTABLE(pdi->swaps);
	}

	pdi->concretes = NULL;
	pdi->singletons = NULL;
	pdi->swaps = NULL;
	zend_object_std_dtor(&pdi->std);
}

static HashTable *pdi_get_gc(zend_object *object, zval **table, int *n)
{
	*table = NULL;
	*n = 0;
	return zend_std_get_properties(object);
}

static void pdi_register_class(void)
{
	pdi_ce = register_class_Pdi();
	pdi_ce->create_object = pdi_create_object;
	pdi_ce->default_object_handlers = &pdi_obj_handlers;

	memcpy(&pdi_obj_handlers, &std_object_handlers, sizeof(zend_object_handlers));
	pdi_obj_handlers.offset = XtOffsetOf(pdi_object_t, std);
	pdi_obj_handlers.free_obj = pdi_free_object;
	pdi_obj_handlers.clone_obj = NULL;
	pdi_obj_handlers.compare = zend_objects_not_comparable;
	pdi_obj_handlers.get_gc = pdi_get_gc;
}

PHP_METHOD(Pdi, __construct)
{
	ZEND_PARSE_PARAMETERS_NONE();
}

static void pdi_register_concrete_ex(
	pdi_object_t *pdi, zend_string *abstract, zend_object *concrete_func, zend_string *concrete_str, bool is_singleton)
{
	zend_class_entry *ce = NULL;

	if (PDI_IS_BOUND_ABSTRACT(pdi, abstract)) {
		zend_throw_exception_ex(pdi_exception_ce, 0, "($abstract) \"%s\" is already bound.", ZSTR_VAL(concrete_str));
		return;
	}

	if (concrete_str && UNEXPECTED((ce = zend_lookup_class(concrete_str)) == NULL))  {
		if (!EG(exception)) {
			zend_throw_exception_ex(pdi_exception_ce, 0, "Class \"%s\" does not exist", ZSTR_VAL(concrete_str));
		}
		return;
	}

	pdi_concrete_t *concrete;
	PDI_INIT_CONCRETE(concrete, ce, concrete_func, is_singleton);
	PDI_REGISTER_CONCRETE(pdi, abstract, concrete);
}

static zend_result pdi_register_concrete_auto(pdi_object_t *pdi, zend_string *concrete_str)
{
	zend_class_entry *ce = NULL;

	if (UNEXPECTED((ce = zend_lookup_class(concrete_str)) == NULL))  {
		return FAILURE;
	}

	if (ce->ce_flags & (ZEND_ACC_INTERFACE | ZEND_ACC_TRAIT | ZEND_ACC_EXPLICIT_ABSTRACT_CLASS | ZEND_ACC_IMPLICIT_ABSTRACT_CLASS | ZEND_ACC_ENUM)) {
		return FAILURE;
	}

	if (!ce->constructor || ce->constructor->common.fn_flags & ZEND_ACC_PUBLIC) {
		goto register_concrete;
	}

	return FAILURE;

register_concrete:;
	pdi_concrete_t *concrete;
	PDI_INIT_CONCRETE(concrete, ce, NULL, false);
	PDI_REGISTER_CONCRETE(pdi, concrete_str, concrete);
	return SUCCESS;
}

static void pdi_bind(INTERNAL_FUNCTION_PARAMETERS, bool is_singleton)
{
	zend_string *abstract, *concrete_str = NULL;
	zend_object *concrete_func = NULL;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(abstract)
		Z_PARAM_OBJ_OR_STR(concrete_func, concrete_str)
	ZEND_PARSE_PARAMETERS_END();

	pdi_object_t *pdi = PDI_FROM_ZVAL(ZEND_THIS);

	pdi_register_concrete_ex(pdi, abstract, concrete_func, concrete_str, is_singleton);

	if (EG(exception)) {
		RETURN_THROWS();
	}
}

PHP_METHOD(Pdi, bind)
{
	pdi_bind(INTERNAL_FUNCTION_PARAM_PASSTHRU, false);
}

PHP_METHOD(Pdi, singleton)
{
	pdi_bind(INTERNAL_FUNCTION_PARAM_PASSTHRU, true);
}

#define PDI_GET_CTOR(ins,ce,ctor) do { \
	zend_class_entry *old_scope; \
	old_scope = EG(fake_scope); \
	EG(fake_scope) = ce; \
	ctor = Z_OBJ_HT_P(ins)->get_constructor(Z_OBJ_P(ins)); \
	EG(fake_scope) = old_scope; \
} while (0)

static zend_result pdi_create_instance(pdi_object_t *pdi, pdi_concrete_t *concrete, zend_string *abstract, HashTable *args, zval *instance);

static zend_result pdi_get_singleton(pdi_object_t *pdi, zend_string *abstract, zval *instance)
{
    zval *obj = zend_hash_find_known_hash(pdi->singletons, abstract);
    ZVAL_OBJ(instance, Z_OBJ_P(obj));
	return SUCCESS;
}

static zend_result pdi_get_instance(pdi_object_t *pdi, zend_string *abstract, HashTable *args, zval *instance)
{
	pdi_concrete_t *concrete;

retry:
	concrete = PDI_GET_CONCRETE(pdi, abstract);

	if (concrete == NULL) {
		if (pdi_register_concrete_auto(pdi, abstract) == SUCCESS) {
			goto retry;
		}
		// TODO: throw exception
		zend_throw_exception_ex(pdi_exception_ce, 0, "no concrete");
		return FAILURE;
	}

	if (PDI_IS_CREATED(concrete) && PDI_IS_SINGLETON(concrete)) {
		return pdi_get_singleton(pdi, abstract, instance);
	}

	return pdi_create_instance(pdi, concrete, abstract, args, instance);
}

static zend_result pdi_create_instance_first_time(
	pdi_object_t *pdi, pdi_concrete_t *concrete, zend_string *abstract, zval *instance, zend_function *constructor, HashTable *args, int argc)
{
	zend_class_entry *ce = concrete->ce;

	if (constructor) {
		if (!(constructor->common.fn_flags & ZEND_ACC_PUBLIC)) {
			zend_throw_exception_ex(pdi_exception_ce, 0, "Access to non-public constructor of class %s", ZSTR_VAL(ce->name));
			zval_ptr_dtor(instance);
			return FAILURE;
		}

		uint32_t required_num_args = constructor->common.required_num_args;
		zend_arg_info *arg_info = constructor->common.arg_info;

		size_t pdi_args_size = sizeof(pdi_args_t);
		concrete->args_info.args = emalloc(pdi_args_size);
		uint32_t count = 0;

		for (int i = 0; i < required_num_args; i++) {
			zend_type *type = &arg_info[i].type;
			if (ZEND_TYPE_HAS_NAME(*type) && !ZEND_TYPE_ALLOW_NULL(*type)) {
				zend_string *arg_abstract = ZEND_TYPE_NAME(arg_info[i].type);
				if (zend_hash_str_exists(pdi->concretes, ZSTR_VAL(arg_abstract), ZSTR_LEN(arg_abstract))) {
				retry:
					count++;
					uint32_t index = count - 1;

					pdi_args_t *pdi_args = emalloc(pdi_args_size);
					pdi_args->name = arg_info[i].name;
					pdi_args->abstract = arg_abstract;

					concrete->args_info.args = (pdi_args_t*) erealloc(concrete->args_info.args, pdi_args_size*count);
					memcpy(&concrete->args_info.args[index], pdi_args, pdi_args_size);
					efree(pdi_args);

					if (!zend_hash_str_exists(args, ZSTR_VAL(arg_info[i].name), ZSTR_LEN(arg_info[i].name))) {
						zval arg_instance;
						pdi_get_instance(pdi, arg_abstract, NULL, &arg_instance);
						zend_hash_update(args, arg_info[i].name, &arg_instance);
					}
				} else if (pdi_register_concrete_auto(pdi, arg_abstract) == SUCCESS) {
					goto retry;
				}
			}
		}

		zend_call_known_function(
			constructor, Z_OBJ_P(instance), Z_OBJCE_P(instance), NULL, 0, NULL, argc == 0 && count == 0 ? NULL : args);

		if (EG(exception)) {
			efree(concrete->args_info.args);
			zend_object_store_ctor_failed(Z_OBJ_P(instance));
		}

		concrete->is_created = true;
		concrete->has_ctor = true;
		concrete->args_info.count = count;
	} else if (argc) {
		PDI_THROW_ERROR_HAS_ARGS_WHEN_NO_CTOR(ce);
	} else {
		concrete->is_created = true;
		concrete->has_ctor = false;
	}

	if (concrete->is_singleton) {
		zend_hash_update(pdi->singletons, abstract, instance);
	}

	return SUCCESS;
}

static zend_result pdi_create_instance_with_deps(
	pdi_object_t *pdi, pdi_concrete_t *concrete, zend_string *abstract, zval *instance, zend_function *constructor, HashTable *args, int argc)
{
	zend_class_entry *ce = concrete->ce;

	if (constructor) {
		uint32_t count = concrete->args_info.count;

		for (int i = 0; i < count; i++) {
			if (!zend_hash_str_exists(args, ZSTR_VAL(concrete->args_info.args[i].name), ZSTR_LEN(concrete->args_info.args[i].name))) {
				zval arg_instance;
				pdi_get_instance(pdi, concrete->args_info.args[i].abstract, NULL, &arg_instance);
				zend_hash_update(args, concrete->args_info.args[i].name, &arg_instance);
			}
		}

		zend_call_known_function(
			constructor, Z_OBJ_P(instance), Z_OBJCE_P(instance), NULL, 0, NULL, args ?: NULL);

		if (EG(exception)) {
			zend_object_store_ctor_failed(Z_OBJ_P(instance));
		}
	} else if (argc) {
		PDI_THROW_ERROR_HAS_ARGS_WHEN_NO_CTOR(ce);
	}

	return SUCCESS;
}

static zend_result pdi_create_instance(pdi_object_t *pdi, pdi_concrete_t *concrete, zend_string *abstract, HashTable *args, zval *instance)
{
	zend_class_entry *ce = concrete->ce;
	zend_function *constructor;
	int argc = 0;
	HashTable *args_internal = NULL;

	if (args) {
		argc = zend_hash_num_elements(args);
	}

	if (concrete->func != NULL) {
		zend_function *closure = zend_get_closure_invoke_method(concrete->func);
		uint32_t closure_req_num_args = closure->common.required_num_args;

		zval pdi_instance;
		ZVAL_OBJ_COPY(&pdi_instance, &(pdi->std));

		zval params[] = { pdi_instance };

		zend_call_known_function(closure, concrete->func, NULL, instance, 1, params, NULL);

		return SUCCESS;
	}

	if (UNEXPECTED(object_init_ex(instance, concrete->ce) != SUCCESS)) {
		return FAILURE;
	}

	PDI_GET_CTOR(instance, ce, constructor);

	if (constructor) {
		ALLOC_HASHTABLE(args_internal);
		zend_hash_init(args_internal, 0, NULL, ZVAL_PTR_DTOR, false);
		if (args) {
			zend_hash_copy(args_internal, args, NULL);
		}
	}

	if (PDI_IS_FIRST_TIME(concrete)) {
		pdi_create_instance_first_time(pdi, concrete, abstract, instance, constructor, args_internal, argc);
		goto end;
	}

	if (PDI_HAS_CTOR(concrete)) {
		pdi_create_instance_with_deps(pdi, concrete, abstract, instance, constructor, args_internal, argc);
		goto end;
	} else if (argc) {
		zend_throw_exception_ex(
			pdi_exception_ce, 0, "Class %s does not have a constructor, so you cannot pass any constructor arguments", ZSTR_VAL(ce->name));
	}

end:
	if (constructor) {
		zend_hash_destroy(args_internal);
		FREE_HASHTABLE(args_internal);
	}

	return SUCCESS;
}

PHP_METHOD(Pdi, make)
{
	zend_string *abstract;
	HashTable *args = NULL;
	zval *instance;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_STR(abstract)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY_HT(args)
	ZEND_PARSE_PARAMETERS_END();

	pdi_object_t *pdi = PDI_FROM_ZVAL(ZEND_THIS);

	if (pdi_get_instance(pdi, abstract, args, return_value) == FAILURE) {
		zend_throw_exception_ex(pdi_exception_ce, 0, "no instance");
	}

	if (EG(exception)) {
		RETURN_THROWS();
	}
}

ZEND_GET_MODULE(pdi)
#endif
