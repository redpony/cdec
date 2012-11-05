#ifndef GRAMMAR_H_
#define GRAMMAR_H_

#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <set>
#include <string>

#include <boost/shared_ptr.hpp>

#include "lattice.h"
#include "trule.h"

struct RuleBin {
  virtual ~RuleBin();
  virtual int GetNumRules() const = 0;
  virtual TRulePtr GetIthRule(int i) const = 0;
  virtual int Arity() const = 0;
};

struct GrammarIter {
  virtual ~GrammarIter();
  virtual const RuleBin* GetRules() const = 0;
  virtual const GrammarIter* Extend(int symbol) const = 0;
};

struct Grammar {
  //TODO: HASH_MAP?
  typedef std::map<WordID, std::vector<TRulePtr> > Cat2Rules;
  static const std::vector<TRulePtr> NO_RULES;

  Grammar(): ctf_levels_(0) {}
  virtual ~Grammar();
  virtual const GrammarIter* GetRoot() const = 0;
  virtual bool HasRuleForSpan(int i, int j, int distance) const;
  const std::string GetGrammarName(){return grammar_name_;}
  unsigned int GetCTFLevels(){ return ctf_levels_; }
  void SetGrammarName(std::string n) {grammar_name_ = n; }
  // cat is the category to be rewritten
  inline const std::vector<TRulePtr>& GetAllUnaryRules() const {
    return unaries_;
  }

  // get all the unary rules that rewrite category cat
  inline const std::vector<TRulePtr>& GetUnaryRulesForRHS(const WordID& cat) const {
    Cat2Rules::const_iterator found = rhs2unaries_.find(cat);
    if (found == rhs2unaries_.end())
      return NO_RULES;
    else
      return found->second;
  }

 protected:
  Cat2Rules rhs2unaries_;     // these must be filled in by subclasses!
  std::vector<TRulePtr> unaries_;
  std::string grammar_name_;
  unsigned int ctf_levels_;
};

typedef boost::shared_ptr<Grammar> GrammarPtr;

class TGImpl;
struct TextGrammar : public Grammar {
  TextGrammar();
  explicit TextGrammar(const std::string& file);
  explicit TextGrammar(std::istream* in);
  void SetMaxSpan(int m) { max_span_ = m; }

  virtual const GrammarIter* GetRoot() const;
  void AddRule(const TRulePtr& rule, const unsigned int ctf_level=0, const TRulePtr& coarse_parent=TRulePtr());
  void ReadFromFile(const std::string& filename);
  void ReadFromStream(std::istream* in);
  virtual bool HasRuleForSpan(int i, int j, int distance) const;
  const std::vector<TRulePtr>& GetUnaryRules(const WordID& cat) const;

 private:
  int max_span_;
  boost::shared_ptr<TGImpl> pimpl_;

};

#endif
