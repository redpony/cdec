#define STRINGLIB_DEBUG
#include "stringlib.h"

using namespace std;
struct print {
  template <class S>
  void operator()(S const& s) const {
    cout<<s<<endl;
  }
};

char p[]=" 1 are u 2 serious?";
int main(int argc, char *argv[]) {
  std::string const& w="verylongword";
  VisitTokens(p,print());
  VisitTokens(w,print());
}
