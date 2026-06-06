.class public final Lcom/android/internal/os/TlsJniSocket;
.super Ljavax/net/ssl/SSLSocket;
.source "TlsJniSocket.java"


# annotations
.annotation system Ldalvik/annotation/MemberClasses;
    value = {
        Lcom/android/internal/os/TlsJniSocket$TlsSession;,
        Lcom/android/internal/os/TlsJniSocket$TlsIn;,
        Lcom/android/internal/os/TlsJniSocket$TlsOut;
    }
.end annotation


# instance fields
.field final host:Ljava/lang/String;

.field private in:Ljava/io/InputStream;

.field private out:Ljava/io/OutputStream;

.field peer:[Ljava/security/cert/X509Certificate;

.field private final session:Lcom/android/internal/os/TlsJniSocket$TlsSession;

.field shook:Z

.field ssl:J

.field final under:Ljava/net/Socket;


# direct methods
.method public constructor <init>(Ljava/net/Socket;Ljava/lang/String;I)V
    .registers 4

    .line 21
    invoke-direct {p0}, Ljavax/net/ssl/SSLSocket;-><init>()V

    .line 19
    new-instance p3, Lcom/android/internal/os/TlsJniSocket$TlsSession;

    invoke-direct {p3, p0}, Lcom/android/internal/os/TlsJniSocket$TlsSession;-><init>(Lcom/android/internal/os/TlsJniSocket;)V

    iput-object p3, p0, Lcom/android/internal/os/TlsJniSocket;->session:Lcom/android/internal/os/TlsJniSocket$TlsSession;

    .line 21
    iput-object p1, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    iput-object p2, p0, Lcom/android/internal/os/TlsJniSocket;->host:Ljava/lang/String;

    return-void
.end method


# virtual methods
.method public addHandshakeCompletedListener(Ljavax/net/ssl/HandshakeCompletedListener;)V
    .registers 2

    .line 62
    return-void
.end method

.method public declared-synchronized close()V
    .registers 6
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/io/IOException;
        }
    .end annotation

    monitor-enter p0

    .line 53
    const-wide/16 v0, 0x0

    :try_start_3
    iget-wide v2, p0, Lcom/android/internal/os/TlsJniSocket;->ssl:J

    cmp-long v4, v2, v0

    if-eqz v4, :cond_e

    invoke-static {v2, v3}, Lcom/android/internal/os/TlsJni;->sslClose(J)V
    :try_end_c
    .catchall {:try_start_3 .. :try_end_c} :catchall_d

    goto :goto_e

    :catchall_d
    move-exception v2

    :cond_e
    :goto_e
    :try_start_e
    iput-wide v0, p0, Lcom/android/internal/os/TlsJniSocket;->ssl:J
    :try_end_10
    .catchall {:try_start_e .. :try_end_10} :catchall_19

    :try_start_10
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0}, Ljava/net/Socket;->close()V
    :try_end_15
    .catchall {:try_start_10 .. :try_end_15} :catchall_16

    goto :goto_17

    :catchall_16
    move-exception v0

    :goto_17
    monitor-exit p0

    return-void

    .line 53
    :catchall_19
    move-exception v0

    monitor-exit p0

    throw v0
.end method

