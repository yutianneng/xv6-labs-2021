#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

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
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  if (strlen(buf) == 0) return buf;
    for (int i = strlen(buf) - 1; i > 0; i--) {
        if (buf[i - 1] != ' ') {
            buf[i] = '\0';
            return buf;
        }
    }
    return buf;
  return buf;
}

void
find(char *path,char *file){
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    if(strcmp(file,fmtname(path))==0){
        fprintf(1,"%s\n",path);
    }
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    //遍历path下每个子目录
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      if(strcmp(de.name,".")==0 || strcmp(de.name,"..")==0)
        continue;
        
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }
      find(buf,file);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{

  if(argc != 3){
    fprintf(2, "Usage: find param err...\n");
    exit(1);
  }
  char *dir=argv[1];
  char *file=argv[2];
  find(dir,file);
  exit(0);
}
