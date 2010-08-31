#ifndef UTILS__SHOW_H
#define UTILS__SHOW_H


//usage: string s=OSTR(1<<" "<<c);
#define OSTR(expr) ((dynamic_cast<ostringstream &>(ostringstream()<<std::dec<<expr)).str())
#define OSTRF(f) ((dynamic_cast<ostringstream &>(f(ostringstream()<<std::dec))).str())
#define OSTRF1(f,x) ((dynamic_cast<ostringstream &>(f(ostringstream()<<std::dec,x))).str())
#define OSTRF2(f,x1,x2) ((dynamic_cast<ostringstream &>(f(ostringstream()<<std::dec,x1,x2))).str())
// std::dec (or seekp, or another manip) is needed to convert to std::ostream reference.

#ifndef SHOWS
#include <iostream>
#define SHOWS std::cerr
#endif

#define SELF_TYPE_PRINT                                                                     \
    template <class Char,class Traits> \
    inline friend std::basic_ostream<Char,Traits> & operator <<(std::basic_ostream<Char,Traits> &o, self_type const& me)     \
    { me.print(o);return o; } \
    typedef self_type has_print;

#define SELF_TYPE_PRINT_ANY_STREAM \
    template <class O> \
    friend inline O & operator <<(O &o, self_type const& me)     \
    { me.print(o);return o; } \
    typedef self_type has_print;

#define SELF_TYPE_PRINT_OSTREAM \
    friend inline std::ostream & operator <<(std::ostream &o, self_type const& me)     \
    { me.print(o);return o; } \
    typedef self_type has_print;

#define PRINT_SELF(self) typedef self self_type; SELF_TYPE_PRINT_OSTREAM



#undef SHOWALWAYS
#define SHOWALWAYS(x) x

/* usage:
#if DEBUG
# define IFD(x) x
#else
# define IFD(x)
#endif

SHOWS(IFD,x) SHOWS(IFD,y) SHOW(IFD,nl_after)

will print x=X y=Y nl_after=NL_AFTER\n if DEBUG.

SHOW3(IFD,x,y,nl_after) is short for the same

SHOWP("a") will just print "a"

careful: none of this is wrapped in a block.  so you can't use one of these macros as a single-line block.

 */


#define SHOWP(IF,x) IF(SHOWS<<x;)
#define SHOWNL(IF) SHOWP("\n")
#define SHOWC(IF,x,s) SHOWP(IF,#x<<"="<<x<<s)
#define SHOW(IF,x) SHOWC(IF,x,"\n")
#define SHOW1(IF,x) SHOWC(IF,x," ")
#define SHOW2(IF,x,y) SHOW1(IF,x) SHOW(IF,y)
#define SHOW3(IF,x,y0,y1) SHOW1(IF,x) SHOW2(IF,y0,y1)
#define SHOW4(IF,x,y0,y1,y2) SHOW1(IF,x) SHOW3(IF,y0,y1,y2)
#define SHOW5(IF,x,y0,y1,y2,y3) SHOW1(IF,x) SHOW4(IF,y0,y1,y2,y3)
#define SHOW6(IF,x,y0,y1,y2,y3,y4) SHOW1(IF,x) SHOW5(IF,y0,y1,y2,y3,y4)
#define SHOW7(IF,x,y0,y1,y2,y3,y4,y5) SHOW1(IF,x) SHOW6(IF,y0,y1,y2,y3,y4,y5)

#define SHOWM(IF,m,x) SHOWP(IF,m<<": ") SHOW(IF,x)
#define SHOWM1(IF,m,x) SHOWM(IF,m,x)
#define SHOWM2(IF,m,x0,x1) SHOWP(IF,m<<": ") SHOW2(IF,x0,x1)
#define SHOWM3(IF,m,x0,x1,x2) SHOWP(IF,m<<": ") SHOW3(IF,x0,x1,x2)
#define SHOWM4(IF,m,x0,x1,x2,x3) SHOWP(IF,m<<": ") SHOW4(IF,x0,x1,x2,x3)
#define SHOWM5(IF,m,x0,x1,x2,x3,x4) SHOWP(IF,m<<": ") SHOW5(IF,x,x0,x1,x2,x3,x4)
#define SHOWM6(IF,m,x0,x1,x2,x3,x4,x5) SHOWP(IF,m<<": ") SHOW6(IF,x0,x1,x2,x3,x4,x5)
#define SHOWM7(IF,m,x0,x1,x2,x3,x4,x5,x6) SHOWP(IF,m<<": ") SHOW7(IF,x0,x1,x2,x3,x4,x5,x6)

#endif
