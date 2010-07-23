#ifndef VALUE_ARRAY_H
#define VALUE_ARRAY_H

//TODO: option for non-constructed version (type_traits pod?), option for small array optimization (if sz < N, store inline in union, see small_vector.h)

#include <cstdlib>
#include <algorithm>
#include <new>
#include <boost/range.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits.hpp>
#include <cstring>
#ifdef USE_BOOST_SERIALIZE
# include <boost/serialization/split_member.hpp>
# include <boost/serialization/access.hpp>
#endif

// valarray like in that size is fixed (so saves space compared to vector), but same interface as vector (less resize/push_back/insert, of course)
template <class T, class A = std::allocator<T> >
class ValueArray : A // private inheritance so stateless allocator adds no size.
{
public:
  static const int SV_MAX=sizeof(T)/sizeof(T*)>1?sizeof(T)/sizeof(T*):1;
  //space optimization: SV_MAX T will fit inside what would otherwise be a pointer to heap data.  todo in the far future if bored.
  typedef T value_type;
  typedef T& reference;
  typedef T const& const_reference;
  typedef T* iterator;
  typedef T const* const_iterator;
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;
  typedef T* pointer;

  size_type size() const { return sz; }
  bool empty() const { return size() == 0; }

  iterator begin() { return array; }
  iterator end() { return array + size(); }
  const_iterator begin() const { return array; }
  const_iterator end() const { return array + size(); }

  reference operator[](size_type pos) { return array[pos]; }
  const_reference operator[](size_type pos) const { return array[pos]; }

  reference at(size_type pos) { return array[pos]; }
  const_reference at(size_type pos) const { return array[pos]; }

  reference front() { return array[0]; }
  reference back() { return array[sz-1]; }

  const_reference front() const { return array[0]; }
  const_reference back() const { return array[sz-1]; }

  ValueArray() : sz(0), array(NULL) {}

  explicit ValueArray(size_type s, const_reference t = T())
  {
    init(s,t);
  }

protected:
  inline void init(size_type s, const_reference t = T()) {
    sz=s;
    array=A::allocate(s);
    for (size_type i = 0; i != sz; ++i) { A::construct(array + i,t); }
  }
public:
  void resize(size_type s, const_reference t = T()) {
    clear();
    init(s,t);
  }

  template <class I>
  ValueArray(I itr, I end)
    : sz(std::distance(itr,end))
    , array(A::allocate(sz))
  {
    copy_construct(itr,end,array);
  }

  ~ValueArray() {
    clear();
  }

  void clear()
  {
    for (size_type i = sz; i != 0; --i) {
      A::destroy(array + (i - 1));
    }
    if (array != NULL) A::deallocate(array,sz);
  }

  void swap(ValueArray& other)
  {
    std::swap(sz,other.sz);
    std::swap(array,other.array);
  }

  ValueArray(ValueArray const& other)
    : sz(other.sz)
    , array(A::allocate(sz))
  {
    copy_construct(other.begin(),other.end(),array);
  }

  ValueArray& operator=(ValueArray const& other)
  {
    ValueArray(other).swap(*this);
    return *this;
  }

  template <class Range>
  ValueArray( Range const& v
              , typename boost::disable_if< boost::is_integral<Range> >::type* = 0)
    : sz(boost::size(v))
    , array(A::allocate(sz))
  {
    copy_construct(boost::begin(v),boost::end(v),array);
  }

  template <class Range> typename
  boost::disable_if<
    boost::is_integral<Range>
   , ValueArray>::type& operator=(Range const& other)
  {
    ValueArray(other).swap(*this);
    return *this;
  }

private:
//friend class boost::serialization::access;

template <class I1, class I2>
void copy_construct(I1 itr, I1 end, I2 into)
{
  for (; itr != end; ++itr, ++into) A::construct(into,*itr);
}

template <class Archive>
void save(Archive& ar, unsigned int version) const
{
  ar << sz;
  for (size_type i = 0; i != sz; ++i) ar << at(i);
}

template <class Archive>
void load(Archive& ar, unsigned int version)
{
  size_type s;
  ar >> s;
  ValueArray v(s);
  for (size_type i = 0; i != s; ++i) ar >> v[i];
  this->swap(v);
}
#ifdef USE_BOOST_SERIALIZE
BOOST_SERIALIZATION_SPLIT_MEMBER()
#endif
size_type sz;
pointer array;
};


template <class T, class A>
bool operator==(ValueArray<T,A> const& v1, ValueArray<T,A> const& v2)
{
  return (v1.size() == v2.size()) and
    std::equal(v1.begin(),v1.end(),v2.begin());
}


template <class T,class A>
bool operator< (ValueArray<T,A> const& v1, ValueArray<T,A> const& v2)
{
  return std::lexicographical_compare( v1.begin()
                                       , v1.end()
                                       , v2.begin()
                                       , v2.end() );
}

template <class T,class A>
void memcpy(void *out,ValueArray<T,A> const& v) {
  std::memcpy(out,v.begin(),v.size()*sizeof(T));
}


#endif
