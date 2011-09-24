#ifndef __WORDCOUNT_HH__
#define __WORDCOUNT_HH__


#include <iostream>
#include <string>

#include "hadoop/Pipes.hh"
#include "hadoop/TemplateFactory.hh"

#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;


class WordcountMapper : public HadoopPipes::Mapper
{
  public:
    WordcountMapper( const HadoopPipes::TaskContext & ) {};
    void map( HadoopPipes::MapContext &context );
};

class WordcountReducer : public HadoopPipes::Reducer
{
  public:
    WordcountReducer( const HadoopPipes::TaskContext & ) {};
    void reduce( HadoopPipes::ReduceContext & context );
};


#endif

