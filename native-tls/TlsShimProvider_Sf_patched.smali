.class public final Lcom/android/internal/os/TlsShimProvider$Sf;
.super Ljavax/net/ssl/SSLSocketFactory;
.source "TlsShimProvider.java"


# annotations
.annotation system Ldalvik/annotation/EnclosingClass;
    value = Lcom/android/internal/os/TlsShimProvider;
.end annotation

.annotation system Ldalvik/annotation/InnerClass;
    accessFlags = 0x19
    name = "Sf"
.end annotation


# direct methods
.method public constructor <init>()V
    .registers 1

    .line 94
    invoke-direct {p0}, Ljavax/net/ssl/SSLSocketFactory;-><init>()V

    return-void
.end method


# virtual methods
.method public createSocket(Ljava/lang/String;I)Ljava/net/Socket;
    .registers 3

    .line 101
    new-instance p1, Ljava/lang/UnsupportedOperationException;

    const-string p2, "TlsShim: no real TLS connect"

    invoke-direct {p1, p2}, Ljava/lang/UnsupportedOperationException;-><init>(Ljava/lang/String;)V

    throw p1
.end method

.method public createSocket(Ljava/lang/String;ILjava/net/InetAddress;I)Ljava/net/Socket;
    .registers 5

    .line 104
    new-instance p1, Ljava/lang/UnsupportedOperationException;

    const-string p2, "TlsShim: no real TLS connect"

    invoke-direct {p1, p2}, Ljava/lang/UnsupportedOperationException;-><init>(Ljava/lang/String;)V

    throw p1
.end method

.method public createSocket(Ljava/net/InetAddress;I)Ljava/net/Socket;
    .registers 3

    .line 107
    new-instance p1, Ljava/lang/UnsupportedOperationException;

    const-string p2, "TlsShim: no real TLS connect"

    invoke-direct {p1, p2}, Ljava/lang/UnsupportedOperationException;-><init>(Ljava/lang/String;)V

    throw p1
.end method

.method public createSocket(Ljava/net/InetAddress;ILjava/net/InetAddress;I)Ljava/net/Socket;
    .registers 5

    .line 110
    new-instance p1, Ljava/lang/UnsupportedOperationException;

    const-string p2, "TlsShim: no real TLS connect"

    invoke-direct {p1, p2}, Ljava/lang/UnsupportedOperationException;-><init>(Ljava/lang/String;)V

    throw p1
.end method

.method public createSocket(Ljava/net/Socket;Ljava/lang/String;IZ)Ljava/net/Socket;
    .registers 6

    new-instance v0, Lcom/android/internal/os/TlsJniSocket;

    invoke-direct {v0, p1, p2, p3}, Lcom/android/internal/os/TlsJniSocket;-><init>(Ljava/net/Socket;Ljava/lang/String;I)V

    return-object v0
.end method

.method public getDefaultCipherSuites()[Ljava/lang/String;
    .registers 2

    .line 95
    const/4 v0, 0x0

    new-array v0, v0, [Ljava/lang/String;

    return-object v0
.end method

.method public getSupportedCipherSuites()[Ljava/lang/String;
    .registers 2

    .line 96
    const/4 v0, 0x0

    new-array v0, v0, [Ljava/lang/String;

    return-object v0
.end method
