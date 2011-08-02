#ifndef _DTRAIN_TESTS_H_
#define _DTRAIN_TESTS_H_

#include <iomanip>
#include <boost/assign/std/vector.hpp>

#include "common.h"
#include "util.h"


namespace dtrain
{


double approx_equal( double x, double y );
void test_ngrams();
void test_metrics();
void test_SetWeights();
void run_tests();


} // namespace


#endif

