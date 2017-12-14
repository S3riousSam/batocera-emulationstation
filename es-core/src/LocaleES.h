#pragma once
#include <boost/locale.hpp>

#if defined(EXTENSION)
#define _(A) boost::locale::gettext(A)
#else
#define _(A) std::string(A)
#endif
