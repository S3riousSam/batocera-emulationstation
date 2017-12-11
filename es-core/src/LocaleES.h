#pragma once
#include <boost/locale.hpp>

#if defined(EXTENSION)
#define _(A) boost::locale::gettext(A)
#endif
