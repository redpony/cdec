#include "wordcount.hh"


void
WordcountMapper::map(HadoopPipes::MapContext & context)
{
  typedef boost::tokenizer<> tokenizer_t;
  tokenizer_t tokenizer(context.getInputValue());

  for( tokenizer_t::const_iterator i = tokenizer.begin();
      tokenizer.end() != i; ++i ) {
    context.emit(boost::to_lower_copy(*i), "1");
  }
}

void
WordcountReducer::reduce(HadoopPipes::ReduceContext & context)
{
  uint32_t count( 0 );

  do {
	++count;
  } while( context.nextValue() );

  std::cout << context.getInputKey() << endl;
  context.emit( context.getInputKey(),
                boost::lexical_cast<std::string>(count) );
}


int
main( int argc, char * argv[] )
{
  HadoopPipes::TemplateFactory2<WordcountMapper,
                                WordcountReducer> factory;
  return HadoopPipes::runTask( factory );
}

