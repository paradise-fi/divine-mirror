// -*- C++ -*-
#include <wibble/exception.h>
#ifndef _APTFRONT_CAST
#define _APTFRONT_CAST

namespace wibble {

template <typename T, typename X> T &downcast(X *v) {
    if (!v)
        throw exception::BadCastExt< X, T >( "downcast on null pointer" );
    T *x = dynamic_cast<T *>(v);
    if (!x)
        throw exception::BadCastExt< X, T >( "dynamic downcast failed" );
    return *x;
}

}

#endif
