
#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <stdlib.h>

#define USE_LOG_FILE

#ifdef USE_LOG_FILE
  extern FILE *logfp;
  #define LOG_STREAM logfp
#else
  #define LOG_STREAM stdout
#endif

#define PRINT(...) fprintf(LOG_STREAM, __VA_ARGS__)

#endif // _LOG_H_
