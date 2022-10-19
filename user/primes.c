#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define ReadFd 0
#define WriteFd 1

int prime(int fd);
int
main(int argc, char *argv[])
{
  if(argc>2)
    {
        fprintf(2,"usage: primes [number]");
        exit(1);
    }
    int n=35;
    if(argc==2)
    {
        int t=atoi(argv[1]);
        if(t<1)
        {
            fprintf(2,"parameter is invalid!");
            exit(1);
        }
        if(t>35)
            n=t;
    }
    int pp[2];
    pipe(pp);
    int pid=fork();
    if(pid>0){
      close(pp[ReadFd]);
      for (int i = 2; i <= n; i++){
        write(pp[WriteFd],&i,sizeof(int));
      }
      close(pp[WriteFd]);
      wait(&pid);
    }else{
      close(pp[WriteFd]);
      prime(pp[ReadFd]);
      close(pp[ReadFd]);
    }
    exit(0);
}
int prime(int fd){
    
    int first=2;
    int buf;
    if(read(fd,&buf,sizeof(int))<=0){
      exit(0);
    }
    first=buf;
    fprintf(0,"prime %d\n",first);
    int pp[2];
    pipe(pp);
    int pid=fork();
    if(pid>0){
      close(pp[ReadFd]);
      while (read(fd,&buf,sizeof(int))>0){
        if(buf%first!=0){
          write(pp[WriteFd],&buf,sizeof(int));
        }
      }
      close(pp[WriteFd]);
    }else{
      close(pp[WriteFd]);
      prime(pp[ReadFd]);
      close(pp[ReadFd]);
    }
    wait(&pid);
    exit(0);
}