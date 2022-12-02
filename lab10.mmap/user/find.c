#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
int count = 0;
char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  return buf;
}

void loop_find(char* path, char* target)
{
    int fd;
    struct dirent de;
    struct stat st;
    char buf[512], *p;
    while((fd = open(path, 0)) < 0);
    while(fstat(fd, &st) < 0);
    

    switch(st.type){
  case T_FILE:
    close(fd);
    break;

  case T_DIR:
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0)
            continue;
        if(strcmp(de.name,".") == 0 || strcmp(de.name,"..") == 0) {
            continue;
        }
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if(strcmp(de.name, target) == 0) {
          printf("%s\n",buf);
        }
        loop_find(buf,target);
    }
    close(fd);
    break;
  }
}

int main(int argc, char *argv[])
{
    loop_find(argv[1],argv[2]);
    return 0;
}