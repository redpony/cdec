#ifndef _alphabet_hh
#define _alphabet_hh

#include <cassert>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

// Alphabet: indexes a set of types 
template <typename T>
class Alphabet: protected std::map<T, int>
{
public:
    Alphabet() {};

    bool empty() const { return std::map<T,int>::empty(); }
    int size() const { return std::map<T,int>::size(); }

    int operator[](const T &k) const
    {
        typename std::map<T,int>::const_iterator cit = find(k);
        if (cit != std::map<T,int>::end())
            return cit->second;
        else
            return -1;
    }

    int lookup(const T &k) const { return (*this)[k]; }

    int insert(const T &k) 
    {
        int sz = size();
        assert((unsigned) sz == _items.size());

        std::pair<typename std::map<T,int>::iterator, bool>
            ins = std::map<T,int>::insert(make_pair(k, sz));

        if (ins.second) 
            _items.push_back(k);

        return ins.first->second;
    }

    const T &type(int i) const
    {
        assert(i >= 0);
        assert(i < size());
        return _items[i];
    }

    std::ostream &display(std::ostream &out, int i) const
    {
        return out << type(i);
    }

private:
    std::vector<T> _items;
};

#endif
