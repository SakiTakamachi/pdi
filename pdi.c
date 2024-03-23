
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

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

static void pdi_concretes_dtor(pdi_concrete_t *concrete)
{
	if (concrete->args_info.args) {
		efree(concrete->args_info.args);
	}
	efree(concrete);
}

static void pdi_concretes_dtor_from_zval(zval *zv)
{
	pdi_concrete_t *concrete = (pdi_concrete_t*) Z_PTR_P(zv);
	pdi_concretes_dtor(concrete);
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

	zend_hash_init(pdi->concretes, 0, NULL, pdi_concretes_dtor_from_zval, false);
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

static pdi_concrete_t *pdi_auto_bind(pdi_object_t *pdi, zend_string *concrete_str)
{
	zend_class_entry *ce = NULL;

	if (UNEXPECTED((ce = zend_lookup_class(concrete_str)) == NULL))  {
		if (!EG(exception)) {
			zend_throw_exception_ex(pdi_exception_ce, 0, "Class \"%s\" does not exist", ZSTR_VAL(concrete_str));
		}
		return NULL;
	}

	pdi_concrete_t *concrete = emalloc(sizeof(pdi_concrete_t));
	concrete->ce = ce;
	concrete->func = NULL;
	concrete->is_created = false;
	concrete->is_singleton = false;
	concrete->args_info.count = 0;
	concrete->args_info.args = NULL;

	zend_hash_str_add_ptr(pdi->concretes, ZSTR_VAL(concrete_str), ZSTR_LEN(concrete_str), concrete);

	return concrete;
}

static void pdi_bind(INTERNAL_FUNCTION_PARAMETERS, bool is_singleton)
{
	zend_string *abstract, *concrete_str = NULL;
	zend_object *concrete_func = NULL;
	zend_class_entry *ce = NULL;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(abstract)
		Z_PARAM_OBJ_OR_STR(concrete_func, concrete_str)
	ZEND_PARSE_PARAMETERS_END();

	if (concrete_str && UNEXPECTED((ce = zend_lookup_class(concrete_str)) == NULL))  {
		if (!EG(exception)) {
			zend_throw_exception_ex(pdi_exception_ce, 0, "Class \"%s\" does not exist", ZSTR_VAL(concrete_str));
		}
		RETURN_THROWS();
	}

	pdi_object_t *pdi = PDI_FROM_ZVAL(ZEND_THIS);
	pdi_concrete_t *concrete = emalloc(sizeof(pdi_concrete_t));
	concrete->ce = ce;
	concrete->func = concrete_func;
	concrete->is_created = false;
	concrete->is_singleton = is_singleton;
	concrete->args_info.count = 0;
	concrete->args_info.args = NULL;

	pdi_concrete_t *old = zend_hash_str_find_ptr(pdi->concretes, ZSTR_VAL(abstract), ZSTR_LEN(abstract));

	if (old == NULL) {
		zend_hash_str_add_ptr(pdi->concretes, ZSTR_VAL(abstract), ZSTR_LEN(abstract), concrete);
	} else {
		zend_hash_str_update_ptr(pdi->concretes, ZSTR_VAL(abstract), ZSTR_LEN(abstract), concrete);
		pdi_concretes_dtor(old);
	}

	if (UNEXPECTED(EG(exception))) {
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

static zval *pdi_create_instance(pdi_object_t *pdi, pdi_concrete_t *concrete, zend_string *abstract, HashTable *args);

static zval *pdi_get_singleton(pdi_object_t *pdi, zend_string *abstract)
{
	zval *obj = zend_hash_find_known_hash(pdi->singletons, abstract);
	ZEND_ASSERT(obj != NULL);
	return obj;
}

static zval *pdi_get_instance(pdi_object_t *pdi, zend_string *abstract, HashTable *args)
{
	pdi_concrete_t *concrete = zend_hash_str_find_ptr(pdi->concretes, ZSTR_VAL(abstract), ZSTR_LEN(abstract));

	if (concrete == NULL) {
		concrete = pdi_auto_bind(pdi, abstract);
		if (UNEXPECTED(concrete == NULL)) {
			return NULL;
		}
	} else if (PDI_IS_CREATED(concrete) && PDI_IS_SINGLETON(concrete)) {
		return pdi_get_singleton(pdi, abstract);
	}

	return pdi_create_instance(pdi, concrete, abstract, args);
}

static void pdi_create_args_first_time(pdi_object_t *pdi, pdi_concrete_t *concrete, zend_function *constructor, HashTable *args)
{
	uint32_t required_num_args = constructor->common.required_num_args;
	zend_arg_info *arg_info = constructor->common.arg_info;

	size_t pdi_args_size = sizeof(pdi_args_t);
	concrete->args_info.args = emalloc(pdi_args_size);
	uint32_t count = 0;

	for (int i = 0; i < required_num_args; i++) {
		zend_type *type = &arg_info[i].type;
		if (ZEND_TYPE_HAS_NAME(*type) && !ZEND_TYPE_ALLOW_NULL(*type)) {
			zend_string *arg_abstract = ZEND_TYPE_NAME(arg_info[i].type);
			count++;
			uint32_t index = count - 1;

			pdi_args_t *pdi_args = emalloc(pdi_args_size);
			pdi_args->name = arg_info[i].name;
			pdi_args->abstract = arg_abstract;

			concrete->args_info.args = (pdi_args_t*) erealloc(concrete->args_info.args, pdi_args_size*count);
			memcpy(&concrete->args_info.args[index], pdi_args, pdi_args_size);
			efree(pdi_args);

			if (!zend_hash_str_exists(args, ZSTR_VAL(arg_info[i].name), ZSTR_LEN(arg_info[i].name))) {
				zval *tmp = pdi_get_instance(pdi, arg_abstract, NULL);
				if (EXPECTED(tmp != NULL)) {
					zval arg_instance;
					ZVAL_OBJ(&arg_instance, Z_OBJ_P(tmp));
					zend_hash_update(args, arg_info[i].name, &arg_instance);
				}
			}
		}
	}
	concrete->args_info.count = count;
}

static zval *pdi_create_instance(pdi_object_t *pdi, pdi_concrete_t *concrete, zend_string *abstract, HashTable *args)
{
	zval zv;
	zval *instance = &zv;

	ZEND_ASSERT(concrete != NULL);
	zend_class_entry *ce = concrete->ce;
	uint32_t argc = 0;

	if (args) {
		argc = zend_hash_num_elements(args);
	}

	if (UNEXPECTED(object_init_ex(instance, concrete->ce) != SUCCESS)) {
		return NULL;
	}

	if (ce->constructor) {
		zend_class_entry *old_scope;
		zend_function *constructor;
		HashTable *internal_args = NULL;
		uint32_t internal_argc = argc + 1;
		uint32_t dependency_count = 0;

		// TODO: check if this is correct
		old_scope = EG(fake_scope);
		EG(fake_scope) = ce;
		constructor = Z_OBJ_HT_P(instance)->get_constructor(Z_OBJ_P(instance));
		EG(fake_scope) = old_scope;

		ALLOC_HASHTABLE(internal_args);
		zend_hash_init(internal_args, 0, NULL, ZVAL_PTR_DTOR, false);
		if (args) {
			zend_hash_copy(internal_args, args, NULL);
		}

		if (PDI_IS_FIRST_TIME(concrete)) {
			pdi_create_args_first_time(pdi, concrete, constructor, internal_args);
		} else {
			uint32_t count = concrete->args_info.count;
			for (uint32_t i = 0; i < count; i++) {
				if (!zend_hash_str_exists(internal_args, ZSTR_VAL(concrete->args_info.args[i].name), ZSTR_LEN(concrete->args_info.args[i].name))) {
					zval *tmp = pdi_get_instance(pdi, concrete->args_info.args[i].abstract, NULL);

					if (EXPECTED(tmp != NULL)) {
						zval arg_instance;
						ZVAL_OBJ(&arg_instance, Z_OBJ_P(tmp));
						zend_hash_update(internal_args, concrete->args_info.args[i].name, &arg_instance);
						internal_argc++;
					}
				}
			}
		}

		zend_call_known_function(
			constructor, Z_OBJ_P(instance), Z_OBJCE_P(instance), NULL, 0, NULL, internal_argc > 0 ? internal_args : NULL);

		if (EG(exception)) {
			zend_object_store_ctor_failed(Z_OBJ_P(instance));
		}

		if (PDI_IS_FIRST_TIME(concrete)) {
			concrete->args_info.count = dependency_count;
		}

		zend_hash_destroy(internal_args);
		FREE_HASHTABLE(internal_args);
	} else if (args && UNEXPECTED(argc > 0)) {
		zend_throw_exception_ex(
			pdi_exception_ce, 0, "Class %s does not have a constructor, so you cannot pass any constructor arguments", ZSTR_VAL(ce->name));
		return NULL;
	}

	if (PDI_IS_FIRST_TIME(concrete)) {
		if (concrete->is_singleton) {
			GC_ADDREF(Z_OBJ_P(instance));
			zend_hash_update(pdi->singletons, abstract, instance);
		}
		concrete->is_created = true;
	}

	return instance;
}

PHP_METHOD(Pdi, make)
{
	zend_string *abstract;
	HashTable *args = NULL;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_STR(abstract)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY_HT(args)
	ZEND_PARSE_PARAMETERS_END();

	pdi_object_t *pdi = PDI_FROM_ZVAL(ZEND_THIS);
	zval *instance = pdi_get_instance(pdi, abstract, args);
	if (UNEXPECTED(instance == NULL && !EG(exception))) {
		zend_throw_exception_ex(pdi_exception_ce, 0, "Unknown error. Could not create instance.");
	}

	if (UNEXPECTED(EG(exception))) {
		RETURN_THROWS();
	}

	ZVAL_OBJ(return_value, Z_OBJ_P(instance));
}

ZEND_GET_MODULE(pdi)
#endif
