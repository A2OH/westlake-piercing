.class public final Landroid/media/session/MediaSession;
.super Ljava/lang/Object;
.source "MediaSession.java"


# annotations
.annotation system Ldalvik/annotation/MemberClasses;
    value = {
        Landroid/media/session/MediaSession$CallbackStub;,
        Landroid/media/session/MediaSession$Token;,
        Landroid/media/session/MediaSession$Callback;,
        Landroid/media/session/MediaSession$CallbackMessageHandler;,
        Landroid/media/session/MediaSession$Command;,
        Landroid/media/session/MediaSession$QueueItem;,
        Landroid/media/session/MediaSession$SessionFlags;
    }
.end annotation


# static fields
.field public static final FLAG_EXCLUSIVE_GLOBAL_PRIORITY:I = 0x10000
    .annotation runtime Landroid/annotation/SystemApi;
        client = .enum Landroid/annotation/SystemApi$Client;->MODULE_LIBRARIES:Landroid/annotation/SystemApi$Client;
    .end annotation
.end field

.field public static final FLAG_HANDLES_MEDIA_BUTTONS:I = 0x1
    .annotation runtime Ljava/lang/Deprecated;
    .end annotation
.end field

.field public static final FLAG_HANDLES_TRANSPORT_CONTROLS:I = 0x2
    .annotation runtime Ljava/lang/Deprecated;
    .end annotation
.end field

.field public static final INVALID_PID:I = -0x1

.field public static final INVALID_UID:I = -0x1

.field static final TAG:Ljava/lang/String; = "MediaSession"


# instance fields
.field private mActive:Z

.field private final mBinder:Landroid/media/session/ISession;

.field private mCallback:Landroid/media/session/MediaSession$CallbackMessageHandler;

.field private final mCbStub:Landroid/media/session/MediaSession$CallbackStub;

.field private mContext:Landroid/content/Context;

.field private final mController:Landroid/media/session/MediaController;

.field private final mLock:Ljava/lang/Object;

.field private final mMaxBitmapSize:I

.field private mPlaybackState:Landroid/media/session/PlaybackState;

.field private final mSessionToken:Landroid/media/session/MediaSession$Token;

.field private mVolumeProvider:Landroid/media/VolumeProvider;


# direct methods
.method static bridge synthetic -$$Nest$fgetmLock(Landroid/media/session/MediaSession;)Ljava/lang/Object;
    .registers 1

    iget-object p0, p0, Landroid/media/session/MediaSession;->mLock:Ljava/lang/Object;

    return-object p0
.end method

.method static bridge synthetic -$$Nest$fgetmPlaybackState(Landroid/media/session/MediaSession;)Landroid/media/session/PlaybackState;
    .registers 1

    iget-object p0, p0, Landroid/media/session/MediaSession;->mPlaybackState:Landroid/media/session/PlaybackState;

    return-object p0
.end method

.method static bridge synthetic -$$Nest$fgetmVolumeProvider(Landroid/media/session/MediaSession;)Landroid/media/VolumeProvider;
    .registers 1

    iget-object p0, p0, Landroid/media/session/MediaSession;->mVolumeProvider:Landroid/media/VolumeProvider;

    return-object p0
.end method

.method public constructor <init>(Landroid/content/Context;Ljava/lang/String;)V
    .registers 4
    .param p1, "context"    # Landroid/content/Context;
    .param p2, "tag"    # Ljava/lang/String;

    .line 163
    const/4 v0, 0x0

    invoke-direct {p0, p1, p2, v0}, Landroid/media/session/MediaSession;-><init>(Landroid/content/Context;Ljava/lang/String;Landroid/os/Bundle;)V

    .line 164
    return-void
.end method

