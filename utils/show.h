#ifndef UTILS__SHOW_H
#define UTILS__SHOW_H

#ifndef SHOWS
#include <iostream>
#endif
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

#ifndef SHOWS
#define SHOWS std::cerr
#endif

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

#endif
