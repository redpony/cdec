#include "dict.h"

#include <string>
#include <vector>
#include <boost/regex.hpp>
#include <boost/algorithm/string/regex.hpp>

void Dict::AsVector(const WordID& id, std::vector<std::string>* results) const {
  boost::algorithm::split_regex(*results, Convert(id), boost::regex(" \\|\\|\\| "));
}

