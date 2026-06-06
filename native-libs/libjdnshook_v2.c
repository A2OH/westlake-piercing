/* Java-DNS hook v2: libcore Linux.android_getaddrinfo JNI may call any of the
 * bionic resolver variants. v1 only hooked android_getaddrinfofornet; this also
 * hooks android_getaddrinfofornetcontext (and plain android_getaddrinfo), all
 * delegated to getaddrinfo() which libdnshook resolves via direct UDP. Logs which
 * variant fires to /data/local/tmp/jdns.log. Header-free. */
extern int getaddrinfo(const char* node, const char* service, const void* hints, void** res);
extern int open(const char*, int, int);
extern long write(int, const void*, unsigned long);
extern int close(int);
static unsigned long sl(const char* s){unsigned long n=0;while(s&&s[n])n++;return n;}
static void lg(const char* t, const char* n){ int fd=open("/data/local/tmp/jdns.log",1089,438); if(fd>=0){ write(fd,t,sl(t)); write(fd," ",1); if(n) write(fd,n,sl(n)); write(fd,"\n",1); close(fd);} }

__attribute__((visibility("default")))
int android_getaddrinfofornet(const char* node, const char* service, const void* hints, unsigned netid, unsigned mark, void** res){
    lg("fornet", node); return getaddrinfo(node, service, hints, res);
}
__attribute__((visibility("default")))
int android_getaddrinfofornetcontext(const char* node, const char* service, const void* hints, const void* netctx, void** res){
    lg("fornetctx", node); return getaddrinfo(node, service, hints, res);
}
__attribute__((visibility("default")))
int android_getaddrinfo(const char* node, const char* service, const void* hints, void** res){
    lg("plain", node); return getaddrinfo(node, service, hints, res);
}
