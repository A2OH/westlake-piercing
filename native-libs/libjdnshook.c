/* Java-DNS hook: android.net Inet6AddressImpl.lookupHostByName -> JNI ->
 * bionic android_getaddrinfofornet(), which on this OHOS adapter has no netd
 * and EPERMs. libdnshook already makes plain getaddrinfo() work via direct UDP,
 * so delegate the *fornet variant to it. Opaque addrinfo (pointers only) keeps
 * this header-free. getaddrinfo resolves to libdnshook's hook (preloaded first). */
struct addrinfo;
extern int getaddrinfo(const char* node, const char* service,
                       const struct addrinfo* hints, struct addrinfo** res);
__attribute__((visibility("default")))
int android_getaddrinfofornet(const char* node, const char* service,
                              const struct addrinfo* hints, unsigned netid,
                              unsigned mark, struct addrinfo** res) {
    (void)netid; (void)mark;
    return getaddrinfo(node, service, hints, res);
}
