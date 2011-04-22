#ifndef _SPARSE_VECTOR_H_
#define _SPARSE_VECTOR_H_

#undef USE_FAST_SPARSE_VECTOR
#ifndef USE_FAST_SPARSE_VECTOR
/*
TODO: specialize for int value types, where it probably makes sense to check if adding/subtracting brings a value to 0, and remove it from the map (e.g. in a gibbs sampler).  or add a separate policy argument for that.
 */

//#define SPARSE_VECTOR_HASH
// if defined, use hash_map rather than map.  map is probably faster/smaller for small vectors

/*
   use SparseVectorList (pair smallvector) for feat funcs / hypergraphs (you rarely need random access; just append a feature to the list)
*/
/* hack: index 0 never gets printed because cdyer is creative and efficient. features which have no weight got feature dict id 0, see, and the models all clobered that value.  nobody wants to see it.  except that vlad is also creative and efficient and stored the oracle bleu there. */
/* NOTE: zero vals may or may not be dropped from map (sparse, but not guaranteed to be so).

   I rely on !v the same as !((bool)v) the same as v==0 and v() same as v(0).

   one exception:

   a local:
   T sum = 0;
   is used instead of
   T sum;

   because T may be a primitive type, and

   T sum();

   is parsed as a function decl :(

   the alternative T sum=T() is also be reasonable.  i've switched to that.
*/

// this is a modified version of code originally written
// by Phil Blunsom

#include <boost/functional/hash.hpp>
#include <stdexcept>
#ifdef SPARSE_VECTOR_HASH
#include "hash.h"
# define SPARSE_VECTOR_MAP HASH_MAP
# define SPARSE_VECTOR_MAP_RESERVED(h,empty,deleted) HASH_MAP_RESERVED(h,empty,deleted)
#else
# define SPARSE_VECTOR_MAP std::map
# define SPARSE_VECTOR_MAP_RESERVED(h,empty,deleted)
#endif

#include <iostream>
#include <map>
#include <tr1/unordered_map>
#include <vector>
#include <valarray>

#include "fdict.h"
#include "small_vector.h"
#include "string_to.h"

#if HAVE_BOOST_ARCHIVE_TEXT_OARCHIVE_HPP
#include <boost/serialization/map.hpp>
#endif

template <class T>
inline T & extend_vector(std::vector<T> &v,int i) {
  if (i>=v.size())
    v.resize(i+1);
  return v[i];
}

template <class T>
class SparseVector {
  void init_reserved() {
    SPARSE_VECTOR_MAP_RESERVED(values_,-1,-2);
  }
public:
  T const& get_singleton() const {
    assert(values_.size()==1);
    return values_.begin()->second;
  }

  typedef SparseVector<T> Self;
  typedef SPARSE_VECTOR_MAP<int, T> MapType;
  typedef typename MapType::const_iterator const_iterator;
  SparseVector() {
    init_reserved();
  }
  typedef typename MapType::value_type value_type;
  typedef typename MapType::iterator iterator;
  explicit SparseVector(std::vector<T> const& v) {
    init_reserved();
    iterator p=values_.begin();
    const T z=0;
    for (unsigned i=0;i<v.size();++i) {
      T const& t=v[i];
      if (t!=z)
        p=values_.insert(p,value_type(i,t)); //hint makes insertion faster
    }
  }

  typedef char const* Str;
  template <class O>
  void print(O &o,Str kvsep="=",Str pairsep=" ",Str pre="",Str post="") const {
    o << pre;
    bool first=true;
    for (const_iterator i=values_.begin(),e=values_.end();i!=e;++i) {
      if (first)
        first=false;
      else
        o<<pairsep;
      o<<FD::Convert(i->first)<<kvsep<<i->second;
    }
    o << post;
  }

  static void error(std::string const& msg) {
    throw std::runtime_error("SparseVector: "+msg);
  }

  enum DupPolicy {
    NO_DUPS,
    KEEP_FIRST,
    KEEP_LAST,
    SUM
  };