.method public constructor <init>(Landroid/content/Context;Ljava/lang/String;Landroid/os/Bundle;)V
    .registers 8
    .param p1, "context"    # Landroid/content/Context;
    .param p2, "tag"    # Ljava/lang/String;
    .param p3, "sessionInfo"    # Landroid/os/Bundle;

    .line 187
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 134
    new-instance v0, Ljava/lang/Object;

    invoke-direct {v0}, Ljava/lang/Object;-><init>()V

    iput-object v0, p0, Landroid/media/session/MediaSession;->mLock:Ljava/lang/Object;

    .line 149
    const/4 v0, 0x0

    iput-boolean v0, p0, Landroid/media/session/MediaSession;->mActive:Z

    .line 188
    if-eqz p1, :cond_96

    .line 191
    invoke-static {p2}, Landroid/text/TextUtils;->isEmpty(Ljava/lang/CharSequence;)Z

    move-result v0

    if-nez v0, :cond_8e

    .line 194
    invoke-static {p3}, Landroid/media/session/MediaSession;->hasCustomParcelable(Landroid/os/Bundle;)Z

    move-result v0

    if-nez v0, :cond_86

    .line 199
    iput-object p1, p0, Landroid/media/session/MediaSession;->mContext:Landroid/content/Context;

    .line 200
    invoke-virtual {p1}, Landroid/content/Context;->getResources()Landroid/content/res/Resources;

    move-result-object v0

    const v1, 0x10500c5

    invoke-virtual {v0, v1}, Landroid/content/res/Resources;->getDimensionPixelSize(I)I

    move-result v0

    iput v0, p0, Landroid/media/session/MediaSession;->mMaxBitmapSize:I

    .line 202
    new-instance v0, Landroid/media/session/MediaSession$CallbackStub;

    invoke-direct {v0, p0}, Landroid/media/session/MediaSession$CallbackStub;-><init>(Landroid/media/session/MediaSession;)V

    iput-object v0, p0, Landroid/media/session/MediaSession;->mCbStub:Landroid/media/session/MediaSession$CallbackStub;

    .line 203
    nop

    .line 204
    const-string v1, "media_session"

    invoke-virtual {p1, v1}, Landroid/content/Context;->getSystemService(Ljava/lang/String;)Ljava/lang/Object;

    move-result-object v1

    check-cast v1, Landroid/media/session/MediaSessionManager;

    if-nez v1, :cond_5f

    new-instance v0, Landroid/os/Binder;

    invoke-direct {v0}, Landroid/os/Binder;-><init>()V

    invoke-static {v0}, Landroid/media/session/ISession$Stub;->asInterface(Landroid/os/IBinder;)Landroid/media/session/ISession;

    move-result-object v0

    iput-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    new-instance v1, Landroid/os/Binder;

    invoke-direct {v1}, Landroid/os/Binder;-><init>()V

    invoke-static {v1}, Landroid/media/session/ISessionController$Stub;->asInterface(Landroid/os/IBinder;)Landroid/media/session/ISessionController;

    move-result-object v1

    new-instance v2, Landroid/media/session/MediaSession$Token;

    invoke-static {}, Landroid/os/Process;->myUid()I

    move-result v3

    invoke-direct {v2, v3, v1}, Landroid/media/session/MediaSession$Token;-><init>(ILandroid/media/session/ISessionController;)V

    iput-object v2, p0, Landroid/media/session/MediaSession;->mSessionToken:Landroid/media/session/MediaSession$Token;

    const/4 v0, 0x0

    iput-object v0, p0, Landroid/media/session/MediaSession;->mController:Landroid/media/session/MediaController;

    return-void

    .line 206
    .local v1, "manager":Landroid/media/session/MediaSessionManager;
    :cond_5f
    :try_start_5f
    invoke-virtual {v1, v0, p2, p3}, Landroid/media/session/MediaSessionManager;->createSession(Landroid/media/session/MediaSession$CallbackStub;Ljava/lang/String;Landroid/os/Bundle;)Landroid/media/session/ISession;

    move-result-object v0

    iput-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    .line 207
    new-instance v2, Landroid/media/session/MediaSession$Token;

    invoke-static {}, Landroid/os/Process;->myUid()I

    move-result v3

    invoke-interface {v0}, Landroid/media/session/ISession;->getController()Landroid/media/session/ISessionController;

    move-result-object v0

    invoke-direct {v2, v3, v0}, Landroid/media/session/MediaSession$Token;-><init>(ILandroid/media/session/ISessionController;)V

    iput-object v2, p0, Landroid/media/session/MediaSession;->mSessionToken:Landroid/media/session/MediaSession$Token;

    .line 208
    new-instance v0, Landroid/media/session/MediaController;

    invoke-direct {v0, p1, v2}, Landroid/media/session/MediaController;-><init>(Landroid/content/Context;Landroid/media/session/MediaSession$Token;)V

    iput-object v0, p0, Landroid/media/session/MediaSession;->mController:Landroid/media/session/MediaController;
    :try_end_7b
    .catch Landroid/os/RemoteException; {:try_start_5f .. :try_end_7b} :catch_7d

    .line 211
    nop

    .line 212
    return-void

    .line 209
    :catch_7d
    move-exception v0

    .line 210
    .local v0, "e":Landroid/os/RemoteException;
    new-instance v2, Ljava/lang/RuntimeException;

    const-string v3, "Remote error creating session."

    invoke-direct {v2, v3, v0}, Ljava/lang/RuntimeException;-><init>(Ljava/lang/String;Ljava/lang/Throwable;)V

    throw v2

    .line 195
    .end local v0    # "e":Landroid/os/RemoteException;
    .end local v1    # "manager":Landroid/media/session/MediaSessionManager;
    :cond_86
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "sessionInfo shouldn\'t contain any custom parcelables"

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v0

    .line 192
    :cond_8e
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "tag cannot be null or empty"

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v0

    .line 189
    :cond_96
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "context cannot be null."

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method static hasCustomParcelable(Landroid/os/Bundle;)Z
    .registers 6
    .param p0, "bundle"    # Landroid/os/Bundle;

    .line 649
    const/4 v0, 0x0

    if-nez p0, :cond_4

    .line 650
    return v0

    .line 654
    :cond_4
    const/4 v1, 0x0

    .line 656
    .local v1, "parcel":Landroid/os/Parcel;
    :try_start_5
    invoke-static {}, Landroid/os/Parcel;->obtain()Landroid/os/Parcel;

    move-result-object v2

    move-object v1, v2

    .line 657
    invoke-virtual {v1, p0}, Landroid/os/Parcel;->writeBundle(Landroid/os/Bundle;)V

    .line 658
    invoke-virtual {v1, v0}, Landroid/os/Parcel;->setDataPosition(I)V

    .line 659
    const/4 v2, 0x0

    invoke-virtual {v1, v2}, Landroid/os/Parcel;->readBundle(Ljava/lang/ClassLoader;)Landroid/os/Bundle;

    move-result-object v2

    .line 661
    .local v2, "out":Landroid/os/Bundle;
    invoke-virtual {v2}, Landroid/os/Bundle;->keySet()Ljava/util/Set;

    move-result-object v3

    invoke-interface {v3}, Ljava/util/Set;->iterator()Ljava/util/Iterator;

    move-result-object v3

    :goto_1d
    invoke-interface {v3}, Ljava/util/Iterator;->hasNext()Z

    move-result v4

    if-eqz v4, :cond_2e

    invoke-interface {v3}, Ljava/util/Iterator;->next()Ljava/lang/Object;

    move-result-object v4

    check-cast v4, Ljava/lang/String;

    .line 662
    .local v4, "key":Ljava/lang/String;
    invoke-virtual {v2, v4}, Landroid/os/Bundle;->get(Ljava/lang/String;)Ljava/lang/Object;
    :try_end_2c
    .catch Landroid/os/BadParcelableException; {:try_start_5 .. :try_end_2c} :catch_36
    .catchall {:try_start_5 .. :try_end_2c} :catchall_34

    .line 663
    nop

    .end local v4    # "key":Ljava/lang/String;
    goto :goto_1d

    .line 668
    .end local v2    # "out":Landroid/os/Bundle;
    :cond_2e
    if-eqz v1, :cond_33

    .line 669
    invoke-virtual {v1}, Landroid/os/Parcel;->recycle()V

    .line 672
    :cond_33
    return v0

    .line 668
    :catchall_34
    move-exception v0

    goto :goto_46

    .line 664
    :catch_36
    move-exception v0

    .line 665
    .local v0, "e":Landroid/os/BadParcelableException;
    :try_start_37
    const-string v2, "MediaSession"

    const-string v3, "Custom parcelable in bundle."

    invoke-static {v2, v3, v0}, Landroid/util/Log;->d(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I
    :try_end_3e
    .catchall {:try_start_37 .. :try_end_3e} :catchall_34

    .line 666
    nop

    .line 668
    if-eqz v1, :cond_44

    .line 669
    invoke-virtual {v1}, Landroid/os/Parcel;->recycle()V

    .line 666
    :cond_44
    const/4 v2, 0x1

    return v2

    .line 668
    .end local v0    # "e":Landroid/os/BadParcelableException;
    :goto_46
    if-eqz v1, :cond_4b

    .line 669
    invoke-virtual {v1}, Landroid/os/Parcel;->recycle()V

    .line 671
    :cond_4b
    throw v0
.end method


# virtual methods
.method dispatchAdjustVolume(Landroid/media/session/MediaSessionManager$RemoteUserInfo;I)V
    .registers 6
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "direction"    # I

    .line 762
    invoke-static {p2}, Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;

    move-result-object v0

    const/4 v1, 0x0

    const/16 v2, 0x16

    invoke-virtual {p0, p1, v2, v0, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 763
    return-void
.end method

.method dispatchCommand(Landroid/media/session/MediaSessionManager$RemoteUserInfo;Ljava/lang/String;Landroid/os/Bundle;Landroid/os/ResultReceiver;)V
    .registers 8
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "command"    # Ljava/lang/String;
    .param p3, "args"    # Landroid/os/Bundle;
    .param p4, "resultCb"    # Landroid/os/ResultReceiver;

    .line 771
    new-instance v0, Landroid/media/session/MediaSession$Command;

    invoke-direct {v0, p2, p3, p4}, Landroid/media/session/MediaSession$Command;-><init>(Ljava/lang/String;Landroid/os/Bundle;Landroid/os/ResultReceiver;)V

    .line 772
    .local v0, "cmd":Landroid/media/session/MediaSession$Command;
    const/4 v1, 0x1

    const/4 v2, 0x0

    invoke-virtual {p0, p1, v1, v0, v2}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 773
    return-void
.end method

.method dispatchCustomAction(Landroid/media/session/MediaSessionManager$RemoteUserInfo;Ljava/lang/String;Landroid/os/Bundle;)V
    .registers 5
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "action"    # Ljava/lang/String;
    .param p3, "args"    # Landroid/os/Bundle;

    .line 748
    const/16 v0, 0x15

    invoke-virtual {p0, p1, v0, p2, p3}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 749
    return-void
.end method

.method dispatchFastForward(Landroid/media/session/MediaSessionManager$RemoteUserInfo;)V
    .registers 4
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;

    .line 728
    const/16 v0, 0x10

    const/4 v1, 0x0

    invoke-virtual {p0, p1, v0, v1, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 729
    return-void
.end method

.method dispatchMediaButton(Landroid/media/session/MediaSessionManager$RemoteUserInfo;Landroid/content/Intent;)V
    .registers 5
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "mediaButtonIntent"    # Landroid/content/Intent;

    .line 752
    const/4 v0, 0x2

    const/4 v1, 0x0

    invoke-virtual {p0, p1, v0, p2, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 753
    return-void
.end method

.method dispatchMediaButtonDelayed(Landroid/media/session/MediaSessionManager$RemoteUserInfo;Landroid/content/Intent;J)V
    .registers 12
    .param p1, "info"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "mediaButtonIntent"    # Landroid/content/Intent;
    .param p3, "delay"    # J

    .line 757
    const/16 v2, 0x18

    const/4 v4, 0x0

    move-object v0, p0

    move-object v1, p1

    move-object v3, p2

    move-wide v5, p3

    invoke-virtual/range {v0 .. v6}, Landroid/media/session/MediaSession;->postToCallbackDelayed(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;J)V

    .line 759
    return-void
.end method

.method dispatchNext(Landroid/media/session/MediaSessionManager$RemoteUserInfo;)V
    .registers 4
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;

    .line 720
    const/16 v0, 0xe

    const/4 v1, 0x0

    invoke-virtual {p0, p1, v0, v1, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 721
    return-void
.end method

.method dispatchPause(Landroid/media/session/MediaSessionManager$RemoteUserInfo;)V
    .registers 4
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;

    .line 712
    const/16 v0, 0xc

    const/4 v1, 0x0

    invoke-virtual {p0, p1, v0, v1, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 713
    return-void
.end method

.method dispatchPlay(Landroid/media/session/MediaSessionManager$RemoteUserInfo;)V
    .registers 4
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;

    .line 692
    const/4 v0, 0x7

    const/4 v1, 0x0

    invoke-virtual {p0, p1, v0, v1, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 693
    return-void
.end method

.method dispatchPlayFromMediaId(Landroid/media/session/MediaSessionManager$RemoteUserInfo;Ljava/lang/String;Landroid/os/Bundle;)V
    .registers 5
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "mediaId"    # Ljava/lang/String;
    .param p3, "extras"    # Landroid/os/Bundle;

    .line 696
    const/16 v0, 0x8

    invoke-virtual {p0, p1, v0, p2, p3}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 697
    return-void
.end method

.method dispatchPlayFromSearch(Landroid/media/session/MediaSessionManager$RemoteUserInfo;Ljava/lang/String;Landroid/os/Bundle;)V
    .registers 5
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "query"    # Ljava/lang/String;
    .param p3, "extras"    # Landroid/os/Bundle;

    .line 700
    const/16 v0, 0x9

    invoke-virtual {p0, p1, v0, p2, p3}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 701
    return-void
.end method

.method dispatchPlayFromUri(Landroid/media/session/MediaSessionManager$RemoteUserInfo;Landroid/net/Uri;Landroid/os/Bundle;)V
    .registers 5
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "uri"    # Landroid/net/Uri;
    .param p3, "extras"    # Landroid/os/Bundle;

    .line 704
    const/16 v0, 0xa

    invoke-virtual {p0, p1, v0, p2, p3}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 705
    return-void
.end method

.method dispatchPrepare(Landroid/media/session/MediaSessionManager$RemoteUserInfo;)V
    .registers 4
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;

    .line 676
    const/4 v0, 0x3

    const/4 v1, 0x0

    invoke-virtual {p0, p1, v0, v1, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 677
    return-void
.end method

.method dispatchPrepareFromMediaId(Landroid/media/session/MediaSessionManager$RemoteUserInfo;Ljava/lang/String;Landroid/os/Bundle;)V
    .registers 5
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "mediaId"    # Ljava/lang/String;
    .param p3, "extras"    # Landroid/os/Bundle;

    .line 680
    const/4 v0, 0x4

    invoke-virtual {p0, p1, v0, p2, p3}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 681
    return-void
.end method

.method dispatchPrepareFromSearch(Landroid/media/session/MediaSessionManager$RemoteUserInfo;Ljava/lang/String;Landroid/os/Bundle;)V
    .registers 5
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "query"    # Ljava/lang/String;
    .param p3, "extras"    # Landroid/os/Bundle;

    .line 684
    const/4 v0, 0x5

    invoke-virtual {p0, p1, v0, p2, p3}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 685
    return-void
.end method

.method dispatchPrepareFromUri(Landroid/media/session/MediaSessionManager$RemoteUserInfo;Landroid/net/Uri;Landroid/os/Bundle;)V
    .registers 5
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "uri"    # Landroid/net/Uri;
    .param p3, "extras"    # Landroid/os/Bundle;

    .line 688
    const/4 v0, 0x6

    invoke-virtual {p0, p1, v0, p2, p3}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 689
    return-void
.end method

.method dispatchPrevious(Landroid/media/session/MediaSessionManager$RemoteUserInfo;)V
    .registers 4
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;

    .line 724
    const/16 v0, 0xf

    const/4 v1, 0x0

    invoke-virtual {p0, p1, v0, v1, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 725
    return-void
.end method

.method dispatchRate(Landroid/media/session/MediaSessionManager$RemoteUserInfo;Landroid/media/Rating;)V
    .registers 5
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "rating"    # Landroid/media/Rating;

    .line 740
    const/16 v0, 0x13

    const/4 v1, 0x0

    invoke-virtual {p0, p1, v0, p2, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 741
    return-void
.end method

.method dispatchRewind(Landroid/media/session/MediaSessionManager$RemoteUserInfo;)V
    .registers 4
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;

    .line 732
    const/16 v0, 0x11

    const/4 v1, 0x0

    invoke-virtual {p0, p1, v0, v1, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 733
    return-void
.end method

.method dispatchSeekTo(Landroid/media/session/MediaSessionManager$RemoteUserInfo;J)V
    .registers 7
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "pos"    # J

    .line 736
    invoke-static {p2, p3}, Ljava/lang/Long;->valueOf(J)Ljava/lang/Long;

    move-result-object v0

    const/4 v1, 0x0

    const/16 v2, 0x12

    invoke-virtual {p0, p1, v2, v0, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 737
    return-void
.end method

.method dispatchSetPlaybackSpeed(Landroid/media/session/MediaSessionManager$RemoteUserInfo;F)V
    .registers 6
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "speed"    # F

    .line 744
    invoke-static {p2}, Ljava/lang/Float;->valueOf(F)Ljava/lang/Float;

    move-result-object v0

    const/4 v1, 0x0

    const/16 v2, 0x14

    invoke-virtual {p0, p1, v2, v0, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 745
    return-void
.end method

.method dispatchSetVolumeTo(Landroid/media/session/MediaSessionManager$RemoteUserInfo;I)V
    .registers 6
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "volume"    # I

    .line 766
    invoke-static {p2}, Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;

    move-result-object v0

    const/4 v1, 0x0

    const/16 v2, 0x17

    invoke-virtual {p0, p1, v2, v0, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 767
    return-void
.end method

.method dispatchSkipToItem(Landroid/media/session/MediaSessionManager$RemoteUserInfo;J)V
    .registers 7
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "id"    # J

    .line 708
    invoke-static {p2, p3}, Ljava/lang/Long;->valueOf(J)Ljava/lang/Long;

    move-result-object v0

    const/4 v1, 0x0

    const/16 v2, 0xb

    invoke-virtual {p0, p1, v2, v0, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 709
    return-void
.end method

.method dispatchStop(Landroid/media/session/MediaSessionManager$RemoteUserInfo;)V
    .registers 4
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;

    .line 716
    const/16 v0, 0xd

    const/4 v1, 0x0

    invoke-virtual {p0, p1, v0, v1, v1}, Landroid/media/session/MediaSession;->postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V

    .line 717
    return-void
.end method

.method public getCallingPackage()Ljava/lang/String;
    .registers 2

    .line 639
    iget-object v0, p0, Landroid/media/session/MediaSession;->mCallback:Landroid/media/session/MediaSession$CallbackMessageHandler;

    if-eqz v0, :cond_15

    invoke-static {v0}, Landroid/media/session/MediaSession$CallbackMessageHandler;->-$$Nest$fgetmCurrentControllerInfo(Landroid/media/session/MediaSession$CallbackMessageHandler;)Landroid/media/session/MediaSessionManager$RemoteUserInfo;

    move-result-object v0

    if-eqz v0, :cond_15

    .line 640
    iget-object v0, p0, Landroid/media/session/MediaSession;->mCallback:Landroid/media/session/MediaSession$CallbackMessageHandler;

    invoke-static {v0}, Landroid/media/session/MediaSession$CallbackMessageHandler;->-$$Nest$fgetmCurrentControllerInfo(Landroid/media/session/MediaSession$CallbackMessageHandler;)Landroid/media/session/MediaSessionManager$RemoteUserInfo;

    move-result-object v0

    invoke-virtual {v0}, Landroid/media/session/MediaSessionManager$RemoteUserInfo;->getPackageName()Ljava/lang/String;

    move-result-object v0

    return-object v0

    .line 642
    :cond_15
    const/4 v0, 0x0

    return-object v0
.end method

.method public getController()Landroid/media/session/MediaController;
    .registers 2

    .line 471
    iget-object v0, p0, Landroid/media/session/MediaSession;->mController:Landroid/media/session/MediaController;

    return-object v0
.end method

.method public final getCurrentControllerInfo()Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .registers 3

    .line 603
    iget-object v0, p0, Landroid/media/session/MediaSession;->mCallback:Landroid/media/session/MediaSession$CallbackMessageHandler;

    if-eqz v0, :cond_11

    invoke-static {v0}, Landroid/media/session/MediaSession$CallbackMessageHandler;->-$$Nest$fgetmCurrentControllerInfo(Landroid/media/session/MediaSession$CallbackMessageHandler;)Landroid/media/session/MediaSessionManager$RemoteUserInfo;

    move-result-object v0

    if-eqz v0, :cond_11

    .line 607
    iget-object v0, p0, Landroid/media/session/MediaSession;->mCallback:Landroid/media/session/MediaSession$CallbackMessageHandler;

    invoke-static {v0}, Landroid/media/session/MediaSession$CallbackMessageHandler;->-$$Nest$fgetmCurrentControllerInfo(Landroid/media/session/MediaSession$CallbackMessageHandler;)Landroid/media/session/MediaSessionManager$RemoteUserInfo;

    move-result-object v0

    return-object v0

    .line 604
    :cond_11
    new-instance v0, Ljava/lang/IllegalStateException;

    const-string v1, "This should be called inside of MediaSession.Callback methods"

    invoke-direct {v0, v1}, Ljava/lang/IllegalStateException;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method public getSessionToken()Landroid/media/session/MediaSession$Token;
    .registers 2

    .line 461
    iget-object v0, p0, Landroid/media/session/MediaSession;->mSessionToken:Landroid/media/session/MediaSession$Token;

    return-object v0
.end method

.method public isActive()Z
    .registers 2

    .line 417
    iget-boolean v0, p0, Landroid/media/session/MediaSession;->mActive:Z

    return v0
.end method

.method public notifyRemoteVolumeChanged(Landroid/media/VolumeProvider;)V
    .registers 5
    .param p1, "provider"    # Landroid/media/VolumeProvider;

    .line 617
    iget-object v0, p0, Landroid/media/session/MediaSession;->mLock:Ljava/lang/Object;

    monitor-enter v0

    .line 618
    if-eqz p1, :cond_1e

    :try_start_5
    iget-object v1, p0, Landroid/media/session/MediaSession;->mVolumeProvider:Landroid/media/VolumeProvider;

    if-eq p1, v1, :cond_a

    goto :goto_1e

    .line 622
    :cond_a
    monitor-exit v0
    :try_end_b
    .catchall {:try_start_5 .. :try_end_b} :catchall_27

    .line 624
    :try_start_b
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-virtual {p1}, Landroid/media/VolumeProvider;->getCurrentVolume()I

    move-result v1

    invoke-interface {v0, v1}, Landroid/media/session/ISession;->setCurrentVolume(I)V
    :try_end_14
    .catch Landroid/os/RemoteException; {:try_start_b .. :try_end_14} :catch_15

    .line 627
    goto :goto_1d

    .line 625
    :catch_15
    move-exception v0

    .line 626
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "MediaSession"

    const-string v2, "Error in notifyVolumeChanged"

    invoke-static {v1, v2, v0}, Landroid/util/Log;->e(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 628
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_1d
    return-void

    .line 619
    :cond_1e
    :goto_1e
    :try_start_1e
    const-string v1, "MediaSession"

    const-string v2, "Received update from stale volume provider"

    invoke-static {v1, v2}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;)I

    .line 620
    monitor-exit v0

    return-void

    .line 622
    :catchall_27
    move-exception v1

    monitor-exit v0
    :try_end_29
    .catchall {:try_start_1e .. :try_end_29} :catchall_27

    throw v1
.end method

.method postToCallback(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;)V
    .registers 12
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "what"    # I
    .param p3, "obj"    # Ljava/lang/Object;
    .param p4, "data"    # Landroid/os/Bundle;

    .line 776
    const-wide/16 v5, 0x0

    move-object v0, p0

    move-object v1, p1

    move v2, p2

    move-object v3, p3

    move-object v4, p4

    invoke-virtual/range {v0 .. v6}, Landroid/media/session/MediaSession;->postToCallbackDelayed(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;J)V

    .line 777
    return-void
.end method

.method postToCallbackDelayed(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;J)V
    .registers 15
    .param p1, "caller"    # Landroid/media/session/MediaSessionManager$RemoteUserInfo;
    .param p2, "what"    # I
    .param p3, "obj"    # Ljava/lang/Object;
    .param p4, "data"    # Landroid/os/Bundle;
    .param p5, "delay"    # J

    .line 781
    iget-object v0, p0, Landroid/media/session/MediaSession;->mLock:Ljava/lang/Object;

    monitor-enter v0

    .line 782
    :try_start_3
    iget-object v1, p0, Landroid/media/session/MediaSession;->mCallback:Landroid/media/session/MediaSession$CallbackMessageHandler;

    if-eqz v1, :cond_f

    .line 783
    move-object v2, p1

    move v3, p2

    move-object v4, p3

    move-object v5, p4

    move-wide v6, p5

    invoke-virtual/range {v1 .. v7}, Landroid/media/session/MediaSession$CallbackMessageHandler;->post(Landroid/media/session/MediaSessionManager$RemoteUserInfo;ILjava/lang/Object;Landroid/os/Bundle;J)V

    .line 785
    :cond_f
    monitor-exit v0

    .line 786
    return-void

    .line 785
    :catchall_11
    move-exception v1

    monitor-exit v0
    :try_end_13
    .catchall {:try_start_3 .. :try_end_13} :catchall_11

    throw v1
.end method

.method public release()V
    .registers 4

    .line 446
    :try_start_0
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0}, Landroid/media/session/ISession;->destroySession()V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_6

    .line 449
    goto :goto_e

    .line 447
    :catch_6
    move-exception v0

    .line 448
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "MediaSession"

    const-string v2, "Error releasing session: "

    invoke-static {v1, v2, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 450
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_e
    return-void
.end method

.method public sendSessionEvent(Ljava/lang/String;Landroid/os/Bundle;)V
    .registers 6
    .param p1, "event"    # Ljava/lang/String;
    .param p2, "extras"    # Landroid/os/Bundle;

    .line 429
    invoke-static {p1}, Landroid/text/TextUtils;->isEmpty(Ljava/lang/CharSequence;)Z

    move-result v0

    if-nez v0, :cond_15

    .line 433
    :try_start_6
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0, p1, p2}, Landroid/media/session/ISession;->sendEvent(Ljava/lang/String;Landroid/os/Bundle;)V
    :try_end_b
    .catch Landroid/os/RemoteException; {:try_start_6 .. :try_end_b} :catch_c

    .line 436
    goto :goto_14

    .line 434
    :catch_c
    move-exception v0

    .line 435
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "MediaSession"

    const-string v2, "Error sending event"

    invoke-static {v1, v2, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 437
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_14
    return-void

    .line 430
    :cond_15
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "event cannot be null or empty"

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method public setActive(Z)V
    .registers 5
    .param p1, "active"    # Z

    .line 400
    iget-boolean v0, p0, Landroid/media/session/MediaSession;->mActive:Z

    if-ne v0, p1, :cond_5

    .line 401
    return-void

    .line 404
    :cond_5
    :try_start_5
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0, p1}, Landroid/media/session/ISession;->setActive(Z)V

    .line 405
    iput-boolean p1, p0, Landroid/media/session/MediaSession;->mActive:Z
    :try_end_c
    .catch Landroid/os/RemoteException; {:try_start_5 .. :try_end_c} :catch_d

    .line 408
    goto :goto_15

    .line 406
    :catch_d
    move-exception v0

    .line 407
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "MediaSession"

    const-string v2, "Failure in setActive."

    invoke-static {v1, v2, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 409
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_15
    return-void
.end method

.method public setCallback(Landroid/media/session/MediaSession$Callback;)V
    .registers 3
    .param p1, "callback"    # Landroid/media/session/MediaSession$Callback;

    .line 224
    const/4 v0, 0x0

    invoke-virtual {p0, p1, v0}, Landroid/media/session/MediaSession;->setCallback(Landroid/media/session/MediaSession$Callback;Landroid/os/Handler;)V

    .line 225
    return-void
.end method

.method public setCallback(Landroid/media/session/MediaSession$Callback;Landroid/os/Handler;)V
    .registers 6
    .param p1, "callback"    # Landroid/media/session/MediaSession$Callback;
    .param p2, "handler"    # Landroid/os/Handler;

    .line 237
    iget-object v0, p0, Landroid/media/session/MediaSession;->mLock:Ljava/lang/Object;

    monitor-enter v0

    .line 238
    :try_start_3
    iget-object v1, p0, Landroid/media/session/MediaSession;->mCallback:Landroid/media/session/MediaSession$CallbackMessageHandler;

    const/4 v2, 0x0

    if-eqz v1, :cond_14

    .line 240
    invoke-static {v1}, Landroid/media/session/MediaSession$CallbackMessageHandler;->-$$Nest$fgetmCallback(Landroid/media/session/MediaSession$CallbackMessageHandler;)Landroid/media/session/MediaSession$Callback;

    move-result-object v1

    invoke-static {v1, v2}, Landroid/media/session/MediaSession$Callback;->-$$Nest$fputmSession(Landroid/media/session/MediaSession$Callback;Landroid/media/session/MediaSession;)V

    .line 241
    iget-object v1, p0, Landroid/media/session/MediaSession;->mCallback:Landroid/media/session/MediaSession$CallbackMessageHandler;

    invoke-virtual {v1, v2}, Landroid/media/session/MediaSession$CallbackMessageHandler;->removeCallbacksAndMessages(Ljava/lang/Object;)V

    .line 243
    :cond_14
    if-nez p1, :cond_1a

    .line 244
    iput-object v2, p0, Landroid/media/session/MediaSession;->mCallback:Landroid/media/session/MediaSession$CallbackMessageHandler;

    .line 245
    monitor-exit v0

    return-void

    .line 247
    :cond_1a
    if-eqz p2, :cond_21

    invoke-virtual {p2}, Landroid/os/Handler;->getLooper()Landroid/os/Looper;

    move-result-object v1

    goto :goto_25

    :cond_21
    invoke-static {}, Landroid/os/Looper;->myLooper()Landroid/os/Looper;

    move-result-object v1

    .line 248
    .local v1, "looper":Landroid/os/Looper;
    :goto_25
    invoke-static {p1, p0}, Landroid/media/session/MediaSession$Callback;->-$$Nest$fputmSession(Landroid/media/session/MediaSession$Callback;Landroid/media/session/MediaSession;)V

    .line 249
    new-instance v2, Landroid/media/session/MediaSession$CallbackMessageHandler;

    invoke-direct {v2, p0, v1, p1}, Landroid/media/session/MediaSession$CallbackMessageHandler;-><init>(Landroid/media/session/MediaSession;Landroid/os/Looper;Landroid/media/session/MediaSession$Callback;)V

    .line 250
    .local v2, "msgHandler":Landroid/media/session/MediaSession$CallbackMessageHandler;
    iput-object v2, p0, Landroid/media/session/MediaSession;->mCallback:Landroid/media/session/MediaSession$CallbackMessageHandler;

    .line 251
    .end local v1    # "looper":Landroid/os/Looper;
    .end local v2    # "msgHandler":Landroid/media/session/MediaSession$CallbackMessageHandler;
    monitor-exit v0

    .line 252
    return-void

    .line 251
    :catchall_31
    move-exception v1

    monitor-exit v0
    :try_end_33
    .catchall {:try_start_3 .. :try_end_33} :catchall_31

    throw v1
.end method

.method public setExtras(Landroid/os/Bundle;)V
    .registers 4
    .param p1, "extras"    # Landroid/os/Bundle;

    .line 588
    :try_start_0
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0, p1}, Landroid/media/session/ISession;->setExtras(Landroid/os/Bundle;)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_6

    .line 591
    goto :goto_c

    .line 589
    :catch_6
    move-exception v0

    .line 590
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "Dead object in setExtras."

    invoke-static {v1, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 592
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_c
    return-void
.end method

.method public setFlags(I)V
    .registers 5
    .param p1, "flags"    # I

    .line 329
    :try_start_0
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0, p1}, Landroid/media/session/ISession;->setFlags(I)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_6

    .line 332
    goto :goto_e

    .line 330
    :catch_6
    move-exception v0

    .line 331
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "MediaSession"

    const-string v2, "Failure in setFlags."

    invoke-static {v1, v2, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 333
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_e
    return-void
.end method

.method public setMediaButtonBroadcastReceiver(Landroid/content/ComponentName;)V
    .registers 5
    .param p1, "broadcastReceiver"    # Landroid/content/ComponentName;

    .line 309
    if-eqz p1, :cond_1b

    .line 310
    :try_start_2
    invoke-virtual {p1}, Landroid/content/ComponentName;->getPackageName()Ljava/lang/String;

    move-result-object v0

    iget-object v1, p0, Landroid/media/session/MediaSession;->mContext:Landroid/content/Context;

    .line 311
    invoke-virtual {v1}, Landroid/content/Context;->getPackageName()Ljava/lang/String;

    move-result-object v1

    .line 310
    invoke-static {v0, v1}, Landroid/text/TextUtils;->equals(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Z

    move-result v0

    if-eqz v0, :cond_13

    goto :goto_1b

    .line 312
    :cond_13
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "broadcastReceiver should belong to the same package as the context given when creating MediaSession."

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    .end local p0    # "this":Landroid/media/session/MediaSession;
    .end local p1    # "broadcastReceiver":Landroid/content/ComponentName;
    throw v0

    .line 316
    .restart local p0    # "this":Landroid/media/session/MediaSession;
    .restart local p1    # "broadcastReceiver":Landroid/content/ComponentName;
    :cond_1b
    :goto_1b
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0, p1}, Landroid/media/session/ISession;->setMediaButtonBroadcastReceiver(Landroid/content/ComponentName;)V
    :try_end_20
    .catch Landroid/os/RemoteException; {:try_start_2 .. :try_end_20} :catch_21

    .line 319
    goto :goto_29

    .line 317
    :catch_21
    move-exception v0

    .line 318
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "MediaSession"

    const-string v2, "Failure in setMediaButtonBroadcastReceiver."

    invoke-static {v1, v2, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 320
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_29
    return-void
.end method

.method public setMediaButtonReceiver(Landroid/app/PendingIntent;)V
    .registers 5
    .param p1, "mbr"    # Landroid/app/PendingIntent;
    .annotation runtime Ljava/lang/Deprecated;
    .end annotation

    .line 286
    :try_start_0
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0, p1}, Landroid/media/session/ISession;->setMediaButtonReceiver(Landroid/app/PendingIntent;)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_6

    .line 289
    goto :goto_e

    .line 287
    :catch_6
    move-exception v0

    .line 288
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "MediaSession"

    const-string v2, "Failure in setMediaButtonReceiver."

    invoke-static {v1, v2, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 290
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_e
    return-void
.end method

.method public setMetadata(Landroid/media/MediaMetadata;)V
    .registers 10
    .param p1, "metadata"    # Landroid/media/MediaMetadata;

    .line 497
    const-wide/16 v0, -0x1

    .line 498
    .local v0, "duration":J
    const/4 v2, 0x0

    .line 499
    .local v2, "fields":I
    const/4 v3, 0x0

    .line 500
    .local v3, "description":Landroid/media/MediaDescription;
    if-eqz p1, :cond_29

    .line 501
    new-instance v4, Landroid/media/MediaMetadata$Builder;

    invoke-direct {v4, p1}, Landroid/media/MediaMetadata$Builder;-><init>(Landroid/media/MediaMetadata;)V

    iget v5, p0, Landroid/media/session/MediaSession;->mMaxBitmapSize:I

    .line 502
    invoke-virtual {v4, v5}, Landroid/media/MediaMetadata$Builder;->setBitmapDimensionLimit(I)Landroid/media/MediaMetadata$Builder;

    move-result-object v4

    .line 503
    invoke-virtual {v4}, Landroid/media/MediaMetadata$Builder;->build()Landroid/media/MediaMetadata;

    move-result-object p1

    .line 504
    const-string v4, "android.media.metadata.DURATION"

    invoke-virtual {p1, v4}, Landroid/media/MediaMetadata;->containsKey(Ljava/lang/String;)Z

    move-result v5

    if-eqz v5, :cond_21

    .line 505
    invoke-virtual {p1, v4}, Landroid/media/MediaMetadata;->getLong(Ljava/lang/String;)J

    move-result-wide v0

    .line 507
    :cond_21
    invoke-virtual {p1}, Landroid/media/MediaMetadata;->size()I

    move-result v2

    .line 508
    invoke-virtual {p1}, Landroid/media/MediaMetadata;->getDescription()Landroid/media/MediaDescription;

    move-result-object v3

    .line 510
    :cond_29
    new-instance v4, Ljava/lang/StringBuilder;

    invoke-direct {v4}, Ljava/lang/StringBuilder;-><init>()V

    const-string v5, "size="

    invoke-virtual {v4, v5}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v4

    invoke-virtual {v4, v2}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v4

    const-string v5, ", description="

    invoke-virtual {v4, v5}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v4

    invoke-virtual {v4, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/Object;)Ljava/lang/StringBuilder;

    move-result-object v4

    invoke-virtual {v4}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v4

    .line 513
    .local v4, "metadataDescription":Ljava/lang/String;
    :try_start_46
    iget-object v5, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v5, p1, v0, v1, v4}, Landroid/media/session/ISession;->setMetadata(Landroid/media/MediaMetadata;JLjava/lang/String;)V
    :try_end_4b
    .catch Landroid/os/RemoteException; {:try_start_46 .. :try_end_4b} :catch_4c

    .line 516
    goto :goto_54

    .line 514
    :catch_4c
    move-exception v5

    .line 515
    .local v5, "e":Landroid/os/RemoteException;
    const-string v6, "MediaSession"

    const-string v7, "Dead object in setPlaybackState."

    invoke-static {v6, v7, v5}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 517
    .end local v5    # "e":Landroid/os/RemoteException;
    :goto_54
    return-void
.end method

.method public setPlaybackState(Landroid/media/session/PlaybackState;)V
    .registers 5
    .param p1, "state"    # Landroid/media/session/PlaybackState;

    .line 480
    iput-object p1, p0, Landroid/media/session/MediaSession;->mPlaybackState:Landroid/media/session/PlaybackState;

    .line 482
    :try_start_2
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0, p1}, Landroid/media/session/ISession;->setPlaybackState(Landroid/media/session/PlaybackState;)V
    :try_end_7
    .catch Landroid/os/RemoteException; {:try_start_2 .. :try_end_7} :catch_8

    .line 485
    goto :goto_10

    .line 483
    :catch_8
    move-exception v0

    .line 484
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "MediaSession"

    const-string v2, "Dead object in setPlaybackState."

    invoke-static {v1, v2, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 486
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_10
    return-void
.end method

.method public setPlaybackToLocal(Landroid/media/AudioAttributes;)V
    .registers 5
    .param p1, "attributes"    # Landroid/media/AudioAttributes;

    .line 347
    if-eqz p1, :cond_11

    .line 351
    :try_start_2
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0, p1}, Landroid/media/session/ISession;->setPlaybackToLocal(Landroid/media/AudioAttributes;)V
    :try_end_7
    .catch Landroid/os/RemoteException; {:try_start_2 .. :try_end_7} :catch_8

    .line 354
    goto :goto_10

    .line 352
    :catch_8
    move-exception v0

    .line 353
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "MediaSession"

    const-string v2, "Failure in setPlaybackToLocal."

    invoke-static {v1, v2, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 355
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_10
    return-void

    .line 348
    :cond_11
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "Attributes cannot be null for local playback."

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method public setPlaybackToRemote(Landroid/media/VolumeProvider;)V
    .registers 6
    .param p1, "volumeProvider"    # Landroid/media/VolumeProvider;

    .line 369
    if-eqz p1, :cond_37

    .line 372
    iget-object v0, p0, Landroid/media/session/MediaSession;->mLock:Ljava/lang/Object;

    monitor-enter v0

    .line 373
    :try_start_5
    iput-object p1, p0, Landroid/media/session/MediaSession;->mVolumeProvider:Landroid/media/VolumeProvider;

    .line 374
    monitor-exit v0
    :try_end_8
    .catchall {:try_start_5 .. :try_end_8} :catchall_34

    .line 375
    new-instance v0, Landroid/media/session/MediaSession$1;

    invoke-direct {v0, p0}, Landroid/media/session/MediaSession$1;-><init>(Landroid/media/session/MediaSession;)V

    invoke-virtual {p1, v0}, Landroid/media/VolumeProvider;->setCallback(Landroid/media/VolumeProvider$Callback;)V

    .line 383
    :try_start_10
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-virtual {p1}, Landroid/media/VolumeProvider;->getVolumeControl()I

    move-result v1

    .line 384
    invoke-virtual {p1}, Landroid/media/VolumeProvider;->getMaxVolume()I

    move-result v2

    invoke-virtual {p1}, Landroid/media/VolumeProvider;->getVolumeControlId()Ljava/lang/String;

    move-result-object v3

    .line 383
    invoke-interface {v0, v1, v2, v3}, Landroid/media/session/ISession;->setPlaybackToRemote(IILjava/lang/String;)V

    .line 385
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-virtual {p1}, Landroid/media/VolumeProvider;->getCurrentVolume()I

    move-result v1

    invoke-interface {v0, v1}, Landroid/media/session/ISession;->setCurrentVolume(I)V
    :try_end_2a
    .catch Landroid/os/RemoteException; {:try_start_10 .. :try_end_2a} :catch_2b

    .line 388
    goto :goto_33

    .line 386
    :catch_2b
    move-exception v0

    .line 387
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "MediaSession"

    const-string v2, "Failure in setPlaybackToRemote."

    invoke-static {v1, v2, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 389
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_33
    return-void

    .line 374
    :catchall_34
    move-exception v1

    :try_start_35
    monitor-exit v0
    :try_end_36
    .catchall {:try_start_35 .. :try_end_36} :catchall_34

    throw v1

    .line 370
    :cond_37
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "volumeProvider may not be null!"

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method public setQueue(Ljava/util/List;)V
    .registers 4
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "(",
            "Ljava/util/List<",
            "Landroid/media/session/MediaSession$QueueItem;",
            ">;)V"
        }
    .end annotation

    .line 532
    .local p1, "queue":Ljava/util/List;, "Ljava/util/List<Landroid/media/session/MediaSession$QueueItem;>;"
    if-nez p1, :cond_8

    .line 533
    :try_start_2
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0}, Landroid/media/session/ISession;->resetQueue()V

    goto :goto_11

    .line 535
    :cond_8
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0}, Landroid/media/session/ISession;->getBinderForSetQueue()Landroid/os/IBinder;

    move-result-object v0

    .line 536
    .local v0, "binder":Landroid/os/IBinder;
    invoke-static {v0, p1}, Landroid/media/session/ParcelableListBinder;->send(Landroid/os/IBinder;Ljava/util/List;)V
    :try_end_11
    .catch Landroid/os/RemoteException; {:try_start_2 .. :try_end_11} :catch_12

    .line 540
    .end local v0    # "binder":Landroid/os/IBinder;
    :goto_11
    goto :goto_18

    .line 538
    :catch_12
    move-exception v0

    .line 539
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "Dead object in setQueue."

    invoke-static {v1, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 541
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_18
    return-void
.end method

.method public setQueueTitle(Ljava/lang/CharSequence;)V
    .registers 4
    .param p1, "title"    # Ljava/lang/CharSequence;

    .line 552
    :try_start_0
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0, p1}, Landroid/media/session/ISession;->setQueueTitle(Ljava/lang/CharSequence;)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_6

    .line 555
    goto :goto_c

    .line 553
    :catch_6
    move-exception v0

    .line 554
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "Dead object in setQueueTitle."

    invoke-static {v1, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 556
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_c
    return-void
.end method

.method public setRatingType(I)V
    .registers 5
    .param p1, "type"    # I

    .line 573
    :try_start_0
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0, p1}, Landroid/media/session/ISession;->setRatingType(I)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_6

    .line 576
    goto :goto_e

    .line 574
    :catch_6
    move-exception v0

    .line 575
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "MediaSession"

    const-string v2, "Error in setRatingType."

    invoke-static {v1, v2, v0}, Landroid/util/Log;->e(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 577
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_e
    return-void
.end method

.method public setSessionActivity(Landroid/app/PendingIntent;)V
    .registers 5
    .param p1, "pi"    # Landroid/app/PendingIntent;

    .line 263
    :try_start_0
    iget-object v0, p0, Landroid/media/session/MediaSession;->mBinder:Landroid/media/session/ISession;

    invoke-interface {v0, p1}, Landroid/media/session/ISession;->setLaunchPendingIntent(Landroid/app/PendingIntent;)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_6

    .line 266
    goto :goto_e

    .line 264
    :catch_6
    move-exception v0

    .line 265
    .local v0, "e":Landroid/os/RemoteException;
    const-string v1, "MediaSession"

    const-string v2, "Failure in setLaunchPendingIntent."

    invoke-static {v1, v2, v0}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 267
    .end local v0    # "e":Landroid/os/RemoteException;
    :goto_e
    return-void
.end method
