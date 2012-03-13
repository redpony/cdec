#include <string.h>
#include <stdlib.h>

/* Like strsep(3) except that the delimiter is a string, not a set of characters. 
*/
char *strstrsep(char **stringp, const char *delim) {
  char *match, *save;
  save = *stringp;
  if (*stringp == NULL)
    return NULL;
  match = strstr(*stringp, delim);
  if (match == NULL) {
    *stringp = NULL;
    return save;
  }
  *match = '\0';
  *stringp = match + strlen(delim);
  return save;
}

static char **words = NULL;
static int max_words;
char **split(char *s, const char *delim, int *pn) {
  int i;
  char *tok, *rest;

  if (words == NULL) {
    max_words = 10;
    words = malloc(max_words*sizeof(char *));
  }
  i = 0;
  rest = s;
  while ((tok = (delim ? strstrsep(&rest, delim) : strsep(&rest, " \t\n"))) != NULL) {
    if (!delim && !*tok) // empty token
      continue;
    while (i+1 >= max_words) {
      max_words *= 2;
      words = realloc(words, max_words*sizeof(char *));
    }
    words[i] = tok;
    i++;
  }
  words[i] = NULL;
  if (pn != NULL)
    *pn = i;
  return words;
}

inline int isspace(char c) {
  return (c == ' ' || c == '\t' || c == '\n');
}

char *strip(char *s) {
  int n;
  while (isspace(*s) && *s != '\0')
    s++;
  n = strlen(s);
  while (n > 0 && isspace(s[n-1])) {
    s[n-1] = '\0';
    n--;
  }
  return s;
}
