#pragma once

#include <qglobal.h>

#if defined(QLITEHTML_LIBRARY)
#define QLITEHTML_EXPORT Q_DECL_EXPORT
#elif defined(QLITEHTML_STATIC_LIBRARY) // Abuse single files for manual tests
#define QLITEHTML_EXPORT
#else
#define QLITEHTML_EXPORT Q_DECL_IMPORT
#endif
