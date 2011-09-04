#include <string>
#include <iostream>
#include <vector>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost;


int main()
{  
  string str="0:0     [X] ||| [X,1] musharrafs ||| [X,1] for musharraf ||| 1.78532981873 1.79239165783 0.301030009985 0.625125288963 1.95314443111 0.0 1.0 a=1 b=1 c=1 ||| 1-2";   
  
  size_t i = 0, c = 0, beg = 0, end = 0;
  string::iterator it = str.begin();
  string s;
  while ( c != 12 ) {
    s = *it;
    if ( s == "|" ) c += 1;
    if ( beg == 0 && c == 9 ) beg = i+2;
    if ( c == 12 ) end = i-beg-3;
    it++;
    i++;
  }

  string sub = str.substr( beg, end );
  vector<string> feats;
  boost::split( feats, sub, boost::is_any_of(" ") );
  vector<string>::iterator f;
  for ( f = feats.begin(); f != feats.end(); f++ ) {
    if ( f->find("=1") != string::npos ) cout << *f << endl;
  }

}

