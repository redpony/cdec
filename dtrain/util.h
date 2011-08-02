#ifndef _DTRAIN_UTIL_H_
#define _DTRAIN_UTIL_H_


#include <iostream>
#include <string>
#include <vector>

#include "fdict.h"
#include "tdict.h"
#include "wordid.h"

using namespace std;


namespace dtrain
{


void register_and_convert(const vector<string>& strs, vector<WordID>& ids);
void print_FD();


} // namespace


#endif

