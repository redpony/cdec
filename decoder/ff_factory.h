#ifndef _FF_FACTORY_H_
#define _FF_FACTORY_H_

/*TODO: register state identity separately from feature function identity?  as
 * in: string registry for name of state somewhere, assert that same result is
 * computed by all users?  or, we can just require that ff sharing same state
 * all be mashed into a single ffunc, which can just emit all the fid scores at
 * once.  that's fine.
 */

//TODO: use http://www.boost.org/doc/libs/1_43_0/libs/functional/factory/doc/html/index.html ?

#include <iostream>
#include <string>
#include <map>

#include <boost/shared_ptr.hpp>

class FeatureFunction;
class FFRegistry;
class FFFactoryBase;
extern boost::shared_ptr<FFRegistry> global_ff_registry;

class FFRegistry {
  friend int main(int argc, char** argv);
  friend class FFFactoryBase;
 public:
  boost::shared_ptr<FeatureFunction> Create(const std::string& ffname, const std::string& param) const;
  std::string usage(std::string const& ffname,bool params=true,bool verbose=true) const;
  void DisplayList() const;
  void Register(const std::string& ffname, FFFactoryBase* factory);
  void Register(FFFactoryBase* factory);
  FFRegistry() {}
 private:
  std::map<std::string, boost::shared_ptr<FFFactoryBase> > reg_;
};

struct FFFactoryBase {
  virtual ~FFFactoryBase();
  virtual boost::shared_ptr<FeatureFunction> Create(const std::string& param) const = 0;
  virtual std::string usage(bool params,bool verbose) const = 0;
};

template<class FF>
class FFFactory : public FFFactoryBase {
  boost::shared_ptr<FeatureFunction> Create(const std::string& param) const {
    return boost::shared_ptr<FeatureFunction>(new FF(param));
  }
  // called with false,false just gives feature name
  virtual std::string usage(bool params,bool verbose) const {
    return FF::usage(params,verbose);
  }

};

#endif
