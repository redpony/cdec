#include "ff_factory.h"

#include "ff.h"
#include "stringlib.h"
#include <stdexcept>

using boost::shared_ptr;
using namespace std;

// global ff registry
FFRegistry ff_registry;

UntypedFactory::~UntypedFactory() {  }

void UntypedFactoryRegistry::clear() {
  reg_.clear();
}

bool UntypedFactoryRegistry::have(std::string const& ffname) {
  return reg_.find(ffname)!=reg_.end();
}

void UntypedFactoryRegistry::DisplayList() const {
  for (Factmap::const_iterator it = reg_.begin();
       it != reg_.end(); ++it) {
    cerr << "  " << it->first << endl;
  }
}

string UntypedFactoryRegistry::usage(string const& ffname,bool params,bool verbose) const {
  Factmap::const_iterator it = reg_.find(ffname);
  return it == reg_.end()
    ? "Unknown feature " + ffname
    : it->second->usage(params,verbose);
}

void UntypedFactoryRegistry::Register(const string& ffname, UntypedFactory* factory) {
  if (reg_.find(ffname) != reg_.end()) {
    cerr << "Duplicate registration of FeatureFunction with name " << ffname << "!\n";
    abort();
  }
  reg_[ffname].reset(factory);
}


void UntypedFactoryRegistry::Register(UntypedFactory* factory) {
  Register(factory->usage(false,false),factory);
}


void ff_usage(std::string const& n,std::ostream &out) {
  bool have=ff_registry.have(n);
  if (have)
    out << "FF " << ff_registry.usage(n,true,true) << endl;
  else {
    cerr << "Unknown feature: " << n << endl;
    abort();
  }
}

