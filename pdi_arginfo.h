/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 42332e75b27e598315f00b9988eba622da01d54f */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Pdi___construct, 0, 0, 0)
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

ZEND_METHOD(Pdi, __construct);
ZEND_METHOD(Pdi, bind);
ZEND_METHOD(Pdi, singleton);
ZEND_METHOD(Pdi, make);

static const zend_function_entry class_Pdi_methods[] = {
	ZEND_ME(Pdi, __construct, arginfo_class_Pdi___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdi, bind, arginfo_class_Pdi_bind, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdi, singleton, arginfo_class_Pdi_singleton, ZEND_ACC_PUBLIC)
	ZEND_ME(Pdi, make, arginfo_class_Pdi_make, ZEND_ACC_PUBLIC)
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
