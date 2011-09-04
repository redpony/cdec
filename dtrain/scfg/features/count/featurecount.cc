#include "featurecount.hh"


void
FeatureCountMapper::map( HadoopPipes::MapContext &context )
{
  string line = context.getInputValue();

  // get features substr
  size_t i = 0, c = 0, beg = 0, end = 0;
  string::iterator it = line.begin();
  string s;
  while ( c != 12 ) {
    s = *it;
    if ( s == "|" ) c += 1;
    if ( beg == 0 && c == 9 ) beg = i+2;
    if ( c == 12 ) end = i-beg-3;
    it++;
    i++;
  }
  string sub = line.substr( beg, end );

  // emit feature:1
  vector<string> f_tok;
  boost::split( f_tok, sub, boost::is_any_of(" ") );
  vector<string>::iterator f;
  for ( f = f_tok.begin(); f != f_tok.end(); f++ ) {
    if ( f->find("=1") != string::npos ) context.emit(*f, "1");
  }
}

void
FeatureCountReducer::reduce( HadoopPipes::ReduceContext &context )
{
  size_t sum = 0;
  while ( context.nextValue() ) sum += HadoopUtils::toInt( context.getInputValue() );
  context.emit( context.getInputKey(), HadoopUtils::toString(sum) );
}


int
main( int argc, char * argv[] )
{
  HadoopPipes::TemplateFactory2<FeatureCountMapper,
                                FeatureCountReducer> factory;

  return HadoopPipes::runTask(factory);
}

