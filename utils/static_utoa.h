#ifndef STATIC_UTOA_H
#define STATIC_UTOA_H

#include "threadlocal.h"
#include "utoa.h"

namespace {
THREADLOCAL char utoa_buf[utoa_bufsize]; // to put end of string character at buf[20]
}

inline char *static_utoa(unsigned n) {
  return utoa(utoa_buf+utoa_bufsizem1,n);
}

inline char *static_itoa(int n) {
  return itoa(utoa_buf+utoa_bufsizem1,n);
}

#ifdef ITOA_SAMPLE
# include <cstdio>
# include <sstream>
# include <iostream>
using namespace std;

int main(int argc,char *argv[]) {
  printf("d U d U d U\n");
  for (int i=1;i<argc;++i) {
    int n;
    unsigned un;
    sscanf(argv[i],"%d",&n);
    sscanf(argv[i],"%u",&un);
    printf("%d %u %s",n,un,static_itoa(n));
    printf(" %s %s %s\n",static_utoa(un),itos(n).c_str(),utos(un).c_str());
  }
  return 0;
}
#endif

#endif
