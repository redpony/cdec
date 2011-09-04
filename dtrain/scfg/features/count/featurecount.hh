#ifndef __FEATURECOUNT_HH__
#define __FEATURECOUNT_HH__

#include "hadoop/Pipes.hh"
#include "hadoop/TemplateFactory.hh"
#include "hadoop/StringUtils.hh"
#include <iostream>
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;


class FeatureCountMapper : public HadoopPipes::Mapper
{
  public:
    FeatureCountMapper( const HadoopPipes::TaskContext& ) {};
    void map( HadoopPipes::MapContext &context );
};

class FeatureCountReducer : public HadoopPipes::Reducer
{
  public:
    FeatureCountReducer( const HadoopPipes::TaskContext& ) {};
    void reduce( HadoopPipes::ReduceContext & context );
};


#endif

