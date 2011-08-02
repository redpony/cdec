#ifndef _DTRAIN_COMMON_H_
#define _DTRAIN_COMMON_H_


#include <sstream>
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

#include "sentence_metadata.h"
#include "verbose.h"
#include "viterbi.h"
#include "kbest.h"
#include "ff_register.h"
#include "decoder.h"
#include "weights.h"

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include "score.h"

#define DTRAIN_DEFAULT_K 100
#define DTRAIN_DEFAULT_N 4
#define DTRAIN_DEFAULT_T 1

#define DTRAIN_DOTOUT 100


using namespace std;
using namespace dtrain;
namespace po = boost::program_options;


#endif

