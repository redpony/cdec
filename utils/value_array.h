#ifndef VALUE_ARRAY_H
#define VALUE_ARRAY_H

#define DBVALUEARRAY(x) x

#include <cstdlib>
#include <algorithm>
#include <new>
#include <boost/range.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits.hpp>
#include <cstring>
#include <boost/functional/hash.hpp>
#ifdef USE_BOOST_SERIALIZE
# include <boost/serialization/split_member.hpp>
# include <boost/serialization/access.hpp>
#endif

//TODO: use awesome type traits (and/or policy typelist argument) to provide these only when possible?
#define VALUE_ARRAY_ADD 1
#define VALUE_ARRAY_MUL 1
#define VALUE_ARRAY_BITWISE 0
#define VALUE_ARRAY_OSTREAM 1

#if VALUE_ARRAY_OSTREAM
# include <iostream>
#endif

// valarray like in that size is fixed (so saves space compared to vector), but same interface as vector (less resize/push_back/insert, of course)
template <class T, class A = std::allocator<T> >
class ValueArray : A { // private inheritance so stateless allocator adds no size.
  typedef ValueArray<T,A> Self;
public:
#if VALUE_ARRAY_OSTREAM
  friend inline std::ostream & operator << (std::ostream &o,Self const& s) {
    o<<'[';
    for (unsigned i=0,e=s.size();i<e;++i) {
      if (i) o<<' ';
      o<<s[i];
    }
    o<<']';
    return o;
  }
#endif
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
  bool empty() const { return !sz; }

  iterator begin() { return array; }
  iterator end() { return array + sz; }
  const_iterator begin() const { return array; }
  const_iterator end() const { return array + sz; }

  reference operator[](size_type pos) { return array[pos]; }
  const_reference operator[](size_type pos) const { return array[pos]; }

  reference at(size_type pos) { return array[pos]; }
  const_reference at(size_type pos) const { return array[pos]; }

  reference front() { return array[0]; }
  reference back() { return array[sz-1]; }

  const_reference front() const { return array[0]; }
  const_reference back() const { return array[sz-1]; }

  ValueArray() : sz(0), array(NULL) {}

  friend inline std::size_t hash_value(Self const& x) {
    return boost::hash_range(x.begin(),x.end());
  }

protected:
  void destroy()
  {
    if (!array) return;
    // it's cool that this destroys in reverse order of construction, but why bother?
    for (pointer i=array+sz;i>array;)
      A::destroy(--i);
  }
  void dealloc() {
    if (sz && array != NULL) A::deallocate(array,sz);
    sz=0;
  }
  void alloc(size_type s) {
    array=s?A::allocate(s):0;
    sz=s;
  }

  void realloc(size_type s) {
    if (sz!=s) {
      dealloc();
      alloc(s);
    }
  }

  void reinit_noconstruct(size_type s) {
    destroy();
    realloc(s);
  }

  template <class C,class F>
  inline void init_map(C & c,F const& f) {
    alloc(c.size());
    copy_construct_map(c.begin(),c.end(),array,f);
  }
  template <class C,class F>
  inline void init_map(C const& c,F const& f) {
    alloc(c.size());
    copy_construct_map(c.begin(),c.end(),array,f);
  }
  // warning: std::distance is likely slow on maps (anything other than random access containers.  so container version using size will be better
  template <class I,class F>
  inline void init_range_map(I itr, I end,F const& f) {
    alloc(std::distance(itr,end));
    copy_construct_map(itr,end,array,f);
  }
  template <class I>
  inline void init_range(I itr, I end) {
    alloc(std::distance(itr,end));
    copy_construct(itr,end,array);
  }
  inline void fill(const_reference t) {
    for (pointer i=array,e=array+sz;i!=e;++i)
      new(i) T(t);
  }
  inline void fill() {
    for (pointer i=array,e=array+sz;i!=e;++i)
      new(i) T();
  }

  inline void init(size_type s) {
    alloc(s);
    fill();
  }
  inline void init(size_type s, const_reference t) {
    alloc(s);
    fill(t);
  }
public:
  ValueArray(size_type s, const_reference t)
  {
    init(s,t);
  }
  explicit ValueArray(size_type s)
  {
    init(s);
  }
  void reinit(size_type s, const_reference t) {
    reinit_noconstruct(s);
    fill(t);
  }
  void reinit(size_type s) {
    reinit_noconstruct(s);
    fill();
  }

  //copy any existing data like std::vector.  not A::construct exception safe.  try blah blah?  swap?
  void resize(size_type s, const_reference t = T()) {
    if (s) {
      pointer na=A::allocate(s);
      size_type nc=s<sz ? s : sz;
      size_type i=0;
      for (;i<nc;++i)
        A::construct(na+i,array[i]);
      for (;i<s;++i)
        A::construct(na+i,t);
      clear();
      array=na;
      sz=s;
    } else
      clear();
  }

  template <class I>
  void reinit_range(I itr, I end) {
    reinit_noconstruct(std::distance(itr,end));
    copy_construct(itr,end,array);
  }

  template <class I,class F>
  void reinit_map(I itr,I end,F const& f) {
    reinit_noconstruct(std::distance(itr,end));
    copy_construct_map(itr,end,array,f);
  }

