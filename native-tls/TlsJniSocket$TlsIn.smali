.class final Lcom/android/internal/os/TlsJniSocket$TlsIn;
.super Ljava/io/InputStream;
.source "TlsJniSocket.java"


# annotations
.annotation system Ldalvik/annotation/EnclosingClass;
    value = Lcom/android/internal/os/TlsJniSocket;
.end annotation

.annotation system Ldalvik/annotation/InnerClass;
    accessFlags = 0x18
    name = "TlsIn"
.end annotation


# instance fields
.field final s:Lcom/android/internal/os/TlsJniSocket;


# direct methods
.method constructor <init>(Lcom/android/internal/os/TlsJniSocket;)V
    .registers 2

    .line 89
    invoke-direct {p0}, Ljava/io/InputStream;-><init>()V

    iput-object p1, p0, Lcom/android/internal/os/TlsJniSocket$TlsIn;->s:Lcom/android/internal/os/TlsJniSocket;

    return-void
.end method


# virtual methods
.method public read()I
    .registers 6
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/io/IOException;
        }
    .end annotation

    .line 90
    const/4 v0, 0x1

    new-array v1, v0, [B

    iget-object v2, p0, Lcom/android/internal/os/TlsJniSocket$TlsIn;->s:Lcom/android/internal/os/TlsJniSocket;

    iget-wide v2, v2, Lcom/android/internal/os/TlsJniSocket;->ssl:J

    const/4 v4, 0x0

    invoke-static {v2, v3, v1, v4, v0}, Lcom/android/internal/os/TlsJni;->sslRead(J[BII)I

    move-result v0

    if-gtz v0, :cond_10

    const/4 v0, -0x1

    goto :goto_14

    :cond_10
    aget-byte v0, v1, v4

    and-int/lit16 v0, v0, 0xff

    :goto_14
    return v0
.end method

.method public read([BII)I
    .registers 6
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/io/IOException;
        }
    .end annotation

    .line 91
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket$TlsIn;->s:Lcom/android/internal/os/TlsJniSocket;

    iget-wide v0, v0, Lcom/android/internal/os/TlsJniSocket;->ssl:J

    invoke-static {v0, v1, p1, p2, p3}, Lcom/android/internal/os/TlsJni;->sslRead(J[BII)I

    move-result p1

    if-gtz p1, :cond_b

    const/4 p1, -0x1

    :cond_b
    return p1
.end method