.method fd()I
    .registers 7
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/io/IOException;
        }
    .end annotation

    .line 25
    :try_start_0
    const-class v0, Ljava/net/Socket;

    const-string v1, "impl"

    invoke-virtual {v0, v1}, Ljava/lang/Class;->getDeclaredField(Ljava/lang/String;)Ljava/lang/reflect/Field;

    move-result-object v0

    const/4 v1, 0x1

    invoke-virtual {v0, v1}, Ljava/lang/reflect/Field;->setAccessible(Z)V

    .line 26
    iget-object v2, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0, v2}, Ljava/lang/reflect/Field;->get(Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v0
    :try_end_12
    .catchall {:try_start_0 .. :try_end_12} :catchall_6a

    .line 27
    nop

    .line 28
    :try_start_13
    const-class v2, Ljava/net/SocketImpl;

    const-string v3, "fd"

    invoke-virtual {v2, v3}, Ljava/lang/Class;->getDeclaredField(Ljava/lang/String;)Ljava/lang/reflect/Field;

    move-result-object v2

    invoke-virtual {v2, v1}, Ljava/lang/reflect/Field;->setAccessible(Z)V

    invoke-virtual {v2, v0}, Ljava/lang/reflect/Field;->get(Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v2

    check-cast v2, Ljava/io/FileDescriptor;
    :try_end_24
    .catchall {:try_start_13 .. :try_end_24} :catchall_25

    goto :goto_27

    :catchall_25
    move-exception v2

    const/4 v2, 0x0

    .line 29
    :goto_27
    const/4 v3, 0x0

    if-nez v2, :cond_42

    :try_start_2a
    invoke-virtual {v0}, Ljava/lang/Object;->getClass()Ljava/lang/Class;

    move-result-object v2

    const-string v4, "getFileDescriptor"

    new-array v5, v3, [Ljava/lang/Class;

    invoke-virtual {v2, v4, v5}, Ljava/lang/Class;->getMethod(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;

    move-result-object v2

    invoke-virtual {v2, v1}, Ljava/lang/reflect/Method;->setAccessible(Z)V

    new-array v4, v3, [Ljava/lang/Object;

    invoke-virtual {v2, v0, v4}, Ljava/lang/reflect/Method;->invoke(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v0

    move-object v2, v0

    check-cast v2, Ljava/io/FileDescriptor;
    :try_end_42
    .catchall {:try_start_2a .. :try_end_42} :catchall_6a

    .line 30
    :cond_42
    :try_start_42
    const-class v0, Ljava/io/FileDescriptor;

    const-string v4, "descriptor"

    invoke-virtual {v0, v4}, Ljava/lang/Class;->getDeclaredField(Ljava/lang/String;)Ljava/lang/reflect/Field;

    move-result-object v0

    invoke-virtual {v0, v1}, Ljava/lang/reflect/Field;->setAccessible(Z)V

    invoke-virtual {v0, v2}, Ljava/lang/reflect/Field;->getInt(Ljava/lang/Object;)I

    move-result v0
    :try_end_51
    .catchall {:try_start_42 .. :try_end_51} :catchall_52

    return v0

    .line 31
    :catchall_52
    move-exception v0

    :try_start_53
    const-class v0, Ljava/io/FileDescriptor;

    const-string v1, "getInt$"

    new-array v4, v3, [Ljava/lang/Class;

    invoke-virtual {v0, v1, v4}, Ljava/lang/Class;->getMethod(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;

    move-result-object v0

    new-array v1, v3, [Ljava/lang/Object;

    invoke-virtual {v0, v2, v1}, Ljava/lang/reflect/Method;->invoke(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Ljava/lang/Integer;

    invoke-virtual {v0}, Ljava/lang/Integer;->intValue()I

    move-result v0
    :try_end_69
    .catchall {:try_start_53 .. :try_end_69} :catchall_6a

    return v0

    .line 32
    :catchall_6a
    move-exception v0

    new-instance v1, Ljava/io/IOException;

    new-instance v2, Ljava/lang/StringBuilder;

    invoke-direct {v2}, Ljava/lang/StringBuilder;-><init>()V

    const-string v3, "TlsJniSocket: cannot get fd: "

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v2

    invoke-virtual {v2, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/Object;)Ljava/lang/StringBuilder;

    move-result-object v0

    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v0

    invoke-direct {v1, v0}, Ljava/io/IOException;-><init>(Ljava/lang/String;)V

    throw v1
.end method

.method public getEnableSessionCreation()Z
    .registers 2

    .line 71
    const/4 v0, 0x1

    return v0
.end method

.method public getEnabledCipherSuites()[Ljava/lang/String;
    .registers 2

    .line 57
    invoke-virtual {p0}, Lcom/android/internal/os/TlsJniSocket;->getSupportedCipherSuites()[Ljava/lang/String;

    move-result-object v0

    return-object v0
.end method

.method public getEnabledProtocols()[Ljava/lang/String;
    .registers 2

    .line 60
    invoke-virtual {p0}, Lcom/android/internal/os/TlsJniSocket;->getSupportedProtocols()[Ljava/lang/String;

    move-result-object v0

    return-object v0
.end method

.method public getInetAddress()Ljava/net/InetAddress;
    .registers 2

    .line 73
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0}, Ljava/net/Socket;->getInetAddress()Ljava/net/InetAddress;

    move-result-object v0

    return-object v0
.end method

.method public getInputStream()Ljava/io/InputStream;
    .registers 2
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/io/IOException;
        }
    .end annotation

    .line 51
    iget-boolean v0, p0, Lcom/android/internal/os/TlsJniSocket;->shook:Z

    if-nez v0, :cond_7

    invoke-virtual {p0}, Lcom/android/internal/os/TlsJniSocket;->startHandshake()V

    :cond_7
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->in:Ljava/io/InputStream;

    if-nez v0, :cond_12

    new-instance v0, Lcom/android/internal/os/TlsJniSocket$TlsIn;

    invoke-direct {v0, p0}, Lcom/android/internal/os/TlsJniSocket$TlsIn;-><init>(Lcom/android/internal/os/TlsJniSocket;)V

    iput-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->in:Ljava/io/InputStream;

    :cond_12
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->in:Ljava/io/InputStream;

    return-object v0
.end method

.method public getLocalPort()I
    .registers 2

    .line 75
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0}, Ljava/net/Socket;->getLocalPort()I

    move-result v0

    return v0
.end method

.method public getLocalSocketAddress()Ljava/net/SocketAddress;
    .registers 2

    .line 77
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0}, Ljava/net/Socket;->getLocalSocketAddress()Ljava/net/SocketAddress;

    move-result-object v0

    return-object v0
.end method

.method public getNeedClientAuth()Z
    .registers 2

    .line 67
    const/4 v0, 0x0

    return v0
.end method

.method public getOutputStream()Ljava/io/OutputStream;
    .registers 2
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/io/IOException;
        }
    .end annotation

    .line 52
    iget-boolean v0, p0, Lcom/android/internal/os/TlsJniSocket;->shook:Z

    if-nez v0, :cond_7

    invoke-virtual {p0}, Lcom/android/internal/os/TlsJniSocket;->startHandshake()V

    :cond_7
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->out:Ljava/io/OutputStream;

    if-nez v0, :cond_12

    new-instance v0, Lcom/android/internal/os/TlsJniSocket$TlsOut;

    invoke-direct {v0, p0}, Lcom/android/internal/os/TlsJniSocket$TlsOut;-><init>(Lcom/android/internal/os/TlsJniSocket;)V

    iput-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->out:Ljava/io/OutputStream;

    :cond_12
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->out:Ljava/io/OutputStream;

    return-object v0
.end method

.method public getPort()I
    .registers 2

    .line 74
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0}, Ljava/net/Socket;->getPort()I

    move-result v0

    return v0
.end method

.method public getRemoteSocketAddress()Ljava/net/SocketAddress;
    .registers 2

    .line 76
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0}, Ljava/net/Socket;->getRemoteSocketAddress()Ljava/net/SocketAddress;

    move-result-object v0

    return-object v0
.end method

.method public getSession()Ljavax/net/ssl/SSLSession;
    .registers 2

    .line 54
    :try_start_0
    iget-boolean v0, p0, Lcom/android/internal/os/TlsJniSocket;->shook:Z

    if-nez v0, :cond_9

    invoke-virtual {p0}, Lcom/android/internal/os/TlsJniSocket;->startHandshake()V
    :try_end_7
    .catch Ljava/io/IOException; {:try_start_0 .. :try_end_7} :catch_8

    goto :goto_9

    :catch_8
    move-exception v0

    :cond_9
    :goto_9
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->session:Lcom/android/internal/os/TlsJniSocket$TlsSession;

    return-object v0
.end method

.method public getSoTimeout()I
    .registers 2
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/net/SocketException;
        }
    .end annotation

    .line 82
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0}, Ljava/net/Socket;->getSoTimeout()I

    move-result v0

    return v0
.end method

.method public getSupportedCipherSuites()[Ljava/lang/String;
    .registers 2

    .line 56
    const-string v0, "TLS_AES_128_GCM_SHA256"

    filled-new-array {v0}, [Ljava/lang/String;

    move-result-object v0

    return-object v0
.end method

.method public getSupportedProtocols()[Ljava/lang/String;
    .registers 3

    .line 59
    const-string v0, "TLSv1.2"

    const-string v1, "TLSv1.3"

    filled-new-array {v0, v1}, [Ljava/lang/String;

    move-result-object v0

    return-object v0
.end method

.method public getTcpNoDelay()Z
    .registers 2
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/net/SocketException;
        }
    .end annotation

    .line 84
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0}, Ljava/net/Socket;->getTcpNoDelay()Z

    move-result v0

    return v0
.end method

.method public getUseClientMode()Z
    .registers 2

    .line 65
    const/4 v0, 0x1

    return v0
.end method

.method public getWantClientAuth()Z
    .registers 2

    .line 69
    const/4 v0, 0x0

    return v0
.end method

.method public isBound()Z
    .registers 2

    .line 80
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0}, Ljava/net/Socket;->isBound()Z

    move-result v0

    return v0
.end method

.method public isClosed()Z
    .registers 2

    .line 79
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0}, Ljava/net/Socket;->isClosed()Z

    move-result v0

    return v0
.end method

.method public isConnected()Z
    .registers 2

    .line 78
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0}, Ljava/net/Socket;->isConnected()Z

    move-result v0

    return v0
.end method

.method public removeHandshakeCompletedListener(Ljavax/net/ssl/HandshakeCompletedListener;)V
    .registers 2

    .line 63
    return-void
.end method

.method public setEnableSessionCreation(Z)V
    .registers 2

    .line 70
    return-void
.end method

.method public setEnabledCipherSuites([Ljava/lang/String;)V
    .registers 2

    .line 58
    return-void
.end method

.method public setEnabledProtocols([Ljava/lang/String;)V
    .registers 2

    .line 61
    return-void
.end method

.method public setKeepAlive(Z)V
    .registers 3
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/net/SocketException;
        }
    .end annotation

    .line 85
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0, p1}, Ljava/net/Socket;->setKeepAlive(Z)V

    return-void
.end method

.method public setNeedClientAuth(Z)V
    .registers 2

    .line 66
    return-void
.end method

.method public setSoLinger(ZI)V
    .registers 4
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/net/SocketException;
        }
    .end annotation

    .line 86
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0, p1, p2}, Ljava/net/Socket;->setSoLinger(ZI)V

    return-void
.end method

.method public setSoTimeout(I)V
    .registers 3
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/net/SocketException;
        }
    .end annotation

    .line 81
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0, p1}, Ljava/net/Socket;->setSoTimeout(I)V

    return-void