  // either key val alternating whitespace sep, or key=val (kvsep char is '=').  end at eof or terminator (non-ws) char
  template <class S>
  void read(S &s,DupPolicy dp=NO_DUPS,bool use_kvsep=true,char kvsep='=',bool use_pairsep=true,char optional_pairsep=';',bool stop_at_terminator=false,char terminator=')') {
    values_.clear();
    std::string id;
    WordID k;
    T v;
#undef SPARSE_MUST_READ
#define SPARSE_MUST_READ(x) if (!(x)) error(#x);
    int ki;
    while (s) {
      if (stop_at_terminator||use_pairsep) {
        char c;
        if (!(s>>c)) goto eof;
        if (stop_at_terminator && c==terminator) return;
        if (!use_pairsep || c!=optional_pairsep)
          s.unget();
      }
      if (!(s>>id)) goto eof;
      if (use_kvsep && (ki=id.find(kvsep))!=std::string::npos) {
        k=FD::Convert(std::string(id,0,ki));
        string_into(id.c_str()+ki+1,v);
      } else {
        k=FD::Convert(id);
        if (!(s>>v)) error("reading value failed");
      }
      std::pair<iterator,bool> vi=values_.insert(value_type(k,v));
      if (!vi.second) {
        T &oldv=vi.first->second;
        switch(dp) {
        case NO_DUPS: error("read duplicate key with NO_DUPS.  key="
                            +FD::Convert(k)+" val="+to_string(v)+" old-val="+to_string(oldv));
          break;
        case KEEP_FIRST: break;
        case KEEP_LAST: oldv=v; break;
        case SUM: oldv+=v; break;
        }
      }
    }
    goto good;
  eof:
    if (!s.eof()) error("reading key failed (before EOF)");
  good:
    s.clear(); // we may have reached eof, but that's no error.
  }

  friend inline std::ostream & operator<<(std::ostream &o,Self const& s) {
    s.print(o);
    return o;
  }

  friend inline std::istream & operator>>(std::istream &o,Self & s) {
    s.read(o);
    return o;
  }

  void init_vector(std::vector<T> *vp) const {
    init_vector(*vp);
  }

  void init_vector(std::vector<T> &v) const {
    v.clear();
    for (const_iterator i=values_.begin(),e=values_.end();i!=e;++i)
      extend_vector(v,i->first)=i->second;
  }

  void set_new_value(int index, T const& val) {
    assert(values_.find(index)==values_.end());
    values_[index]=val;
  }


  // warning: exploits the fact that 0 values are always removed from map.  change this if you change that.
  bool nonzero(int index) const {
    typename MapType::const_iterator found = values_.find(index);
    return found==values_.end() || !found->second;
  }

  void remove_zeros() {
    typename MapType::iterator it = values_.begin();
    for (; it != values_.end(); ++it)
      if (!it->second) values_.erase(it);
  }

  T get(int index) const {
    typename MapType::const_iterator found = values_.find(index);
    return found==values_.end()?T():found->second;
  }

  T value(int i) const { return get(i); }

  // same as above but may add a 0 entry.  TODO: check that people relying on no entry use get
  T & operator[](int index){
    return values_[index];
  }

  inline void maybe_set_value(int index, const T &value) {
    if (value) values_[index] = value;
  }

  inline void set_value(int index, const T &value) {
    values_[index] = value;
  }

  inline void maybe_add(int index, const T& value) {
    if (value) add_value(index,value);
  }

    T& add_value(int index, const T &value) {
#if 1
      return values_[index]+=value;
#else
      // this is not really going to be any faster, and we already rely on default init = 0 init
      std::pair<typename MapType::iterator,bool> art=values_.insert(std::make_pair(index,value));
      T &val=art.first->second;
      if (!art.second) val += value; // already existed
      return val;
#endif
    }


    void store(std::valarray<T>* target) const {
      (*target) *= 0;
      for (typename MapType::const_iterator
              it = values_.begin(); it != values_.end(); ++it) {
        if (it->first >= target->size()) break;
        (*target)[it->first] = it->second;
      }
    }

    int max_index() const {
      if (empty()) return 0;
        typename MapType::const_iterator found =values_.end();
        --found;
        return found->first;
    }

