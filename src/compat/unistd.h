#pragma once

#ifdef _WIN32

#include <direct.h>
#include <io.h>

#ifndef F_OK
#define F_OK 0
#endif

#ifndef R_OK
#define R_OK 4
#endif

#ifndef W_OK
#define W_OK 2
#endif

#ifndef X_OK
#define X_OK 0
#endif

#ifndef access
#define access _access
#endif

#endif
