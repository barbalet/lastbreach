#include "lastbreach.h"

void dief(const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  exit(1);
}
void *xmalloc(size_t n){ void *p=malloc(n); if(!p) dief("out of memory"); return p; }
void *xrealloc(void *p,size_t n){ void*q=realloc(p,n); if(!q) dief("out of memory"); return q; }
char *xstrdup(const char *s){ if(!s) return NULL; size_t n=strlen(s); char *p=(char*)xmalloc(n+1); memcpy(p,s,n+1); return p; }


