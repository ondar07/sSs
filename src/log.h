
#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <stdlib.h>

#define LOG_STREAM stdout

#define PRINT(...) fprintf(LOG_STREAM, __VA_ARGS__)

#endif // _LOG_H_
