.class final Lcom/android/internal/os/TlsJniSocket$TlsSession;
.super Ljava/lang/Object;
.source "TlsJniSocket.java"

# interfaces
.implements Ljavax/net/ssl/SSLSession;


# annotations
.annotation system Ldalvik/annotation/EnclosingClass;
    value = Lcom/android/internal/os/TlsJniSocket;
.end annotation

.annotation system Ldalvik/annotation/InnerClass;
    accessFlags = 0x18
    name = "TlsSession"
.end annotation


# instance fields
.field final s:Lcom/android/internal/os/TlsJniSocket;


# direct methods
.method constructor <init>(Lcom/android/internal/os/TlsJniSocket;)V
    .registers 2

    .line 99
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    iput-object p1, p0, Lcom/android/internal/os/TlsJniSocket$TlsSession;->s:Lcom/android/internal/os/TlsJniSocket;

    return-void
.end method


# virtual methods
.method public getApplicationBufferSize()I
    .registers 2

    .line 119
    const/16 v0, 0x4000

    return v0
.end method

.method public getCipherSuite()Ljava/lang/String;
    .registers 2

    .line 114
    const-string v0, "TLS_AES_128_GCM_SHA256"

    return-object v0
.end method

.method public getCreationTime()J
    .registers 3

    .line 102
    invoke-static {}, Ljava/lang/System;->currentTimeMillis()J

    move-result-wide v0

    return-wide v0
.end method

.method public getId()[B
    .registers 2

    .line 100
    const/4 v0, 0x0

    new-array v0, v0, [B

    return-object v0
.end method

.method public getLastAccessedTime()J
    .registers 3

    .line 103
    invoke-static {}, Ljava/lang/System;->currentTimeMillis()J

    move-result-wide v0

    return-wide v0
.end method

.method public getLocalCertificates()[Ljava/security/cert/Certificate;
    .registers 2

    .line 111
    const/4 v0, 0x0

    return-object v0
.end method

.method public getLocalPrincipal()Ljava/security/Principal;
    .registers 2

    .line 113
    const/4 v0, 0x0

    return-object v0
.end method

.method public getPacketBufferSize()I
    .registers 2

    .line 118
    const/16 v0, 0x4145

    return v0
.end method

.method public getPeerCertificates()[Ljava/security/cert/Certificate;
    .registers 3
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljavax/net/ssl/SSLPeerUnverifiedException;
        }
    .end annotation

    .line 110
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket$TlsSession;->s:Lcom/android/internal/os/TlsJniSocket;

    iget-object v0, v0, Lcom/android/internal/os/TlsJniSocket;->peer:[Ljava/security/cert/X509Certificate;

    if-eqz v0, :cond_b

    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket$TlsSession;->s:Lcom/android/internal/os/TlsJniSocket;

    iget-object v0, v0, Lcom/android/internal/os/TlsJniSocket;->peer:[Ljava/security/cert/X509Certificate;

    return-object v0

    :cond_b
    new-instance v0, Ljavax/net/ssl/SSLPeerUnverifiedException;

    const-string v1, "no peer cert"

    invoke-direct {v0, v1}, Ljavax/net/ssl/SSLPeerUnverifiedException;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method public getPeerHost()Ljava/lang/String;
    .registers 2

    .line 116
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket$TlsSession;->s:Lcom/android/internal/os/TlsJniSocket;

    iget-object v0, v0, Lcom/android/internal/os/TlsJniSocket;->host:Ljava/lang/String;

    return-object v0
.end method

.method public getPeerPort()I
    .registers 2

    .line 117
    const/4 v0, -0x1

    return v0
.end method

.method public getPeerPrincipal()Ljava/security/Principal;
    .registers 3
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljavax/net/ssl/SSLPeerUnverifiedException;
        }
    .end annotation

    .line 112
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket$TlsSession;->s:Lcom/android/internal/os/TlsJniSocket;

    iget-object v0, v0, Lcom/android/internal/os/TlsJniSocket;->peer:[Ljava/security/cert/X509Certificate;

    if-eqz v0, :cond_12

    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket$TlsSession;->s:Lcom/android/internal/os/TlsJniSocket;

    iget-object v0, v0, Lcom/android/internal/os/TlsJniSocket;->peer:[Ljava/security/cert/X509Certificate;

    const/4 v1, 0x0

    aget-object v0, v0, v1

    invoke-virtual {v0}, Ljava/security/cert/X509Certificate;->getSubjectDN()Ljava/security/Principal;

    move-result-object v0

    return-object v0

    :cond_12
    new-instance v0, Ljavax/net/ssl/SSLPeerUnverifiedException;

    const-string v1, "no peer"

    invoke-direct {v0, v1}, Ljavax/net/ssl/SSLPeerUnverifiedException;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method public getProtocol()Ljava/lang/String;
    .registers 2

    .line 115
    const-string v0, "TLSv1.2"

    return-object v0
.end method

.method public getSessionContext()Ljavax/net/ssl/SSLSessionContext;
    .registers 2

    .line 101
    const/4 v0, 0x0

    return-object v0
.end method

.method public getValue(Ljava/lang/String;)Ljava/lang/Object;
    .registers 2

    .line 107
    const/4 p1, 0x0

    return-object p1
.end method

.method public getValueNames()[Ljava/lang/String;
    .registers 2

    .line 109
    const/4 v0, 0x0

    new-array v0, v0, [Ljava/lang/String;

    return-object v0
.end method

.method public invalidate()V
    .registers 1

    .line 104
    return-void
.end method

.method public isValid()Z
    .registers 2

    .line 105
    const/4 v0, 0x1

    return v0
.end method

.method public putValue(Ljava/lang/String;Ljava/lang/Object;)V
    .registers 3

    .line 106
    return-void
.end method

.method public removeValue(Ljava/lang/String;)V
    .registers 2

    .line 108
    return-void
.end method
