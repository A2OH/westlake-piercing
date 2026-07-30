#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
static int tryconnect(const char* host, const char* port){
    struct addrinfo hints, *res=0; memset(&hints,0,sizeof hints);
    hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM;
    int g=getaddrinfo(host,port,&hints,&res);
    if(g!=0){ printf("  DNS FAIL %s: %s\n", host, gai_strerror(g)); return -1; }
    char ip[64]="?";
    if(res->ai_family==AF_INET) inet_ntop(AF_INET,&((struct sockaddr_in*)res->ai_addr)->sin_addr,ip,sizeof ip);
    else inet_ntop(AF_INET6,&((struct sockaddr_in6*)res->ai_addr)->sin6_addr,ip,sizeof ip);
    printf("  DNS ok %s -> %s\n", host, ip);
    int fd=socket(res->ai_family,SOCK_STREAM,0);
    fcntl(fd,F_SETFL,O_NONBLOCK);
    connect(fd,res->ai_addr,res->ai_addrlen);
    fd_set w; FD_ZERO(&w); FD_SET(fd,&w);
    struct timeval tv={6,0};
    int s=select(fd+1,0,&w,0,&tv);
    if(s<=0){ printf("  TCP connect TIMEOUT/err to %s:%s (firewall?)\n",ip,port); close(fd); freeaddrinfo(res); return -2; }
    int err=0; socklen_t el=sizeof err; getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&el);
    if(err){ printf("  TCP connect FAIL to %s:%s err=%d %s\n",ip,port,err,strerror(err)); close(fd); freeaddrinfo(res); return -3; }
    printf("  TCP connect OK to %s:%s\n",ip,port);
    close(fd); freeaddrinfo(res); return 0;
}
int main(){
    printf("cdn.trynoice.com:443 (public sounds):\n"); tryconnect("cdn.trynoice.com","443");
    printf("api.trynoice.com:443 (account API):\n");    tryconnect("api.trynoice.com","443");
    printf("8.8.8.8:443 (raw internet):\n");            tryconnect("8.8.8.8","443");
    printf("google.com:443:\n");                        tryconnect("google.com","443");
    return 0;
}
