#include "lastbreach.h"

char *read_entire_file(const char *path){
  FILE *f=fopen(path,"rb");
  if(!f) return NULL;
  fseek(f,0,SEEK_END);
  long n=ftell(f);
  fseek(f,0,SEEK_SET);
  if(n<0){ fclose(f); return NULL; }
  char *buf=(char*)xmalloc((size_t)n+1);
  size_t rd=fread(buf,1,(size_t)n,f);
  fclose(f);
  buf[rd]=0;
  return buf;
}
int file_exists(const char *path){
  FILE *f=fopen(path,"rb");
  if(!f) return 0;
  fclose(f);
  return 1;
}


