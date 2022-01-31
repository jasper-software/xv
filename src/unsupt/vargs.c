/*
 * vargs.c   -  a simple program that tests the 'varargs' handling on your
 *              compiler.  Many folks probably have a 'gcc' that has improperly
 *              installed include files.  On such systems, this program will
 *              not work (nor will *any* 'varargs' using program compiled with
 *              gcc, most notably XV).  If such is the case, get your sysadmin
 *              to fix the gcc installation on your machine.  Or use cc.
 */


#include <stdio.h>
#include <varargs.h>

void test();

main()
{
  test(1, "foobie", 0);
}

void test(va_alist)
va_dcl
{
  va_list args;
  int i;
  char *foo;

  va_start(args);
  i = va_arg(args, int);
  fprintf(stderr,"i = %d\n", i);

  foo = va_arg(args, char *);
  if (!foo) fprintf(stderr,"foo = nil\n");
  else fprintf(stderr,"foo = '%s'\n",foo);

  va_end(args);
}

