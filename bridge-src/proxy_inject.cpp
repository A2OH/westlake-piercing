// proxy_inject.cpp — optional HTTP(S) proxy injector for adapter apps.
//
// Call oh_set_proxy_props(env) at the end of the adapter bridge JNI_OnLoad.
// It is a NO-OP unless the gate file /data/local/tmp/oh_proxy exists with
// contents "host:port".  When present it sets the JVM system properties
// http(s).proxyHost/Port so that framework HttpURLConnection clients
// (e.g. ExoPlayer's DefaultHttpDataSource) tunnel through that proxy.
//
// Use case: the RK3568 board has no WAN.  Pair with:
//   host$  python3 diagnostics/proxy.py            # CONNECT forward proxy :8080
//   host$  hdc rport tcp:8080 tcp:8080             # device:8080 -> host:8080
//   device# echo -n 127.0.0.1:8080 > /data/local/tmp/oh_proxy
// The proxy resolves hostnames host-side, so the device needs no DNS.
// Because the bridge is preloaded in the appspawn-x zygote, the property is
// set once in the parent and inherited by every forked app.

#include <jni.h>
#include <stdio.h>

void oh_set_proxy_props(JNIEnv* env) {
    FILE* f = fopen("/data/local/tmp/oh_proxy", "r");
    if (!f) return;                              // gate off -> no-op
    char host[64] = {0}, port[16] = {0};
    int n = fscanf(f, "%63[^:]:%15s", host, port);
    fclose(f);
    if (n != 2) return;

    jclass sys = env->FindClass("java/lang/System");
    if (!sys) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
    jmethodID setProp = env->GetStaticMethodID(sys, "setProperty",
        "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    if (!setProp) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }

    const char* hkeys[] = {"https.proxyHost", "http.proxyHost"};
    const char* pkeys[] = {"https.proxyPort", "http.proxyPort"};
    jstring jhost = env->NewStringUTF(host);
    jstring jport = env->NewStringUTF(port);
    for (int i = 0; i < 2; i++) {
        jstring k = env->NewStringUTF(hkeys[i]);
        jobject r = env->CallStaticObjectMethod(sys, setProp, k, jhost);
        if (r) env->DeleteLocalRef(r);
        env->DeleteLocalRef(k);
        jstring pk = env->NewStringUTF(pkeys[i]);
        jobject r2 = env->CallStaticObjectMethod(sys, setProp, pk, jport);
        if (r2) env->DeleteLocalRef(r2);
        env->DeleteLocalRef(pk);
    }
    env->DeleteLocalRef(jhost); env->DeleteLocalRef(jport);
    if (env->ExceptionCheck()) env->ExceptionClear();
}
