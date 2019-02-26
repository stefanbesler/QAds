#ifndef QADS_GLOBAL_H
#define QADS_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(QADS_LIBRARY)
#  define QADSSHARED_EXPORT Q_DECL_EXPORT
#else
#  define QADSSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // QADS_GLOBAL_H
