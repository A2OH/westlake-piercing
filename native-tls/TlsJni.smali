.class public final Lcom/android/internal/os/TlsJni;
.super Ljava/lang/Object;
.source "TlsJni.java"


# direct methods
.method static constructor <clinit>()V
    .registers 1

    .line 3
    :try_start_0
    const-string v0, "/system/android/lib/libtlsjni.so"

    invoke-static {v0}, Ljava/lang/System;->load(Ljava/lang/String;)V
    :try_end_5
    .catchall {:try_start_0 .. :try_end_5} :catchall_6

    goto :goto_7

    :catchall_6
    move-exception v0

    :goto_7
    return-void
.end method

.method public constructor <init>()V
    .registers 1

    .line 2
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method public static native sslClose(J)V
.end method

.method public static native sslConnect(ILjava/lang/String;)J
.end method

.method public static native sslPeerCertDer(J)[B
.end method

.method public static native sslRead(J[BII)I
.end method

.method public static native sslWrite(J[BII)I
.end method
