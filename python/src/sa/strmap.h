#ifndef _STRMAP_H_
#define _STRMAP_H_

#ifdef __cplusplus
  extern "C" {
#else
    typedef struct StrMap StrMap; /* dummy type to stand in for class */
#endif

struct StrMap;

StrMap* stringmap_new();
void stringmap_delete(StrMap *vocab);
int stringmap_index(StrMap *vocab, char *s);
char* stringmap_word(StrMap *vocab, int i);

#ifdef __cplusplus
  }
#endif


#endif
