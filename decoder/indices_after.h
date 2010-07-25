#ifndef INDICES_AFTER_REMOVING_H
#define INDICES_AFTER_REMOVING_H

#include <boost/config.hpp> // STATIC_CONSTANT
#include <algorithm> //swap

// iterator wrapper.  inverts boolean value.
template <class AB>
struct keep_not_marked
{
    typedef keep_not_marked<AB> self_type;
    AB i;
    bool operator *() const
    {
        return !*i;
    }
    void operator++()
    {
        ++i;
    }
    bool operator ==(self_type const& o)
    {
        return i==o.i;
    }
};

// outputs sequence to out iterator, of new indices for each element i, corresponding to deleting element i from an array when remove[i] is true (-1 if it was deleted, new index otherwise), returning one-past-end of out (the return value = # of elements left after deletion)
template <class KEEP,class KEEPe,class O>
unsigned new_indices_keep(KEEP i, KEEPe end,O out) {
    unsigned f=0;
    while (i!=end)
      *out++ = *i++ ? f++ : (unsigned)-1;
    return f;
};

template <class It,class KeepIf,class O>
unsigned new_indices_keep_if_n(unsigned n,It i,KeepIf const& r,O out)
{
    unsigned f=0;
    while(n--)
      *out++ = r(*i++) ? f++ : (unsigned)-1;
    return f;
}

template <class KEEP,class O>
unsigned new_indices(KEEP keep,O out) {
  return new_indices(keep.begin(),keep.end(),out);
}

// given a vector and a parallel sequence of bools where true means keep, keep only the marked elements while maintaining order
struct indices_after
{
  BOOST_STATIC_CONSTANT(unsigned,REMOVED=(unsigned)-1);
  unsigned *map; // map[i] == REMOVED if i is deleted
  unsigned n_kept;
  unsigned n_mapped;
  template <class AB,class ABe>
  indices_after(AB i, ABe end) {
    init(i,end);
  }

  template <class Vec,class R>
  void init_keep_if(Vec v,R const& r)
  {
    n_mapped=v.size();
    if ( !n_mapped ) return;
    map=(unsigned *)::operator new(sizeof(unsigned)*n_mapped);
    n_kept=new_indices_keep_if_n(n_mapped,r,map);
  }

  template <class AB,class ABe>
  void init(AB i, ABe end) {
    n_mapped=end-i;
    if (n_mapped>0) {
      map=(unsigned *)::operator new(sizeof(unsigned)*n_mapped);
      n_kept=new_indices_keep(i,end,map);
    } else
      map=NULL;
  }
  template <class A>
  void init(A const& a)
  {
    init(a.begin(),a.end());
  }

  template <class A>
  indices_after(A const& a)
  {
    init(a.begin(),a.end());
  }
  indices_after() : n_mapped(0) {}
  ~indices_after()
  {
    if (n_mapped)
      ::operator delete((void*)map);
  }
  bool removing(unsigned i) const
  {
    return map[i] == REMOVED;
  }
  bool keeping(unsigned i) const
  {
    return map[i] != REMOVED;
  }

  unsigned operator[](unsigned i) const
  {
    return map[i];
  }

  template <class Vec>
  void do_moves(Vec &v) const
  {
    assert(v.size()==n_mapped);
    unsigned i=0;
    for (;i<n_mapped&&keeping(i);++i) ;
    for(;i<n_mapped;++i)
      if (keeping(i))
        v[map[i]]=v[i];
    v.resize(n_kept);
  }

  template <class Vec>
  void do_moves_swap(Vec &v) const
  {
    using std::swap;
    assert(v.size()==n_mapped);
    unsigned r=n_mapped;
    unsigned i=0;
    for (;i<n_mapped&&keeping(i);++i) ;
    for(;i<n_mapped;++i)
      if (keeping(i))
        std::swap(v[map[i]],v[i]);
    v.resize(n_kept);
  }

  template <class Vecto,class Vecfrom>
  void copy_to(Vecto &to,Vecfrom const& v) const {
    to.resize(n_kept);
    for (unsigned i=0;i<n_mapped;++i)
      if (keeping(i))
        to[map[i]]=v[i];
  }

private:
  indices_after(indices_after const& o)
  {
    map=NULL;
  }
};

#endif
