#include <stdio.h>
#include <stdarg.h>

/* Activates debug printouts */
unsigned int verbose;

/* Enable debug prints by setting verbose variable */
void debug_printf(const char *fmt, ...)
{
  va_list arglist;
  if (verbose) {
    va_start(arglist, fmt);
    vprintf(fmt, arglist);
    va_end(arglist);
  }
}
