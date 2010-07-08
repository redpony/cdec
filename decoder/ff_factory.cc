#include "ff_factory.h"

#include "ff.h"

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

shared_ptr<FeatureFunction> FFRegistry::Create(const string& ffname, const string& param) const {
  map<string, shared_ptr<FFFactoryBase> >::const_iterator it = reg_.find(ffname);
  shared_ptr<FeatureFunction> res;
  if (it == reg_.end()) {
    cerr << "I don't know how to create feature " << ffname << endl;
  } else {
    res = it->second->Create(param);
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
