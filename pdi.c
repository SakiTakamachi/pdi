
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

static zend_object *pdi_create_object(zend_class_entry *ce)
{
	pdi_object_t *pdi;
    HashTable concretes, singletons, swaps;

	pdi = zend_object_alloc(sizeof(pdi_object_t), ce);
	zend_object_std_init(&pdi->std, ce);
	object_properties_init(&pdi->std, ce);
	rebuild_object_properties(&pdi->std);
	pdi->concretes = &concretes;
    pdi->singletons = &singletons;
    pdi->swaps = &swaps;
    pdi->has_swap = false;

	return &pdi->std;
}

static void pdi_free_object(zend_object *object)
{
	pdi_object_t *pdi = PDI_FROM_OBJ(object);
	pdi->concretes = NULL;
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

static void pdi_register_concrete(INTERNAL_FUNCTION_PARAMETERS, bool is_singleton)
{
	zend_string *abstract, *concrete_str = NULL;
    zend_object *concrete_func = NULL;
    zend_class_entry *ce = NULL;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(abstract)
        Z_PARAM_OBJ_OR_STR(concrete_func, concrete_str)
	ZEND_PARSE_PARAMETERS_END();

    if (concrete_str && UNEXPECTED((ce = zend_lookup_class(concrete_str)) == NULL)) {
        if (!EG(exception)) {
            zend_throw_exception_ex(pdi_exception_ce, 0, "Class \"%s\" does not exist", ZSTR_VAL(concrete_str));
        }
        RETURN_THROWS();
    }

    pdi_concrete_t *concrete;
    PDI_INIT_CONCRETE(concrete, ce, concrete_func, is_singleton);

    pdi_object_t *pdi = PDI_FROM_ZVAL(ZEND_THIS);
    PDI_REGISTER_CONCRETE(pdi, abstract, concrete);
}

PHP_METHOD(Pdi, bind)
{
	pdi_register_concrete(INTERNAL_FUNCTION_PARAM_PASSTHRU, false);
}

PHP_METHOD(Pdi, singleton)
{
	pdi_register_concrete(INTERNAL_FUNCTION_PARAM_PASSTHRU, true);
}

static zval *pdi_create_instance(pdi_object_t *pdi, pdi_concrete_t *concrete, const zend_string *abstract, HashTable *args, bool is_first_time);

static zval *pdi_get_instance(pdi_object_t *pdi, const zend_string *abstract, HashTable *args)
{
    pdi_concrete_t *concrete = zend_hash_str_find_ptr(pdi->concretes, ZSTR_VAL(abstract), ZSTR_LEN(abstract));

    if (concrete == NULL) {
        // TODO: throw exception
        return NULL;
    }

    if (PDI_IS_CREATED(concrete)) {
        if (PDI_IS_SINGLETON(concrete)) {
            return PDI_GET_SINGLETON(pdi, abstract);
        }

        return pdi_create_instance(pdi, concrete, abstract, args, false);
    }

    return pdi_create_instance(pdi, concrete, abstract, args, true);
}

static zval *pdi_create_instance_first_time(
    pdi_object_t *pdi, pdi_concrete_t *concrete, const zend_string *abstract, zval *instance, zend_function *constructor, HashTable *args, int argc)
{
    uint32_t required_num_args = constructor->common.required_num_args;
    zend_arg_info *arg_info = constructor->common.arg_info;
    zend_string *type_str = zend_type_to_string(arg_info->type);
    
    size_t pdi_args_size = sizeof(pdi_args_t);
    pdi_args_t *pdi_args;
    uint32_t count = 0;
    bool use_args = false;

    for (int i = 0; i < required_num_args; i++) {
        if (ZEND_TYPE_HAS_NAME(arg_info[i].type)) {
            if (argc && zend_hash_str_exists(args, ZSTR_VAL(arg_info[i].name), ZSTR_LEN(arg_info[i].name))) {
                use_args = true;
            } else {
                zend_string *arg_abstract = ZEND_TYPE_NAME(arg_info[i].type);
                if (zend_hash_str_exists(pdi->concretes, ZSTR_VAL(arg_abstract), ZSTR_LEN(arg_abstract))) {
                    pdi_args = erealloc(pdi_args, pdi_args_size * (count+1));
                    pdi_args[count].name = arg_info[i].name;
                    pdi_args[count].abstract = arg_abstract;

                    zval *arg_instance = pdi_get_instance(pdi, arg_abstract, NULL);
                    zend_hash_str_add_ptr(args, ZSTR_VAL(arg_info[i].name), ZSTR_LEN(arg_info[i].name), arg_instance);
                    count++;
                }
            }
        }
    }

    zend_call_known_function(
        constructor, Z_OBJ_P(instance), Z_OBJCE_P(instance), NULL, 0, NULL, args);

    if (EG(exception)) {
        zend_object_store_ctor_failed(Z_OBJ_P(instance));
    }

    if (!use_args || PDI_IS_SINGLETON(concrete)) {
        concrete->is_created = true;
        concrete->args_info.count = count;
        concrete->args_info.args = pdi_args;
    }

    if (PDI_IS_SINGLETON(concrete)) {
        PDI_REGISTER_SINGLETON(pdi, abstract, instance);
    }
    
    return instance;
}

static zval *pdi_create_instance_default(
    pdi_object_t *pdi, pdi_concrete_t *concrete, const zend_string *abstract, zval *instance, zend_function *constructor, HashTable *args, int argc)
{
    uint32_t count = concrete->args_info.count;
    pdi_args_t *pdi_args = concrete->args_info.args;

    for (int i = 0; i < count; i++) {
        if (!argc && !zend_hash_str_exists(args, ZSTR_VAL(pdi_args[i].name), ZSTR_LEN(pdi_args[i].name))) {
            zval *arg_instance = pdi_get_instance(pdi, pdi_args[i].abstract, NULL);
            zend_hash_str_add_ptr(args, ZSTR_VAL(pdi_args[i].name), ZSTR_LEN(pdi_args[i].name), arg_instance);
        }
    }

    zend_call_known_function(
        constructor, Z_OBJ_P(instance), Z_OBJCE_P(instance), NULL, 0, NULL, args);

    if (EG(exception)) {
        zend_object_store_ctor_failed(Z_OBJ_P(instance));
    }
    
    return instance;
}

static zval *pdi_create_instance(pdi_object_t *pdi, pdi_concrete_t *concrete, const zend_string *abstract, HashTable *args, bool is_first_time)
{
	zend_class_entry *old_scope, *ce = concrete->ce;
    zval *instance;
    zend_function *constructor;
	int argc = 0;

    if (args) {
		argc = zend_hash_num_elements(args);
	}

	if (UNEXPECTED(object_init_ex(instance, ce) != SUCCESS)) {
		return NULL;
	}

	old_scope = EG(fake_scope);
	EG(fake_scope) = ce;
	constructor = Z_OBJ_HT_P(instance)->get_constructor(Z_OBJ_P(instance));
	EG(fake_scope) = old_scope;

	if (constructor) {
		if (!(constructor->common.fn_flags & ZEND_ACC_PUBLIC)) {
			zend_throw_exception_ex(pdi_exception_ce, 0, "Access to non-public constructor of class %s", ZSTR_VAL(ce->name));
			zval_ptr_dtor(instance);
			return NULL;
		}

        if (is_first_time) {
            return pdi_create_instance_first_time(pdi, concrete, abstract, instance, constructor, args, argc);
        } else {
            return pdi_create_instance_default(pdi, concrete, abstract, instance, constructor, args, argc);
        }

	} else if (argc) {
		zend_throw_exception_ex(
            pdi_exception_ce, 0, "Class %s does not have a constructor, so you cannot pass any constructor arguments", ZSTR_VAL(ce->name));
	}
}

PHP_METHOD(Pdi, make)
{
    zend_string *abstract;
    HashTable *args;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_STR(abstract)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(args)
	ZEND_PARSE_PARAMETERS_END();

    pdi_object_t *pdi = PDI_FROM_ZVAL(ZEND_THIS);
    return_value = pdi_get_instance(pdi, abstract, args);
}

#endif
