#pragma once
#include <boost/locale.hpp>

#define _(A) boost::locale::gettext(A)
