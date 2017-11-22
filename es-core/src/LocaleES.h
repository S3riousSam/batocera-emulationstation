#pragma once
#include <boost/locale.hpp>

#define _(A) boost::locale::gettext(A)
#define ngettext(A, B, C) boost::locale::ngettext(A, B, C)

