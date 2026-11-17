PHP_ARG_ENABLE([turbocov],
  [whether to enable turbocov],
  [AS_HELP_STRING([--enable-turbocov], [Enable turbocov])])

PHP_ARG_ENABLE([turbocov-impersonate-pcov],
  [whether turbocov registers under the name pcov],
  [AS_HELP_STRING([--enable-turbocov-impersonate-pcov], [Register the extension, its functions and its INI settings under the name pcov])],
  [no], [no])

if test "$PHP_TURBOCOV" != "no"; then
  if test "$PHP_TURBOCOV_IMPERSONATE_PCOV" != "no"; then
    AC_DEFINE([TURBOCOV_IMPERSONATE_PCOV], [1], [Define to 1 to register turbocov under the name pcov.])
  fi

  PHP_NEW_EXTENSION([turbocov], [turbocov.c], [$ext_shared])
fi
