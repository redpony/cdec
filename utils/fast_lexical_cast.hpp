#ifndef FAST_LEXICAL_CAST_HPP
#define FAST_LEXICAL_CAST_HPP

#define BOOST_LEXICAL_CAST_ASSUME_C_LOCALE
// This should make casting to/from string reasonably fast (see http://accu.org/index.php/journals/1375)

#include <boost/lexical_cast.hpp>

using boost::lexical_cast;

#endif
