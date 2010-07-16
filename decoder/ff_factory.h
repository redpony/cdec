#ifndef _FF_FACTORY_H_
#define _FF_FACTORY_H_

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
