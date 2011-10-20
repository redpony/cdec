#ifndef _FF_FACTORY_H_
#define _FF_FACTORY_H_

// FsaF* vs F* (regular ff/factory).

//TODO: use http://www.boost.org/doc/libs/1_43_0/libs/functional/factory/doc/html/index.html ?

/*TODO: register state identity separately from feature function identity?  as
 * in: string registry for name of state somewhere, assert that same result is
 * computed by all users?  or, we can just require that ff sharing same state
 * all be mashed into a single ffunc, which can just emit all the fid scores at
 * once.  that's fine.
 */


#include <iostream>
#include <string>
#include <map>
#include <stdexcept>

#include <boost/shared_ptr.hpp>

class FeatureFunction;

class FsaFeatureFunction;


struct UntypedFactory {
  virtual ~UntypedFactory();
  virtual std::string usage(bool params,bool verbose) const = 0;
};

template <class FF>
struct FactoryBase : public UntypedFactory {
  typedef FF F;
  typedef boost::shared_ptr<F> FP;

  virtual FP Create(std::string param) const = 0;
};

/* see cdec_ff.cc for example usage: this create concrete factories to be registered */
template<class FF>
struct FFFactory : public FactoryBase<FeatureFunction> {
  FP Create(std::string param) const {
    FF *ret=new FF(param);
    ret->Init();
    return FP(ret);
  }
  virtual std::string usage(bool params,bool verbose) const {
    return FF::usage(params,verbose);
  }
};


// same as above, but we didn't want to require a typedef e.g. Parent in FF class, and template typedef isn't available
template<class FF>
struct FsaFactory : public FactoryBase<FsaFeatureFunction> {
  FP Create(std::string param) const {
    FF *ret=new FF(param);
    ret->Init();
    return FP(ret);
  }
  virtual std::string usage(bool params,bool verbose) const {
    return FF::usage(params,verbose);
  }
};

struct UntypedFactoryRegistry {
  std::string usage(std::string const& ffname,bool params=true,bool verbose=true) const;
  bool have(std::string const& ffname);
  void DisplayList() const;
  void Register(const std::string& ffname, UntypedFactory* factory);
  void Register(UntypedFactory* factory);
  void clear();
  static bool parse_debug(std::string & param_in_out); // returns true iff param starts w/ debug (and remove that prefix from param)
 protected:
  typedef boost::shared_ptr<UntypedFactory> FactoryP;
  typedef std::map<std::string, FactoryP > Factmap;
  Factmap reg_;
  friend int main(int argc, char** argv);
  friend class UntypedFactory;
};



template <class Feat>
struct FactoryRegistry : public UntypedFactoryRegistry {
  typedef Feat F;
  typedef boost::shared_ptr<F> FP;
  typedef FactoryBase<F> FB;

  FP Create(const std::string& ffname, std::string param) const {
    using namespace std;
    Factmap::const_iterator it = reg_.find(ffname);
    if (it == reg_.end())
      throw std::runtime_error("I don't know how to create feature "+ffname);
    bool debug=parse_debug(param);
    if (debug)
      cerr<<"debug enabled for "<<ffname<< " - remaining options: '"<<param<<"'\n";
    FP res = dynamic_cast<FB const&>(*it->second).Create(param);
    res->init_name_debug(ffname,debug);
    // could add a res->Init() here instead of in Create if we wanted feature id to potentially differ based on the registered name rather than static usage() - of course, specific feature ids can be computed on the basis of feature param as well; this only affects the default single feature id=name
    return res;
  }
};

typedef FactoryRegistry<FeatureFunction> FFRegistry;
typedef FactoryRegistry<FsaFeatureFunction> FsaFFRegistry;

extern FsaFFRegistry fsa_ff_registry;
inline FsaFFRegistry & global_fsa_ff_registry() { return fsa_ff_registry; }
extern FFRegistry ff_registry;
inline FFRegistry & global_ff_registry() { return ff_registry; }

void ff_usage(std::string const& name,std::ostream &out=std::cout);

/*
extern boost::shared_ptr<FsaFFRegistry> global_fsa_ff_registry;
extern boost::shared_ptr<FFRegistry> global_ff_registry;
*/
#endif
