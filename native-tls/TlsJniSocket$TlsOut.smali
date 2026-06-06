.class final Lcom/android/internal/os/TlsJniSocket$TlsOut;
.super Ljava/io/OutputStream;
.source "TlsJniSocket.java"


# annotations
.annotation system Ldalvik/annotation/EnclosingClass;
    value = Lcom/android/internal/os/TlsJniSocket;
.end annotation

.annotation system Ldalvik/annotation/InnerClass;
    accessFlags = 0x18
    name = "TlsOut"
.end annotation


# instance fields
.field final s:Lcom/android/internal/os/TlsJniSocket;


# direct methods
.method constructor <init>(Lcom/android/internal/os/TlsJniSocket;)V
    .registers 2

    .line 94
    invoke-direct {p0}, Ljava/io/OutputStream;-><init>()V

    iput-object p1, p0, Lcom/android/internal/os/TlsJniSocket$TlsOut;->s:Lcom/android/internal/os/TlsJniSocket;

    return-void
.end method


# virtual methods
.method public write(I)V
    .registers 7
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/io/IOException;
        }
    .end annotation

    .line 95
    iget-object v0, p0, Lcom/android/internal/os/TlsJniSocket$TlsOut;->s:Lcom/android/internal/os/TlsJniSocket;

    iget-wide v0, v0, Lcom/android/internal/os/TlsJniSocket;->ssl:J

    const/4 v2, 0x1

    new-array v3, v2, [B

    int-to-byte p1, p1

    const/4 v4, 0x0

    aput-byte p1, v3, v4

    invoke-static {v0, v1, v3, v4, v2}, Lcom/android/internal/os/TlsJni;->sslWrite(J[BII)I

    move-result p1

    if-lez p1, :cond_12

    return-void

    :cond_12
    new-instance p1, Ljava/io/IOException;

    const-string v0, "ssl write"

    invoke-direct {p1, v0}, Ljava/io/IOException;-><init>(Ljava/lang/String;)V

    throw p1
.end method

.method public write([BII)V
    .registers 9
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/io/IOException;
        }
    .end annotation

    .line 96
    const/4 v0, 0x0

    :goto_1
    if-ge v0, p3, :cond_1b

    iget-object v1, p0, Lcom/android/internal/os/TlsJniSocket$TlsOut;->s:Lcom/android/internal/os/TlsJniSocket;

    iget-wide v1, v1, Lcom/android/internal/os/TlsJniSocket;->ssl:J

    add-int v3, p2, v0

    sub-int v4, p3, v0

    invoke-static {v1, v2, p1, v3, v4}, Lcom/android/internal/os/TlsJni;->sslWrite(J[BII)I

    move-result v1

    if-lez v1, :cond_13

    add-int/2addr v0, v1

    goto :goto_1

    :cond_13
    new-instance p1, Ljava/io/IOException;

    const-string p2, "ssl write"

    invoke-direct {p1, p2}, Ljava/io/IOException;-><init>(Ljava/lang/String;)V

    throw p1

    :cond_1b
    return-void
.end method
