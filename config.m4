dnl $Id$
dnl config.m4 for extension pdi

sinclude(./autoconf/pecl.m4)
sinclude(./autoconf/php-executable.m4)

PHP_ARG_ENABLE(pdi, whether to enable pdi, [ --enable-pdi   Enable pdi])

if test "$PHP_PDI" != "no"; then
  AC_DEFINE(HAVE_PDI, 1, [whether pdi is enabled])
  PHP_NEW_EXTENSION(pdi, pdi.c, $ext_shared)

  PHP_ADD_MAKEFILE_FRAGMENT
  PHP_INSTALL_HEADERS([ext/pdi], [php_pdi.h])
fi
