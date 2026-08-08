#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
int main(int argc, char** argv){
    if(argc<4){fprintf(stderr,"usage: memread pid hexaddr nbytes\n");return 2;}
    pid_t pid=(pid_t)strtol(argv[1],0,10);
    uint64_t addr=strtoull(argv[2],0,16);
    long n=strtol(argv[3],0,10);
    char path[64]; snprintf(path,sizeof path,"/proc/%d/mem",pid);
    int fd=open(path,O_RDONLY); if(fd<0){perror("open");return 1;}
    char buf[8192]; long got=0;
    while(got<n){ long want=n-got; if(want>(long)sizeof buf) want=sizeof buf;
        long r=pread(fd,buf,want,(off_t)(addr+got)); if(r<=0){perror("pread");return 1;}
        fwrite(buf,1,r,stdout); got+=r; }
    return 0;
}
