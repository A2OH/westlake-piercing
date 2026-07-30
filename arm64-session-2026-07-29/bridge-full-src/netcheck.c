#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
int main(){
    int fd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in a; memset(&a,0,sizeof a);
    a.sin_family=AF_INET; a.sin_port=htons(8080);
    inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);
    if(connect(fd,(struct sockaddr*)&a,sizeof a)!=0){printf("CONNECT_FAIL to 127.0.0.1:8080\n");return 1;}
    const char* req="CONNECT cdn.trynoice.com:443 HTTP/1.1\r\nHost: cdn.trynoice.com:443\r\n\r\n";
    write(fd,req,strlen(req));
    char buf[512]; int n=read(fd,buf,sizeof buf-1);
    if(n<=0){printf("NO_RESPONSE from proxy\n");return 2;}
    buf[n]=0; printf("PROXY_RESP: %.*s\n", n>80?80:n, buf);
    return 0;
}
