#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define ReadFd 0
#define WriteFd 1

/**
 * pipe是一段内核buffer，包含一对fd，一个用来读，一个用来写
 */

int
main(int argc, char *argv[])
{
  if(argc != 1){
    fprintf(2, "Usage: sleep param too many...\n");
    exit(1);
  }
  int f2c[2];
  int c2f[2];
  pipe(f2c);
  pipe(c2f);

  char buf[64];
  //fork对于父进程返回子进程ID，对于子进程返回0
  int pid=fork();
  if(pid>0){
      close(c2f[WriteFd]);
      close(f2c[ReadFd]);
      int n;
      //子进程先发送ping，父进程接收
      while((n=read(c2f[ReadFd],buf,64))<=0);
      fprintf(0,"%d: received %s\n",getpid(),buf);
      strcpy(buf,"pong");
      //收到ping后，父进程发送pong
      n=0;
      while((n=write(f2c[WriteFd],buf,sizeof(buf)))<=0);
      close(f2c[WriteFd]);
      close(c2f[ReadFd]);
      wait(&pid);
      exit(0);
  }else{
      close(c2f[ReadFd]);
      close(f2c[WriteFd]);
      strcpy(buf,"ping");
      int n=0;
      while((n=write(c2f[WriteFd],buf,sizeof(buf)))<=0);
      n=0;
      while ((n=read(f2c[ReadFd],buf,64))<=0);
      fprintf(0,"%d: received %s\n",getpid(),buf);
      close(f2c[WriteFd]);
      close(c2f[ReadFd]);
      exit(0);
  }
}
