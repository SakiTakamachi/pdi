/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: b3efc046ecee13ee2e88e8059caffbef563063b9 */

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_Pdi_container, 0, 1, Pdi, 0)
	ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Pdi_bind, 0, 2, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, abstract, IS_STRING, 0)
	ZEND_ARG_TYPE_MASK(0, concrete, MAY_BE_CALLABLE|MAY_BE_STRING, NULL)
ZEND_END_ARG_INFO()

#define arginfo_class_Pdi_singleton arginfo_class_Pdi_bind

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Pdi_make, 0, 1, IS_OBJECT, 0)
	ZEND_ARG_TYPE_INFO(0, abstract, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, parameters, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Pdi_swap, 0, 2, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, abstract, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, instance, IS_OBJECT, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Pdi_clearSwap, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_METHOD(Pdi, container);
ZEND_METHOD(Pdi, bind);
ZEND_METHOD(Pdi, singleton);
ZEND_METHOD(Pdi, make);
ZEND_METHOD(Pdi, swap);
ZEND_METHOD(Pdi, clearSwap);

static const zend_function_entry class_Pdi_methods[] = {
	ZEND_ME(Pdi, container, arginfo_class_Pdi_container, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(Pdi, bind, arginfo_class_Pdi_bind, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdi, singleton, arginfo_class_Pdi_singleton, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdi, make, arginfo_class_Pdi_make, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdi, swap, arginfo_class_Pdi_swap, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdi, clearSwap, arginfo_class_Pdi_clearSwap, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry class_PdiException_methods[] = {
	ZEND_FE_END
};

static zend_class_entry *register_class_Pdi(void)
{
	zend_class_entry ce, *class_entry;

	INIT_CLASS_ENTRY(ce, "Pdi", class_Pdi_methods);
	class_entry = zend_register_internal_class_ex(&ce, NULL);

	return class_entry;
}

static zend_class_entry *register_class_PdiException(zend_class_entry *class_entry_RuntimeException)
{
	zend_class_entry ce, *class_entry;

	INIT_CLASS_ENTRY(ce, "PdiException", class_PdiException_methods);
	class_entry = zend_register_internal_class_ex(&ce, class_entry_RuntimeException);

	return class_entry;
}
