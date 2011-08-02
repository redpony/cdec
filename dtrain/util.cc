#include "util.h"


namespace dtrain
{


/*
 * register_and_convert
 *
 */
void
register_and_convert(const vector<string>& strs, vector<WordID>& ids)
{
  vector<string>::const_iterator it;
  for ( it = strs.begin(); it < strs.end(); it++ ) {
    ids.push_back( TD::Convert( *it ) );
  }
}


/*
 * print_FD
 *
 */
void
print_FD()
{
  for ( size_t i = 0; i < FD::NumFeats(); i++ ) cout << FD::Convert(i)<< endl;
}


} // namespace