  // warning: std::distance is likely slow on maps,lists (anything other than random access containers.  so container version below using size() will be better
  template <class C,class F>
  void reinit_map(C const& c,F const& f) {
    reinit_noconstruct(c.size());
    copy_construct_map(c.begin(),c.end(),array,f);
  }

  template <class C,class F>
  void reinit_map(C & c,F const& f) {
    reinit_noconstruct(c.size());
    copy_construct_map(c.begin(),c.end(),array,f);
  }

  template <class I>
  ValueArray(I itr, I end)
  {
    init_range(itr,end);
  }
  template <class I,class F>
  ValueArray(I itr, I end,F const& map)
  {
    init_range_map(itr,end,map);
  }

  ~ValueArray() {
    clear();
  }

#undef VALUE_ARRAY_OPEQ
#define VALUE_ARRAY_OPEQ(op) template <class T2,class A2> Self & operator op (ValueArray<T2,A2> const& o) { assert(sz==o.sz); for (int i=0,e=sz;i<=e;++i) array[i] op o.array[i]; return *this; }
#if VALUE_ARRAY_ADD
  VALUE_ARRAY_OPEQ(+=)
  VALUE_ARRAY_OPEQ(-=)
#endif
#if VALUE_ARRAY_MUL
  VALUE_ARRAY_OPEQ(*=)
  VALUE_ARRAY_OPEQ(/=)
#endif
#if VALUE_ARRAY_BITWISE
  VALUE_ARRAY_OPEQ(|=)
  VALUE_ARRAY_OPEQ(*=)
#endif
#undef VALUE_ARRAY_OPEQ
#undef VALUE_ARRAY_BINOP
#define VALUE_ARRAY_BINOP(op,opeq) template <class T2,class A2> friend inline Self operator op (Self x,ValueArray<T2,A2> const& y) { x opeq y; return x; }
#if VALUE_ARRAY_ADD
  VALUE_ARRAY_BINOP(+,+=)
  VALUE_ARRAY_BINOP(-,-=)
#endif
#if VALUE_ARRAY_MUL
  VALUE_ARRAY_BINOP(*,*=)
  VALUE_ARRAY_BINOP(/,/=)
#endif
#if VALUE_ARRAY_BITWISE
  VALUE_ARRAY_BINOP(|,|=)
  VALUE_ARRAY_BINOP(*,*=)
#endif

#undef VALUE_ARRAY_BINOP

  void clear()
  {
    destroy();
    dealloc();
  }

  void swap(ValueArray& other)
  {
    std::swap(sz,other.sz);
    std::swap(array,other.array);
  }

  ValueArray(ValueArray const& other)
    : A(other)
    , sz(other.sz)
    , array(sz?A::allocate(sz):0)

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
    , array(sz?A::allocate(sz):0)
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

  template <class I1>
  void copy_construct(I1 itr, I1 end, T *into)
  {
    for (; itr != end; ++itr, ++into) A::construct(into,*itr);
  }

  //TODO: valgrind doesn't think this works.
  template <class I1,class F>
  void copy_construct_map(I1 itr, I1 end, T *into,F const& f)
  {
    while ( itr != end) {
      DBVALUEARRAY(assert(into<array+sz));
      A::construct(into,f(*itr));
      ++itr;++into;
    }

  }
  //friend class boost::serialization::access;
public:
  template <class Archive>
  void save(Archive& ar, unsigned int /*version*/) const
  {
    ar << sz;
    for (size_type i = 0; i != sz; ++i) ar << at(i);
  }

  template <class Archive>
  void load(Archive& ar, unsigned int /*version*/)
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
private:
  size_type sz;
  pointer array;
};


template <class T, class A>
bool operator==(ValueArray<T,A> const& v1, ValueArray<T,A> const& v2)
{
  return (v1.size() == v2.size()) and
    std::equal(v1.begin(),v1.end(),v2.begin());
}

template <class A>
bool operator==(ValueArray<char,A> const& v1, ValueArray<char,A> const& v2)
{
  typename ValueArray<char,A>::size_type sz=v1.size();
  return sz == v2.size() &&
    0==std::memcmp(v1.begin(),v2.begin(),sizeof(char)*sz);
}

template <class A>
bool operator==(ValueArray<unsigned char,A> const& v1, ValueArray<unsigned char,A> const& v2)
{
  typename ValueArray<char,A>::size_type sz=v1.size();
  return sz == v2.size() &&
    0==std::memcmp(v1.begin(),v2.begin(),sizeof(char)*sz);
}

template <class T,class A>
bool operator< (ValueArray<T,A> const& v1, ValueArray<T,A> const& v2)
{
  return std::lexicographical_compare( v1.begin()
                                       , v1.end()
                                       , v2.begin()
                                       , v2.end() );
}

template <class A>
bool operator<(ValueArray<unsigned char,A> const& v1, ValueArray<unsigned char,A> const& v2)
{
  typename ValueArray<char,A>::size_type sz=v1.size();
  return sz == v2.size() &&
    std::memcmp(v1.begin(),v2.begin(),sizeof(char)*sz)<0;
}


template <class T,class A>
void memcpy(void *out,ValueArray<T,A> const& v) {
  std::memcpy(out,v.begin(),v.size()*sizeof(T));
}


#endif
