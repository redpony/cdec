#ifndef STRUTIL_H
#define STRUTIL_H

char *strstrsep(char **stringp, const char *delim);
char *strip(char *s);
char **split(char *s, const char *delim, int *pn);

#endif
