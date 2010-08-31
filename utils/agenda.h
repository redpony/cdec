#ifndef AGENDA_H
#define AGENDA_H

#define DBG_AGENDA(x) x
/*
  a priority queue where you expect to queue the same item at different
  priorities several times before finally popping it.  higher priority = better.
  so in best first you'd be using negative cost or e^-cost (probabilities, in
  other words).

  this means you have a way to look up a key and see its location in the queue,
  so its priority can be adjusted (or, simpler implementation: so when you pop,
  you see if you've already popped before at a lower cost, and skip the
  subsequent pops).

  it's assumed that you'll never queue an item @ a better priority after it has
  already been popped.  that is, the agenda will track already completed items.
  maybe in the future i will let you recompute a cheaper way to reach things
  after first-pop also, it's assumed that we're always improving prios of
  existing items, never making them worse (even though technically this is
  possible and sensible if it hasn't been popped yet).

  simple binary max heap for now.  there are better practical options w/
  superior cache locaility.  movements in the heap need to update a record for
  that key of where the key went.  i do this by creating canonical key pointers
  out of boost object pools (if the key were lightweight e.g. an int, then it
  would make sense to use the hash lookup too

  since i'm doing key hashing to start with, i also allow you to attach some
  arbitrary data (value) payload beyond key+priority.

  hash map from key to done (has been popped) -> set where doneness is marked in key item?

  a slightly different way to make an adjustable heap would be to use
  tree-structured parent/children links intrusively (or mapped by key) in the
  key, rather than indices in a compact binary-tree heap

 */

#include "best.h"
#include "intern_pool.h"
#include "d_ary_heap.h"
#include "lvalue_pmap.h"
#include <vector>
#include <functional>

/*
template <class P>
struct priority_traits {
  typedef typename P::priority_type priority_type;
};
*/

typedef best_t agenda_best_t;
typedef unsigned agenda_location_t;

PMAP_MEMBER_INDIRECT(LocationMap,agenda_location_t,location)
PMAP_MEMBER_INDIRECT(PriorityMap,agenda_best_t,priority)

struct Less {
  typedef bool result_type;
  template <class A,class B>
  bool operator()(A const& a,B const& b) const { return a<b; }
};

// LocMap and PrioMap are boost property maps put(locmap,key,size_t), Better(get(priomap,k1),get(priomap,k2)) means k1 should be above k2 (be popped first).  Locmap and PrioMap may have state; the rest are assumed stateless functors
// make sure the (default) location is not -1 for anything you add, or else an assertion may trigger
template <class Item,class Better=Less, /* intern_pool args */ class KeyF=get_key<Item>,class HashKey=boost::hash<typename KeyF::result_type>,class EqKey=std::equal_to<typename KeyF::result_type>, class Pool=boost::object_pool<Item> >
struct Agenda : intern_pool<Item,KeyF,HashKey,EqKey,Pool> {
  typedef intern_pool<Item,KeyF,HashKey,EqKey,Pool> Intern; // inherited because I want to use construct()
  /* this is less generic than it could be, because I want to use a single hash mapping to intern to canonical mutable object pointers, where the property maps are just lvalue accessors */
  typedef typename KeyF::result_type Key;
  typedef Item * Handle;
  typedef LocationMap<Handle> LocMap;
  typedef PriorityMap<Handle> PrioMap;
  LocMap locmap;
  PrioMap priomap; // note: priomap[item] is set by caller before giving us the item; then tracks best (for canonicalized item) thereafter

  Better better;
  //NOT NEEDED: initialize function object state (there is none)

  typedef Item *ItemC; //canonicalized pointer
  typedef Item *ItemP;
  static const std::size_t heap_arity=4; // might be fastest possible (depends on key size probably - cache locality is bad w/ arity=2)
  typedef std::vector<ItemC> HeapStorage;
  typedef d_ary_heap_indirect<Handle,heap_arity,LocMap,PrioMap,Better,HeapStorage,agenda_location_t> Heap;
  Heap q;

  // please don't call q.push etc. directly.
  void add(ItemP i) {
    bool fresh=interneq(i);
    DBG_AGENDA(assert(fresh && !q.contains(i)));
    q.push(i);
  }
  bool improve(ItemP i) {
    ItemP c=i;
    bool fresh=interneq(c);
    if (fresh)
      return add(c);
    DBG_AGENDA(assert(q.contains(c)));
    return q.maybe_improve(priomap[i]);
  }
  inline bool empty() {
    return q.empty();
  }
  // no need to destroy the canon. item because we want to remember the best cost and reject more expensive ways of using it).
  ItemC pop() {
    DBG_AGENDA(assert(!empty()));
    ItemC r=q.top();
    q.pop();
    return r;
  }
  agenda_best_t best() const {
    return priomap[q.top()]; //TODO: cache/track the global best?
  }

  Agenda(unsigned reserve=1000000,LocMap const& lm=LocMap(),PrioMap const& pm=PrioMap(),EqKey const& eq=EqKey(),Better const& better=Better()) : locmap(lm), priomap(pm), better(better), q(priomap,locmap,better,reserve) {  }
};

#endif
