#ifndef _ARC_FF_FACTORY_H_
#define _ARC_FF_FACTORY_H_

#include <string>
#include <map>
#include <boost/shared_ptr.hpp>

struct ArcFFFactoryBase {
  virtual boost::shared_ptr<ArcFeatureFunction> Create(const std::string& param) const = 0;
};

template<class FF>
struct ArcFFFactory : public ArcFFFactoryBase {
  boost::shared_ptr<ArcFeatureFunction> Create(const std::string& param) const {
    return boost::shared_ptr<ArcFeatureFunction>(new FF(param));
  }
};

struct ArcFFRegistry {
  boost::shared_ptr<ArcFeatureFunction> Create(const std::string& name, const std::string& param) const {
    std::map<std::string, ArcFFFactoryBase*>::const_iterator it = facts.find(name);
    assert(it != facts.end());
    return it->second->Create(param);
  }

  void Register(const std::string& name, ArcFFFactoryBase* fact) {
    ArcFFFactoryBase*& f = facts[name];
    assert(f == NULL);
    f = fact;
  }
  std::map<std::string, ArcFFFactoryBase*> facts;
};

std::ostream& operator<<(std::ostream& os, const ArcFFRegistry& reg) {
  for (std::map<std::string, ArcFFFactoryBase*>::const_iterator it = reg.facts.begin();
       it != reg.facts.end(); ++it) {
    os << "  " << it->first << std::endl;
  }
  return os;
}

#endif
