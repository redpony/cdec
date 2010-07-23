#include "ff_factory.h"

#include "ff.h"
#include "stringlib.h"

using boost::shared_ptr;
using namespace std;

FFFactoryBase::~FFFactoryBase() {}

void FFRegistry::DisplayList() const {
  for (map<string, shared_ptr<FFFactoryBase> >::const_iterator it = reg_.begin();
       it != reg_.end(); ++it) {
    cerr << "  " << it->first << endl;
  }
}

string FFRegistry::usage(string const& ffname,bool params,bool verbose) const {
  map<string, shared_ptr<FFFactoryBase> >::const_iterator it = reg_.find(ffname);
  return it == reg_.end()
    ? "Unknown feature " + ffname
    : it->second->usage(params,verbose);
}

namespace {
std::string const& debug_pre="debug";
}

shared_ptr<FeatureFunction> FFRegistry::Create(const string& ffname, const string& param) const {
  map<string, shared_ptr<FFFactoryBase> >::const_iterator it = reg_.find(ffname);
  shared_ptr<FeatureFunction> res;
  if (it == reg_.end()) {
    cerr << "I don't know how to create feature " << ffname << endl;
  } else {
    int pl=debug_pre.size();
    bool space=false;
    std::string p=param;
    bool debug=match_begin(p,debug_pre)&&
      (p.size()==pl || (space=(p[pl]==' ')));
    if (debug) {
      p.erase(0,debug_pre.size()+space);
      cerr<<"debug enabled for "<<ffname<< " - rest of param='"<<p<<"'\n";
    }
    res = it->second->Create(p);
    res->name=ffname;
    res->debug=debug;
  }
  return res;
}

void FFRegistry::Register(const string& ffname, FFFactoryBase* factory) {
  if (reg_.find(ffname) != reg_.end()) {
    cerr << "Duplicate registration of FeatureFunction with name " << ffname << "!\n";
    abort();
  }
  reg_[ffname].reset(factory);
}


void FFRegistry::Register(FFFactoryBase* factory)
{
  Register(factory->usage(false,false),factory);
}
