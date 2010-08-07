#include "ff_factory.h"

#include "ff.h"
#include "stringlib.h"
#include <stdexcept>

using boost::shared_ptr;
using namespace std;

UntypedFactory::~UntypedFactory() {  }

namespace {
std::string const& debug_pre="debug";
}

void UntypedFactoryRegistry::clear() {
  reg_.clear();
}

bool UntypedFactoryRegistry::parse_debug(std::string & p) {
  int pl=debug_pre.size();
  bool space=false;
  bool debug=match_begin(p,debug_pre)&&
    (p.size()==pl || (space=(p[pl]==' ')));
  if (debug)
    p.erase(0,debug_pre.size()+space);
  return debug;
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


void UntypedFactoryRegistry::Register(UntypedFactory* factory)
{
  Register(factory->usage(false,false),factory);
}


/*FIXME: I want these to go in ff_factory.cc, but extern etc. isn't workign right:
  ../decoder/libcdec.a(ff_factory.o): In function `~UntypedFactory':
/nfs/topaz/graehl/ws10smt/decoder/ff_factory.cc:9: multiple definition of `global_ff_registry'
mr_vest_generate_mapper_input.o:/nfs/topaz/graehl/ws10smt/vest/mr_vest_generate_mapper_input.cc:307: first defined here
*/
FsaFFRegistry fsa_ff_registry;
FFRegistry ff_registry;

/*
#include "null_deleter.h"
boost::shared_ptr<FsaFFRegistry> global_fsa_ff_registry(&fsa_ff_registry,null_deleter());
boost::shared_ptr<FFRegistry> global_ff_registry(&ff_registry,null_deleter());
*/

void ff_usage(std::string const& n,std::ostream &out)
{
  bool have=ff_registry.have(n);
  if (have)
    cout<<"FF "<<ff_registry.usage(n,true,true)<<endl;
  if (fsa_ff_registry.have(n))
    cout<<"Fsa FF "<<fsa_ff_registry.usage(n,true,true)<<endl;
  else if (!have)
    throw std::runtime_error("Unknown feature "+n);
}

