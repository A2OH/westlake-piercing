/* libtlsjni: minimal native TLS for the OHOS adapter, backed by the device's
 * libssl_openssl (BoringSSL/OpenSSL). Exposes JNI for com.android.internal.os.TlsJni.
 * dlopen/dlsym the OpenSSL TLS client API at runtime (no link-path coupling). */
#include <jni.h>
#include <stdint.h>
extern void* dlopen(const char*, int);
extern void* dlsym(void*, const char*);

static void *(*p_TLS_client_method)(void);
static void *(*p_SSL_CTX_new)(const void*);
static void *(*p_SSL_new)(void*);
static int  (*p_SSL_set_fd)(void*, int);
static long (*p_SSL_ctrl)(void*, int, long, void*);
static int  (*p_SSL_connect)(void*);
static int  (*p_SSL_read)(void*, void*, int);
static int  (*p_SSL_write)(void*, const void*, int);
static int  (*p_SSL_shutdown)(void*);
static void (*p_SSL_free)(void*);
static int  (*p_OPENSSL_init_ssl)(uint64_t, const void*);
static void* (*p_SSL_get1_peer_certificate)(void*);
static int  (*p_i2d_X509)(void*, unsigned char**);
static void (*p_X509_free)(void*);
static void* g_ctx = 0;
static int g_init = 0;

static int initlib(void) {
    if (g_init) return g_ctx != 0;
    g_init = 1;
    dlopen("/system/lib/platformsdk/libcrypto_openssl.z.so", 2);
    void* ssl = dlopen("/system/lib/platformsdk/libssl_openssl.z.so", 2);
    if (!ssl) ssl = dlopen("libssl_openssl.z.so", 2);
    if (!ssl) return 0;
    p_TLS_client_method = (void*(*)(void))dlsym(ssl, "TLS_client_method");
    p_SSL_CTX_new = (void*(*)(const void*))dlsym(ssl, "SSL_CTX_new");
    p_SSL_new = (void*(*)(void*))dlsym(ssl, "SSL_new");
    p_SSL_set_fd = (int(*)(void*,int))dlsym(ssl, "SSL_set_fd");
    p_SSL_ctrl = (long(*)(void*,int,long,void*))dlsym(ssl, "SSL_ctrl");
    p_SSL_connect = (int(*)(void*))dlsym(ssl, "SSL_connect");
    p_SSL_read = (int(*)(void*,void*,int))dlsym(ssl, "SSL_read");
    p_SSL_write = (int(*)(void*,const void*,int))dlsym(ssl, "SSL_write");
    p_SSL_shutdown = (int(*)(void*))dlsym(ssl, "SSL_shutdown");
    p_SSL_free = (void(*)(void*))dlsym(ssl, "SSL_free");
    p_OPENSSL_init_ssl = (int(*)(uint64_t,const void*))dlsym(ssl, "OPENSSL_init_ssl");
    p_SSL_get1_peer_certificate = (void*(*)(void*))dlsym(ssl, "SSL_get1_peer_certificate");
    if (!p_SSL_get1_peer_certificate) p_SSL_get1_peer_certificate = (void*(*)(void*))dlsym(ssl, "SSL_get_peer_certificate");
    void* cr = dlopen("/system/lib/platformsdk/libcrypto_openssl.z.so", 2);
    if (!cr) cr = dlopen("libcrypto_openssl.z.so", 2);
    if (cr) { p_i2d_X509 = (int(*)(void*,unsigned char**))dlsym(cr,"i2d_X509"); p_X509_free=(void(*)(void*))dlsym(cr,"X509_free"); }
    if (!p_TLS_client_method || !p_SSL_CTX_new || !p_SSL_new || !p_SSL_connect || !p_SSL_set_fd) return 0;
    if (p_OPENSSL_init_ssl) p_OPENSSL_init_ssl(0, 0);
    void* m = p_TLS_client_method();
    if (!m) return 0;
    g_ctx = p_SSL_CTX_new(m);  /* default verify = none for a client -> trust-all (test-grade) */
    return g_ctx != 0;
}

JNIEXPORT jlong JNICALL
Java_com_android_internal_os_TlsJni_sslConnect(JNIEnv* env, jclass c, jint fd, jstring host) {
    if (!initlib()) return 0;
    void* ssl = p_SSL_new(g_ctx);
    if (!ssl) return 0;
    p_SSL_set_fd(ssl, fd);
    if (host) {
        const char* h = (*env)->GetStringUTFChars(env, host, 0);
        if (h) p_SSL_ctrl(ssl, 55, 0, (void*)h);   /* SSL_set_tlsext_host_name (SNI) */
        int r = p_SSL_connect(ssl);
        if (h) (*env)->ReleaseStringUTFChars(env, host, h);
        if (r != 1) { p_SSL_free(ssl); return 0; }
    } else if (p_SSL_connect(ssl) != 1) { p_SSL_free(ssl); return 0; }
    return (jlong)(intptr_t)ssl;
}

JNIEXPORT jint JNICALL
Java_com_android_internal_os_TlsJni_sslRead(JNIEnv* env, jclass c, jlong sslp, jbyteArray buf, jint off, jint len) {
    void* ssl = (void*)(intptr_t)sslp; if (!ssl) return -1;
    jbyte* b = (*env)->GetByteArrayElements(env, buf, 0); if (!b) return -1;
    int n = p_SSL_read(ssl, (char*)b + off, len);
    (*env)->ReleaseByteArrayElements(env, buf, b, 0);
    return n;
}

JNIEXPORT jint JNICALL
Java_com_android_internal_os_TlsJni_sslWrite(JNIEnv* env, jclass c, jlong sslp, jbyteArray buf, jint off, jint len) {
    void* ssl = (void*)(intptr_t)sslp; if (!ssl) return -1;
    jbyte* b = (*env)->GetByteArrayElements(env, buf, 0); if (!b) return -1;
    int n = p_SSL_write(ssl, (char*)b + off, len);
    (*env)->ReleaseByteArrayElements(env, buf, b, 2);  /* JNI_ABORT */
    return n;
}

JNIEXPORT void JNICALL
Java_com_android_internal_os_TlsJni_sslClose(JNIEnv* env, jclass c, jlong sslp) {
    void* ssl = (void*)(intptr_t)sslp;
    if (ssl) { if (p_SSL_shutdown) p_SSL_shutdown(ssl); if (p_SSL_free) p_SSL_free(ssl); }
}

JNIEXPORT jbyteArray JNICALL
Java_com_android_internal_os_TlsJni_sslPeerCertDer(JNIEnv* env, jclass c, jlong sslp) {
    void* ssl = (void*)(intptr_t)sslp;
    if (!ssl || !p_SSL_get1_peer_certificate || !p_i2d_X509) return 0;
    void* x = p_SSL_get1_peer_certificate(ssl);
    if (!x) return 0;
    int len = p_i2d_X509(x, 0);
    if (len <= 0) { if (p_X509_free) p_X509_free(x); return 0; }
    jbyteArray arr = (*env)->NewByteArray(env, len);
    if (arr) {
        jbyte* b = (*env)->GetByteArrayElements(env, arr, 0);
        if (b) { unsigned char* p = (unsigned char*)b; p_i2d_X509(x, &p); (*env)->ReleaseByteArrayElements(env, arr, b, 0); }
    }
    if (p_X509_free) p_X509_free(x);
    return arr;
}
