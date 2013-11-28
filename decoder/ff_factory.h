#ifndef _FF_FACTORY_H_
#define _FF_FACTORY_H_

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


class UntypedFactory {
 public:
  virtual ~UntypedFactory();
  virtual std::string usage(bool params,bool verbose) const = 0;
};

template <class FF>
class FactoryBase : public UntypedFactory {
 public:
  typedef FF F;
  typedef boost::shared_ptr<F> FP;

  virtual FP Create(std::string param) const = 0;
};

/* see cdec_ff.cc for example usage: this create concrete factories to be registered */
template<class FF>
class FFFactory : public FactoryBase<FeatureFunction> {
 public:
  FP Create(std::string param) const {
    FF *ret=new FF(param);
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
    FP res = dynamic_cast<FB const&>(*it->second).Create(param);
    return res;
  }
};

typedef FactoryRegistry<FeatureFunction> FFRegistry;

extern FFRegistry ff_registry;
inline FFRegistry& global_ff_registry() { return ff_registry; }

void ff_usage(std::string const& name,std::ostream& out=std::cerr);

#endif