    // dot product with a unit vector of the same length
    // as the sparse vector
    T dot() const {
        T sum = T();
        for (typename MapType::const_iterator
                it = values_.begin(); it != values_.end(); ++it)
            sum += it->second;
        return sum;
    }

    template<typename S>
    S cosine_sim(const SparseVector<S> &vec) const {
      return dot(vec)/(l2norm()*vec.l2norm());
    }

  // if values are binary, gives |A intersect B|/|A union B|
    template<typename S>
    S tanimoto_coef(const SparseVector<S> &vec) const {
      S dp=dot(vec);
      return dp/(l2norm_sq()+vec.l2norm_sq()-dp);
    }

    template<typename S>
    S dot(const SparseVector<S> &vec) const {
        S sum = S();
        for (typename MapType::const_iterator
                it = values_.begin(); it != values_.end(); ++it)
        {
            typename MapType::const_iterator
                found = vec.values_.find(it->first);
            if (found != vec.values_.end())
                sum += it->second * found->second;
        }
        return sum;
    }

    template<typename S>
    S dot(const std::vector<S> &vec) const {
      S sum = S();
        for (typename MapType::const_iterator
                it = values_.begin(); it != values_.end(); ++it)
        {
            if (it->first < static_cast<int>(vec.size()))
                sum += it->second * vec[it->first];
        }
        return sum;
    }

    template<typename S>
    S dot(const S *vec) const {
        // this is not range checked!
        S sum = S();
        for (typename MapType::const_iterator
                it = values_.begin(); it != values_.end(); ++it)
            sum += it->second * vec[it->first];
        std::cout << "dot(*vec) " << sum << std::endl;
        return sum;
    }

    T l1norm() const {
      T sum = T();
      for (typename MapType::const_iterator
              it = values_.begin(); it != values_.end(); ++it)
        sum += fabs(it->second);
      return sum;
    }

  T l2norm_sq() const {
      T sum = T();
      for (typename MapType::const_iterator
              it = values_.begin(); it != values_.end(); ++it)
        sum += it->second * it->second;
      return sum;
  }

    T l2norm() const {
      return sqrt(l2norm_sq());
    }

  void erase(int key) {
    values_.erase(key);
/*    typename MapType::iterator found = values_.find(key);
    if (found!=values_end())
    values_.erase(found);*/
  }

  template <class T2>
  void set_from(SparseVector<T2> const& other) {
    for (typename MapType::const_iterator
           it = other.values_.begin(); it != other.values_.end(); ++it)
    {
      values_[it->first]=it->second;
    }
  }

    SparseVector<T> &operator+=(const SparseVector<T> &other) {
        for (typename MapType::const_iterator
                it = other.values_.begin(); it != other.values_.end(); ++it)
        {
//            T v =
              (values_[it->first] += it->second);
//            if (!v) values_.erase(it->first);
        }
        return *this;
    }

    template <typename R>
    SparseVector<T> &operator+=(const SparseVector<R> &other) {
        for (typename SparseVector<R>::MapType::const_iterator
                it = other.values_.begin(); it != other.values_.end(); ++it)
        {
//            T v =
              (values_[it->first] += it->second);
//            if (!v) values_.erase(it->first);
        }
        return *this;
    }

    SparseVector<T> &operator-=(const SparseVector<T> &other) {
        for (typename MapType::const_iterator
                it = other.values_.begin(); it != other.values_.end(); ++it)
        {
//            T v =
          (values_[it->first] -= it->second);
//            if (!v) values_.erase(it->first);
        }
        return *this;
    }

  friend SparseVector<T> operator -(SparseVector<T> x,SparseVector<T> const& y) {
    x-=y;
    return x;
  }
  friend SparseVector<T> operator +(SparseVector<T> x,SparseVector<T> const& y) {
    x+=y;
    return x;
  }

private:
  // DEPRECATED: becuase 0 values are dropped from the map, this doesn't even make sense if you have a fully populated (not really sparse re: what you'll ever use) vector
    SparseVector<T> &operator-=(T const& x) {
        for (typename MapType::iterator
                it = values_.begin(); it != values_.end(); ++it)
            it->second -= x;
        return *this;
    }

