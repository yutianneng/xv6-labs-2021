#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define BUFSIZE 1024
#define MAXLEN 100
int
main(int argc,char* argv[]){
    if(argc<2){
        fprintf(2,"Usage: xargs command\n");
        exit(1);
    }
    char *param[MAXARG];
    int index=0;
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"-n")==0){
            i++;
        }else{
            param[index]=(char*)malloc(strlen(argv[i]));
            strcpy(param[index++],argv[i]);
        }
    }
    char buf[MAXLEN],*p=buf;
    while(read(0,p++,1)==1);
    int l=0,r=0;
    //"\n"输入后是两个字符"\\" +"n"，而不是"\n"
    while(r<strlen(buf)){
        if(buf[r]=='\\'&&r<strlen(buf)-1&&buf[r+1]=='n'){
            buf[l]='\n';
            r+=2;
            l++;
        }else{
            buf[l++]=buf[r++];
        }
    }
    buf[l]=0;
    // fprintf(1,"buf: %s, len: %d\n",buf,strlen(buf));
    l=0;
    while(buf[l]=='"'){
        l++;
    }
    //最后面会有一个换行符
    r=strlen(buf)-1-1;
    while(buf[r]=='"'){
        r--;
    }
    for(int i=l;i<=r ;i++){
        buf[i-l]=buf[i];
    } 
    buf[r+1-l]='\n';
    buf[r+2-l]=0;
    // fprintf(1,"l: %d, r: %d, buf: %s, len: %d\n",l,r,buf,strlen(buf));

    l=0;r=0;
    while(r<strlen(buf)){
        if(buf[r]=='\n'){
            param[index]=(char*)malloc(r-l);
            memmove(param[index],buf+l,r-l);
            // fprintf(1,"index: %d, line: %s\n",index,param[index]);
            int pid=fork();
            if (pid==0){
                exec(param[0], param);
                exit(0);
            }else{
                wait(&pid);
            }
            r++;
            l=r;
        }else{
            r++;
        }
    }
    exit(0);
}