.end method

.method public setTcpNoDelay(Z)V
    .registers 3
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/net/SocketException;
        }
    .end annotation

    .line 83
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->under:Ljava/net/Socket;

    invoke-virtual {v0, p1}, Ljava/net/Socket;->setTcpNoDelay(Z)V

    return-void
.end method

.method public setUseClientMode(Z)V
    .registers 2

    .line 64
    return-void
.end method

.method public setWantClientAuth(Z)V
    .registers 2

    .line 68
    return-void
.end method

.method public startHandshake()V
    .registers 5
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/io/IOException;
        }
    .end annotation

    .line 36
    iget-boolean v0, p0, Lcom/android/internal/os/TlsJniSocket;->shook:Z

    if-eqz v0, :cond_5

    return-void

    .line 37
    :cond_5
    invoke-virtual {p0}, Lcom/android/internal/os/TlsJniSocket;->fd()I

    move-result v0

    .line 38
    iget-object v1, p0, Lcom/android/internal/os/TlsJniSocket;->host:Ljava/lang/String;

    invoke-static {v0, v1}, Lcom/android/internal/os/TlsJni;->sslConnect(ILjava/lang/String;)J

    move-result-wide v0

    iput-wide v0, p0, Lcom/android/internal/os/TlsJniSocket;->ssl:J

    .line 39
    const-wide/16 v2, 0x0

    cmp-long v2, v0, v2

    if-eqz v2, :cond_3b

    .line 41
    :try_start_17
    invoke-static {v0, v1}, Lcom/android/internal/os/TlsJni;->sslPeerCertDer(J)[B

    move-result-object v0

    .line 42
    if-eqz v0, :cond_36

    .line 43
    const-string v1, "X.509"

    invoke-static {v1}, Ljava/security/cert/CertificateFactory;->getInstance(Ljava/lang/String;)Ljava/security/cert/CertificateFactory;

    move-result-object v1

    .line 44
    new-instance v2, Ljava/io/ByteArrayInputStream;

    invoke-direct {v2, v0}, Ljava/io/ByteArrayInputStream;-><init>([B)V

    invoke-virtual {v1, v2}, Ljava/security/cert/CertificateFactory;->generateCertificate(Ljava/io/InputStream;)Ljava/security/cert/Certificate;

    move-result-object v0

    check-cast v0, Ljava/security/cert/X509Certificate;

    .line 45
    filled-new-array {v0}, [Ljava/security/cert/X509Certificate;

    move-result-object v0

    iput-object v0, p0, Lcom/android/internal/os/TlsJniSocket;->peer:[Ljava/security/cert/X509Certificate;
    :try_end_34
    .catchall {:try_start_17 .. :try_end_34} :catchall_35

    goto :goto_36

    .line 47
    :catchall_35
    move-exception v0

    :cond_36
    :goto_36
    nop

    .line 48
    const/4 v0, 0x1

    iput-boolean v0, p0, Lcom/android/internal/os/TlsJniSocket;->shook:Z

    .line 49
    return-void

    .line 39
    :cond_3b
    new-instance v0, Ljavax/net/ssl/SSLHandshakeException;

    new-instance v1, Ljava/lang/StringBuilder;

    invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V

    const-string v2, "TlsJni: SSL_connect failed for "

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v1

    iget-object v2, p0, Lcom/android/internal/os/TlsJniSocket;->host:Ljava/lang/String;

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v1

    invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v1

    invoke-direct {v0, v1}, Ljavax/net/ssl/SSLHandshakeException;-><init>(Ljava/lang/String;)V

    throw v0
.end method