    SparseVector<T> &operator+=(T const& x) {
        for (typename MapType::iterator
                it = values_.begin(); it != values_.end(); ++it)
            it->second += x;
        return *this;
    }
public:
    SparseVector<T> &operator/=(const T &x) {
        for (typename MapType::iterator
                it = values_.begin(); it != values_.end(); ++it)
            it->second /= x;
        return *this;
    }

    SparseVector<T> &operator*=(const T& x) {
        for (typename MapType::iterator
                it = values_.begin(); it != values_.end(); ++it)
            it->second *= x;
        return *this;
    }

    SparseVector<T> operator+(T const& x) const {
        SparseVector<T> result = *this;
        return result += x;
    }

    SparseVector<T> operator-(T const& x) const {
        SparseVector<T> result = *this;
        return result -= x;
    }

    SparseVector<T> operator/(T const& x) const {
        SparseVector<T> result = *this;
        return result /= x;
    }

    std::ostream &operator<<(std::ostream& out) const {
      Write(true, &out);
      return out;
    }

    void Write(const bool with_semi, std::ostream* os) const {
        bool first = true;
        for (typename MapType::const_iterator
                it = values_.begin(); it != values_.end(); ++it) {
          // by definition feature id 0 is a dummy value
          if (!it->first) continue;
          if (with_semi) {
            (*os) << (first ? "" : ";")
	         << FD::Convert(it->first) << '=' << it->second;
          } else {
            (*os) << (first ? "" : " ")
	         << FD::Convert(it->first) << '=' << it->second;
          }
          first = false;
        }
    }

  bool operator==(Self const & other) const {
    return size()==other.size() && contains_keys_of(other) && other.contains_i(*this);
  }

  std::size_t hash_impl() const {
    return boost::hash_range(begin(),end());
  }

  bool contains(Self const &o) const {
    return size()>o.size() && contains(o);
  }

  bool at_equals(int i,T const& val) const {
    const_iterator it=values_.find(i);
    if (it==values_.end()) return !val;
    return it->second==val;
  }

  bool contains_i(Self const& o) const {
    for (typename MapType::const_iterator i=o.begin(),e=o.end();i!=e;++i)
      if (!at_equals(i->first,i->second))
        return false;
    return true;
  }

  bool contains_keys_of(Self const& o) const {
    for (typename MapType::const_iterator i=o.begin(),e=o.end();i!=e;++i)
      if (values_.find(i->first)==values_.end())
        return false;
    return true;
  }

#ifndef SPARSE_VECTOR_HASH
    bool operator<(const SparseVector<T> &other) const {
        typename MapType::const_iterator it = values_.begin();
        typename MapType::const_iterator other_it = other.values_.begin();

        for (; it != values_.end() && other_it != other.values_.end(); ++it, ++other_it)
        {
            if (it->first < other_it->first) return true;
            if (it->first > other_it->first) return false;
            if (it->second < other_it->second) return true;
            if (it->second > other_it->second) return false;
        }
        return values_.size() < other.values_.size();
    }
#endif

  int size() const { return values_.size(); }

    int num_active() const { return values_.size(); }
    bool empty() const { return values_.empty(); }

    const_iterator begin() const { return values_.begin(); }
    const_iterator end() const { return values_.end(); }

    void clear() {
        values_.clear();
    }

    void swap(SparseVector<T>& other) {
      values_.swap(other.values_);
    }

  MapType values_;
private:

#if HAVE_BOOST_ARCHIVE_TEXT_OARCHIVE_HPP
  friend class boost::serialization::access;
  template<class Archive>
  void save(Archive & ar, const unsigned int version) const {
    (void) version;
    int eff_size = values_.size();
    const_iterator it = this->begin();
    if (values_.find(0) != values_.end()) { ++it; --eff_size; }
    ar & eff_size;
    while (it != this->end()) {
      const std::pair<const std::string&, const T&> wire_pair(FD::Convert(it->first), it->second);
      ar & wire_pair;
      ++it;
    }
  }
  template<class Archive>
  void load(Archive & ar, const unsigned int version) {
    (void) version;
    this->clear();
    int sz; ar & sz;
    for (int i = 0; i < sz; ++i) {
      std::pair<std::string, T> wire_pair;
      ar & wire_pair;
      this->set_value(FD::Convert(wire_pair.first), wire_pair.second);
    }
  }
  BOOST_SERIALIZATION_SPLIT_MEMBER()
#endif
};

