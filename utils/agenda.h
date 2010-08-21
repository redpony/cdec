#ifndef AGENDA_H
#define AGENDA_H

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
#include "hash.h"
#include "d_ary_heap.h"
#include "lvalue_pmap.h"
#include <vector>
#include <functional>

/*
template <class P>
struct priority_traits {
  typedef typename P::priority_type priority_type;
};

// P p has priority_traits<P>::type &p->agenda_priority() and unsigned &p->agenda_location(), and bool & p->agenda_done()
// this is really 4 functors in 1 (or lvalue property maps); you can supply your own as the Prio type in Agenda<...> below ; state is allowed.
template <class P>
struct AdjustablePriority {
  typedef AdjustablePriority<P> Self;
  typedef typename priority_traits<P>::priority_type Priority;
  Priority & priority(P const &x) {
    return x->agenda_priority();
  }
  unsigned & location(P const &x) { // this gets updated by push, pop, and adjust
    return x->agenda_location();
  }
  void is_done(P const& x) const {
    return x->agenda_done();
  }
  void set_done(P const& x) const {
    x->agenda_done()=true;
  }
};
*/

typedef best_t agenda_best_t;

PMAP_MEMBER_INDIRECT(LocationMap,unsigned,location)
PMAP_MEMBER_INDIRECT(PriorityMap,best_t,priority)

// LocMap and PrioMap are boost property maps put(locmap,key,size_t), Better(get(priomap,k1),get(priomap,k2)) means k1 should be above k2 (be popped first).  Locmap and PrioMap may have state; the rest are assumed stateless functors
template <class Item,class Hash=boost::hash<Item>,class Equal=std::equal_to<Item>,class Better=std::less<Item> >
struct Agenda {
  /* this is less generic than it could be, because I want to use a single hash mapping to intern to canonical mutable object pointers, where the property maps are just lvalue accessors */
/*
  typedef Item *CanonItem;
  static const std::size_t heap_arity=4; // might be fastest possible (depends on key size probably - cache locality is bad w/ arity=2)
  typedef std::vector<CanonItem> HeapStorage;
  typedef boost::detail::d_ary_heap_indirect<Key,heap_arity,LocMap,PrioMap,Better,HeapStorage> Heap;
  HASH_SET<Key,Hash,Equal> queued;
  typedef LocationMap<ItemP>
  LocMap locmap;
  PrioMap priomap;
  Agenda(LocMap const& lm=LocMap(),PrioMap const& pm=PrioMap()) : locmap(lm), priomap(pm) {  }
*/
};

#endif
