#ifndef _SPARSE_VECTOR_H_
#define _SPARSE_VECTOR_H_

#define SPARSE_VECTOR_HASH

#ifdef SPARSE_VECTOR_HASH
#include "hash.h"
# define SPARSE_VECTOR_MAP HASH_MAP
# define SPARSE_VECTOR_MAP_RESERVED(h,empty,deleted) HASH_MAP_RESERVED(h,empty,deleted)
#else
# define SPARSE_VECTOR_MAP std::map
# define SPARSE_VECTOR_MAP_RESERVED(h,empty,deleted)
#endif
/*
   use SparseVectorList (pair smallvector) for feat funcs / hypergraphs (you rarely need random access; just append a feature to the list)
*/
/* hack: index 0 never gets printed because cdyer is creative and efficient. features which have no weight got feature dict id 0, see, and the models all clobered that value.  nobody wants to see it.  except that vlad is also creative and efficient and stored the oracle bleu there. */


// this is a modified version of code originally written
// by Phil Blunsom

#include <iostream>
#include <map>
#include <tr1/unordered_map>
#include <vector>
#include <valarray>

#include "fdict.h"
#include "small_vector.h"

template <class T>
inline T & extend_vector(std::vector<T> &v,int i) {
  if (i>=v.size())
    v.resize(i+1);
  return v[i];
}

template <typename T>
class SparseVector {
  void init_reserved() {
    SPARSE_VECTOR_MAP_RESERVED(values_,-1,-2);
  }
public:
  typedef SparseVector<T> Self;
  typedef SPARSE_VECTOR_MAP<int, T> MapType;
  typedef typename MapType::const_iterator const_iterator;
  SparseVector() {
    init_reserved();
  }
  explicit SparseVector(std::vector<T> const& v) {
    init_reserved();
    typename MapType::iterator p=values_.begin();
    const T z=0;
    for (unsigned i=0;i<v.size();++i) {
      T const& t=v[i];
      if (t!=z)
        p=values_.insert(p,typename MapType::value_type(i,t)); //hint makes insertion faster
    }
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
    return values_.find(index) != values_.end();
  }


  T operator[](int index) const {
    typename MapType::const_iterator found = values_.find(index);
    if (found == values_.end())
      return 0;
    else
      return found->second;
  }

  void set_value(int index, const T &value) {
    values_[index] = value;
  }

    T const& add_value(int index, const T &value) {
      std::pair<typename MapType::iterator,bool> art=values_.insert(std::make_pair(index,value));
      T &val=art.first->second;
      if (!art.second) val += value; // already existed
      return val;
    }

    T value(int index) const {
        typename MapType::const_iterator found = values_.find(index);
        if (found != values_.end())
            return found->second;
        else
            return 0;
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
        T sum = 0;
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
        S sum = 0;
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
        S sum = 0;
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
        S sum = 0;
        for (typename MapType::const_iterator
                it = values_.begin(); it != values_.end(); ++it)
            sum += it->second * vec[it->first];
        std::cout << "dot(*vec) " << sum << std::endl;
        return sum;
    }

    T l1norm() const {
      T sum = 0;
      for (typename MapType::const_iterator
              it = values_.begin(); it != values_.end(); ++it)
        sum += fabs(it->second);
      return sum;
    }

  T l2norm_sq() const {
      T sum = 0;
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

    SparseVector<T> &operator+=(const SparseVector<T> &other) {
        for (typename MapType::const_iterator
                it = other.values_.begin(); it != other.values_.end(); ++it)
        {
//            T v =
              (values_[it->first] += it->second);
//            if (v == 0) values_.erase(it->first);
        }
        return *this;
    }

    SparseVector<T> &operator-=(const SparseVector<T> &other) {
        for (typename MapType::const_iterator
                it = other.values_.begin(); it != other.values_.end(); ++it)
        {
//            T v =
          (values_[it->first] -= it->second);
//            if (v == 0) values_.erase(it->first);
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
          if (it->first == 0) continue;
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

  bool contains(Self const &o) const {
    return size()>o.size() && contains(o);
  }

  bool at_equals(int i,T const& val) const {
    const_iterator it=values_.find(i);
    if (it==values_.end()) return val==0;
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
      if (values_.find(i)==values_.end())
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

private:
  MapType values_;
};

// doesn't support fast indexing directly
template <class T>
class SparseVectorList {
  typedef typename std::pair<int,T> Pair;
  typedef SmallVector<Pair,1> List;
  typedef typename List::const_iterator const_iterator;
  SparseVectorList() {  }
  template <class I>
  SparseVectorList(I i,I const& end) {
    const T z=0;
    int c=0;
    for (;i<end;++i,++c) {
      if (*i!=z)
        p.push_back(Pair(c,*i));
    }
    p.compact();
  }
  explicit SparseVectorList(std::vector<T> const& v) {
    const T z=0;
    for (unsigned i=0;i<v.size();++i) {
      T const& t=v[i];
      if (t!=z)
        p.push_back(Pair(i,t));
    }
    p.compact();
  }
  // unlike SparseVector, this doesn't overwrite - but conversion to SparseVector will use last value, which is the same
  void set_value(int i,T const& val) {
    p.push_back(Pair(i,val));
  }
  void overlay(SparseVector<T> *to) const {
    for (int i=0;i<p.size();++i)
      to->set_value(p[i].first,p[i].second);
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

typedef SparseVectorList<double> FeatureVectorList;
typedef SparseVector<double> FeatureVector;
typedef SparseVector<double> WeightVector;
typedef std::vector<double> DenseWeightVector;
template <typename T>
SparseVector<T> operator+(const SparseVector<T>& a, const SparseVector<T>& b) {
  SparseVector<T> result = a;
  return result += b;
}

template <typename T>
SparseVector<T> operator*(const SparseVector<T>& a, const double& b) {
  SparseVector<T> result = a;
  return result *= b;
}

template <typename T>
SparseVector<T> operator*(const SparseVector<T>& a, const T& b) {
  SparseVector<T> result = a;
  return result *= b;
}

template <typename T>
SparseVector<T> operator*(const double& a, const SparseVector<T>& b) {
  SparseVector<T> result = b;
  return result *= a;
}

template <typename T>
std::ostream &operator<<(std::ostream &out, const SparseVector<T> &vec)
{
    return vec.operator<<(out);
}

namespace B64 {
  void Encode(double objective, const SparseVector<double>& v, std::ostream* out);
  // returns false if failed to decode
  bool Decode(double* objective, SparseVector<double>* v, const char* data, size_t size);
}

#endif