#if HAVE_BOOST_ARCHIVE_TEXT_OARCHIVE_HPP
BOOST_CLASS_TRACKING(SparseVector<double>,track_never)
#endif

template <class T>
inline void swap(SparseVector<T> &a,SparseVector<T> &b) {
  a.swap(b);
}

//like a pair but can live in a union, because it lacks default+copy ctors, dtor.
template <class T>
struct feature_val {
  int fid;
  T val;
};

template <class T>
inline feature_val<T> featval(int fid,T const &val) {
  feature_val<T> f;
  f.fid=fid;
  f.val=val;
  return f;
}


// doesn't support fast indexing directly
template <class T>
class SparseVectorList {
  typedef feature_val<T> Pair;
  typedef SmallVector<Pair,1> List;
  typedef typename List::const_iterator const_iterator;
  SparseVectorList() {  }
  template <class I>
  SparseVectorList(I i,I const& end) {
    int c=0;
    for (;i<end;++i,++c) {
      if (*i)
        p.push_back(featval(c,*i));
    }
    p.compact();
  }
  explicit SparseVectorList(std::vector<T> const& v) {
    for (unsigned i=0;i<v.size();++i) {
      T const& t=v[i];
      if (t)
        p.push_back(featval(i,t));
    }
    p.compact();
  }
  // unlike SparseVector, this doesn't overwrite - but conversion to SparseVector will use last value, which is the same
  void set_value(int i,T const& val) {
    p.push_back(Pair(i,val));
  }
  void overlay(SparseVector<T> *to) const {
    for (int i=0;i<p.size();++i)
      to->set_value(p[i].fid,p[i].val);
  }
  void copy_to(SparseVector<T> *to) const {
    to->clear();
    overlay(to);
  }
  SparseVector<T> sparse() const {
    SparseVector<T> r;
    copy_to(r);
    return r;
  }
private:
  List p;
};

template <class T>
std::size_t hash_value(SparseVector<T> const& x) {
  return x.hash_impl();
}

template <class T>
SparseVector<T> operator+(const SparseVector<T>& a, const SparseVector<T>& b) {
  SparseVector<T> result = a;
  return result += b;
}

template <class T>
SparseVector<T> operator*(const double& a, const SparseVector<T>& b) {
  SparseVector<T> result = b;
  return result *= a;
}

#else

#include "fast_sparse_vector.h"
#define SparseVector FastSparseVector

#endif

template <class T>
SparseVector<T> operator*(const SparseVector<T>& a, const double& b) {
  SparseVector<T> result = a;
  return result *= b;
}

template <class T>
SparseVector<T> operator*(const SparseVector<T>& a, const T& b) {
  SparseVector<T> result = a;
  return result *= b;
}

template <class T>
SparseVector<T> operator/(const SparseVector<T>& a, const double& b) {
  SparseVector<T> result = a;
  return result *= b;
}

template <class T>
SparseVector<T> operator/(const SparseVector<T>& a, const T& b) {
  SparseVector<T> result = a;
  return result *= b;
}

#include "fdict.h"

template <class O, typename T>
inline void print(O &o,const SparseVector<T>& v, const char* kvsep="=",const char* pairsep=" ",const char* pre="",const char* post="") {
  o << pre;
  bool first=true;
  for (typename SparseVector<T>::const_iterator i=v.begin(),e=v.end();i!=e;++i) {
    if (first)
      first=false;
    else
      o<<pairsep;
    o<<FD::Convert(i->first)<<kvsep<<i->second;
  }
  o << post;
}

template <typename T>
inline std::ostream& operator<<(std::ostream& out, const SparseVector<T>& v) {
  print(out, v);
  return out;
}

namespace B64 {
  void Encode(double objective, const SparseVector<double>& v, std::ostream* out);
  // returns false if failed to decode
  bool Decode(double* objective, SparseVector<double>* v, const char* data, size_t size);
}

#endif
