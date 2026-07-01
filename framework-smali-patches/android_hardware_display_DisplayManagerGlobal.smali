.class public final Landroid/hardware/display/DisplayManagerGlobal;
.super Ljava/lang/Object;
.source "DisplayManagerGlobal.java"


# annotations
.annotation system Ldalvik/annotation/MemberClasses;
    value = {
        Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;,
        Landroid/hardware/display/DisplayManagerGlobal$DisplayManagerCallback;,
        Landroid/hardware/display/DisplayManagerGlobal$VirtualDisplayCallback;,
        Landroid/hardware/display/DisplayManagerGlobal$DisplayEvent;
    }
.end annotation


# static fields
.field public static final CACHE_KEY_DISPLAY_INFO_PROPERTY:Ljava/lang/String; = "cache_key.display_info"

.field private static final DEBUG:Z = false

.field public static final EVENT_DISPLAY_ADDED:I = 0x1

.field public static final EVENT_DISPLAY_BRIGHTNESS_CHANGED:I = 0x4

.field public static final EVENT_DISPLAY_CHANGED:I = 0x2

.field public static final EVENT_DISPLAY_HDR_SDR_RATIO_CHANGED:I = 0x5

.field public static final EVENT_DISPLAY_REMOVED:I = 0x3

.field private static final TAG:Ljava/lang/String; = "DisplayManager"

.field private static final USE_CACHE:Z

.field private static sInstance:Landroid/hardware/display/DisplayManagerGlobal;


# instance fields
.field private mCallback:Landroid/hardware/display/DisplayManagerGlobal$DisplayManagerCallback;

.field private mDispatchNativeCallbacks:Z

.field private mDisplayCache:Landroid/app/PropertyInvalidatedCache;
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "Landroid/app/PropertyInvalidatedCache<",
            "Ljava/lang/Integer;",
            "Landroid/view/DisplayInfo;",
            ">;"
        }
    .end annotation
.end field

.field private mDisplayIdCache:[I

.field private final mDisplayInfoCache:Landroid/util/SparseArray;
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "Landroid/util/SparseArray<",
            "Landroid/view/DisplayInfo;",
            ">;"
        }
    .end annotation
.end field

.field private final mDisplayListeners:Ljava/util/concurrent/CopyOnWriteArrayList;
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "Ljava/util/concurrent/CopyOnWriteArrayList<",
            "Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;",
            ">;"
        }
    .end annotation
.end field

.field private final mDm:Landroid/hardware/display/IDisplayManager;

.field private final mLock:Ljava/lang/Object;

.field private mNativeCallbackReportedRefreshRate:F

.field private final mOverlayProperties:Landroid/hardware/OverlayProperties;

.field private mRegisteredEventsMask:J

.field private final mWideColorSpace:Landroid/graphics/ColorSpace;

.field private mWifiDisplayScanNestCount:I


# direct methods
.method static bridge synthetic -$$Nest$fgetmDm(Landroid/hardware/display/DisplayManagerGlobal;)Landroid/hardware/display/IDisplayManager;
    .registers 1

    iget-object p0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    return-object p0
.end method

.method static bridge synthetic -$$Nest$mhandleDisplayEvent(Landroid/hardware/display/DisplayManagerGlobal;II)V
    .registers 3

    invoke-direct {p0, p1, p2}, Landroid/hardware/display/DisplayManagerGlobal;->handleDisplayEvent(II)V

    return-void
.end method

.method public constructor <init>(Landroid/hardware/display/IDisplayManager;)V
    .registers 5
    .param p1, "dm"    # Landroid/hardware/display/IDisplayManager;

    .line 127
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 107
    const/4 v0, 0x0

    iput-boolean v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDispatchNativeCallbacks:Z

    .line 109
    new-instance v0, Ljava/lang/Object;

    invoke-direct {v0}, Ljava/lang/Object;-><init>()V

    iput-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mLock:Ljava/lang/Object;

    .line 115
    const-wide/16 v0, 0x0

    iput-wide v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mRegisteredEventsMask:J

    .line 116
    new-instance v0, Ljava/util/concurrent/CopyOnWriteArrayList;

    invoke-direct {v0}, Ljava/util/concurrent/CopyOnWriteArrayList;-><init>()V

    iput-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayListeners:Ljava/util/concurrent/CopyOnWriteArrayList;

    .line 119
    new-instance v0, Landroid/util/SparseArray;

    invoke-direct {v0}, Landroid/util/SparseArray;-><init>()V

    iput-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayInfoCache:Landroid/util/SparseArray;

    .line 139
    new-instance v0, Landroid/hardware/display/DisplayManagerGlobal$1;

    const/16 v1, 0x8

    const-string v2, "cache_key.display_info"

    invoke-direct {v0, p0, v1, v2}, Landroid/hardware/display/DisplayManagerGlobal$1;-><init>(Landroid/hardware/display/DisplayManagerGlobal;ILjava/lang/String;)V

    iput-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayCache:Landroid/app/PropertyInvalidatedCache;

    .line 128
    iput-object p1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    .line 130
    nop

    .line 132
    :try_start_2d
    invoke-static {}, Landroid/graphics/ColorSpace$Named;->values()[Landroid/graphics/ColorSpace$Named;

    move-result-object v0

    invoke-interface {p1}, Landroid/hardware/display/IDisplayManager;->getPreferredWideGamutColorSpaceId()I

    move-result v1

    aget-object v0, v0, v1

    .line 131
    invoke-static {v0}, Landroid/graphics/ColorSpace;->get(Landroid/graphics/ColorSpace$Named;)Landroid/graphics/ColorSpace;

    move-result-object v0

    iput-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mWideColorSpace:Landroid/graphics/ColorSpace;

    .line 133
    invoke-interface {p1}, Landroid/hardware/display/IDisplayManager;->getOverlaySupport()Landroid/hardware/OverlayProperties;

    move-result-object v0

    iput-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mOverlayProperties:Landroid/hardware/OverlayProperties;
    :try_end_43
    .catch Landroid/os/RemoteException; {:try_start_2d .. :try_end_43} :catch_45

    .line 136
    nop

    .line 137
    return-void

    .line 134
    :catch_45
    move-exception v0

    .line 135
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method private calculateEventsMaskLocked()I
    .registers 8

    .line 395
    const/4 v0, 0x0

    .line 396
    .local v0, "mask":I
    iget-object v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayListeners:Ljava/util/concurrent/CopyOnWriteArrayList;

    invoke-virtual {v1}, Ljava/util/concurrent/CopyOnWriteArrayList;->size()I

    move-result v1

    .line 397
    .local v1, "numListeners":I
    const/4 v2, 0x0

    .local v2, "i":I
    :goto_8
    if-ge v2, v1, :cond_1a

    .line 398
    int-to-long v3, v0

    iget-object v5, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayListeners:Ljava/util/concurrent/CopyOnWriteArrayList;

    invoke-virtual {v5, v2}, Ljava/util/concurrent/CopyOnWriteArrayList;->get(I)Ljava/lang/Object;

    move-result-object v5

    check-cast v5, Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;

    iget-wide v5, v5, Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;->mEventsMask:J

    or-long/2addr v3, v5

    long-to-int v0, v3

    .line 397
    add-int/lit8 v2, v2, 0x1

    goto :goto_8

    .line 400
    .end local v2    # "i":I
    :cond_1a
    iget-boolean v2, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDispatchNativeCallbacks:Z

    if-eqz v2, :cond_23

    .line 401
    int-to-long v2, v0

    const-wide/16 v4, 0x7

    or-long/2addr v2, v4

    long-to-int v0, v2

    .line 405
    :cond_23
    return v0
.end method

.method private static eventToString(I)Ljava/lang/String;
    .registers 2
    .param p0, "event"    # I

    .line 1294
    packed-switch p0, :pswitch_data_16

    .line 1306
    const-string v0, "UNKNOWN"

    return-object v0

    .line 1304
    :pswitch_6
    const-string v0, "HDR_SDR_RATIO_CHANGED"

    return-object v0

    .line 1302
    :pswitch_9
    const-string v0, "BRIGHTNESS_CHANGED"

    return-object v0

    .line 1300
    :pswitch_c
    const-string v0, "REMOVED"

    return-object v0

    .line 1298
    :pswitch_f
    const-string v0, "CHANGED"

    return-object v0

    .line 1296
    :pswitch_12
    const-string v0, "ADDED"

    return-object v0

    nop

    :pswitch_data_16
    .packed-switch 0x1
        :pswitch_12
        :pswitch_f
        :pswitch_c
        :pswitch_9
        :pswitch_6
    .end packed-switch
.end method

.method private findDisplayListenerLocked(Landroid/hardware/display/DisplayManager$DisplayListener;)I
    .registers 5
    .param p1, "listener"    # Landroid/hardware/display/DisplayManager$DisplayListener;

    .line 384
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayListeners:Ljava/util/concurrent/CopyOnWriteArrayList;

    invoke-virtual {v0}, Ljava/util/concurrent/CopyOnWriteArrayList;->size()I

    move-result v0

    .line 385
    .local v0, "numListeners":I
    const/4 v1, 0x0

    .local v1, "i":I
    :goto_7
    if-ge v1, v0, :cond_19

    .line 386
    iget-object v2, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayListeners:Ljava/util/concurrent/CopyOnWriteArrayList;

    invoke-virtual {v2, v1}, Ljava/util/concurrent/CopyOnWriteArrayList;->get(I)Ljava/lang/Object;

    move-result-object v2

    check-cast v2, Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;

    iget-object v2, v2, Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;->mListener:Landroid/hardware/display/DisplayManager$DisplayListener;

    if-ne v2, p1, :cond_16

    .line 387
    return v1

    .line 385
    :cond_16
    add-int/lit8 v1, v1, 0x1

    goto :goto_7

    .line 390
    .end local v1    # "i":I
    :cond_19
    const/4 v1, -0x1

    return v1
.end method

.method private getDisplayInfoLocked(I)Landroid/view/DisplayInfo;
    .registers 5
    .param p1, "displayId"    # I

    .line 191
    const/4 v0, 0x0

    .line 192
    .local v0, "info":Landroid/view/DisplayInfo;
    iget-object v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayCache:Landroid/app/PropertyInvalidatedCache;

    if-eqz v1, :cond_11

    .line 193
    invoke-static {p1}, Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;

    move-result-object v2

    invoke-virtual {v1, v2}, Landroid/app/PropertyInvalidatedCache;->query(Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v1

    move-object v0, v1

    check-cast v0, Landroid/view/DisplayInfo;

    goto :goto_1d

    .line 196
    :cond_11
    :try_start_11
    iget-object v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v1, p1}, Landroid/hardware/display/IDisplayManager;->getDisplayInfo(I)Landroid/view/DisplayInfo;

    move-result-object v1
    :try_end_17
    .catch Landroid/os/RemoteException; {:try_start_11 .. :try_end_17} :catch_19

    move-object v0, v1

    .line 199
    goto :goto_1d

    .line 197
    :catch_19
    move-exception v1

    .line 198
    .local v1, "ex":Landroid/os/RemoteException;
    invoke-virtual {v1}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    .line 201
    .end local v1    # "ex":Landroid/os/RemoteException;
    :goto_1d
    if-nez v0, :cond_21

    .line 202
    const/4 v1, 0x0

    return-object v1

    .line 205
    :cond_21
    invoke-direct {p0}, Landroid/hardware/display/DisplayManagerGlobal;->registerCallbackIfNeededLocked()V

    .line 210
    return-object v0
.end method

.method public static getInstance()Landroid/hardware/display/DisplayManagerGlobal;
    .registers 4

    .line 161
    const-class v0, Landroid/hardware/display/DisplayManagerGlobal;

    monitor-enter v0

    .line 162
    :try_start_3
    sget-object v1, Landroid/hardware/display/DisplayManagerGlobal;->sInstance:Landroid/hardware/display/DisplayManagerGlobal;

    if-nez v1, :cond_1a

    .line 163
    const-string v1, "display"

    invoke-static {v1}, Landroid/os/ServiceManager;->getService(Ljava/lang/String;)Landroid/os/IBinder;

    move-result-object v1

    .line 164
    .local v1, "b":Landroid/os/IBinder;
    if-eqz v1, :cond_1a

    .line 165
    new-instance v2, Landroid/hardware/display/DisplayManagerGlobal;

    invoke-static {v1}, Landroid/hardware/display/IDisplayManager$Stub;->asInterface(Landroid/os/IBinder;)Landroid/hardware/display/IDisplayManager;

    move-result-object v3

    invoke-direct {v2, v3}, Landroid/hardware/display/DisplayManagerGlobal;-><init>(Landroid/hardware/display/IDisplayManager;)V

    sput-object v2, Landroid/hardware/display/DisplayManagerGlobal;->sInstance:Landroid/hardware/display/DisplayManagerGlobal;

    .line 168
    .end local v1    # "b":Landroid/os/IBinder;
    :cond_1a
    sget-object v1, Landroid/hardware/display/DisplayManagerGlobal;->sInstance:Landroid/hardware/display/DisplayManagerGlobal;

    monitor-exit v0

    return-object v1

    .line 169
    :catchall_1e
    move-exception v1

    monitor-exit v0
    :try_end_20
    .catchall {:try_start_3 .. :try_end_20} :catchall_1e

    throw v1
.end method

.method private static getLooperForHandler(Landroid/os/Handler;)Landroid/os/Looper;
    .registers 4
    .param p0, "handler"    # Landroid/os/Handler;

    .line 373
    if-eqz p0, :cond_7

    invoke-virtual {p0}, Landroid/os/Handler;->getLooper()Landroid/os/Looper;

    move-result-object v0

    goto :goto_b

    :cond_7
    invoke-static {}, Landroid/os/Looper;->myLooper()Landroid/os/Looper;

    move-result-object v0

    .line 374
    .local v0, "looper":Landroid/os/Looper;
    :goto_b
    if-nez v0, :cond_11

    .line 375
    invoke-static {}, Landroid/os/Looper;->getMainLooper()Landroid/os/Looper;

    move-result-object v0

    .line 377
    :cond_11
    if-eqz v0, :cond_14

    .line 380
    return-object v0

    .line 378
    :cond_14
    new-instance v1, Ljava/lang/RuntimeException;

    const-string v2, "Could not get Looper for the UI thread."

    invoke-direct {v1, v2}, Ljava/lang/RuntimeException;-><init>(Ljava/lang/String;)V

    throw v1
.end method

.method private handleDisplayEvent(II)V
    .registers 8
    .param p1, "displayId"    # I
    .param p2, "event"    # I

    .line 429
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mLock:Ljava/lang/Object;

    monitor-enter v0

    .line 438
    :try_start_3
    invoke-direct {p0, p1}, Landroid/hardware/display/DisplayManagerGlobal;->getDisplayInfoLocked(I)Landroid/view/DisplayInfo;

    move-result-object v1

    .line 439
    .local v1, "info":Landroid/view/DisplayInfo;
    const/4 v2, 0x2

    if-ne p2, v2, :cond_29

    iget-boolean v2, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDispatchNativeCallbacks:Z

    if-eqz v2, :cond_29

    .line 442
    if-nez p1, :cond_29

    .line 445
    invoke-direct {p0, p1}, Landroid/hardware/display/DisplayManagerGlobal;->getDisplayInfoLocked(I)Landroid/view/DisplayInfo;

    move-result-object v2

    .line 446
    .local v2, "display":Landroid/view/DisplayInfo;
    if-eqz v2, :cond_29

    iget v3, p0, Landroid/hardware/display/DisplayManagerGlobal;->mNativeCallbackReportedRefreshRate:F

    .line 447
    invoke-virtual {v2}, Landroid/view/DisplayInfo;->getRefreshRate()F

    move-result v4

    cmpl-float v3, v3, v4

    if-eqz v3, :cond_29

    .line 448
    invoke-virtual {v2}, Landroid/view/DisplayInfo;->getRefreshRate()F

    move-result v3

    iput v3, p0, Landroid/hardware/display/DisplayManagerGlobal;->mNativeCallbackReportedRefreshRate:F

    .line 450
    invoke-static {v3}, Landroid/hardware/display/DisplayManagerGlobal;->nSignalNativeCallbacks(F)V

    .line 454
    .end local v2    # "display":Landroid/view/DisplayInfo;
    :cond_29
    monitor-exit v0
    :try_end_2a
    .catchall {:try_start_3 .. :try_end_2a} :catchall_41

    .line 457
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayListeners:Ljava/util/concurrent/CopyOnWriteArrayList;

    invoke-virtual {v0}, Ljava/util/concurrent/CopyOnWriteArrayList;->iterator()Ljava/util/Iterator;

    move-result-object v0

    :goto_30
    invoke-interface {v0}, Ljava/util/Iterator;->hasNext()Z

    move-result v2

    if-eqz v2, :cond_40

    invoke-interface {v0}, Ljava/util/Iterator;->next()Ljava/lang/Object;

    move-result-object v2

    check-cast v2, Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;

    .line 458
    .local v2, "listener":Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;
    invoke-virtual {v2, p1, p2, v1}, Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;->sendDisplayEvent(IILandroid/view/DisplayInfo;)V

    .line 459
    .end local v2    # "listener":Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;
    goto :goto_30

    .line 460
    :cond_40
    return-void

    .line 454
    .end local v1    # "info":Landroid/view/DisplayInfo;
    :catchall_41
    move-exception v1

    :try_start_42
    monitor-exit v0
    :try_end_43
    .catchall {:try_start_42 .. :try_end_43} :catchall_41

    throw v1
.end method

.method public static invalidateLocalDisplayInfoCaches()V
    .registers 1

    .line 1247
    const-string v0, "cache_key.display_info"

    invoke-static {v0}, Landroid/app/PropertyInvalidatedCache;->invalidateCache(Ljava/lang/String;)V

    .line 1248
    return-void
.end method

.method private static native nSignalNativeCallbacks(F)V
.end method

.method private registerCallbackIfNeededLocked()V
    .registers 3

    .line 409
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mCallback:Landroid/hardware/display/DisplayManagerGlobal$DisplayManagerCallback;

    if-nez v0, :cond_f

    .line 410
    new-instance v0, Landroid/hardware/display/DisplayManagerGlobal$DisplayManagerCallback;

    const/4 v1, 0x0

    invoke-direct {v0, p0, v1}, Landroid/hardware/display/DisplayManagerGlobal$DisplayManagerCallback;-><init>(Landroid/hardware/display/DisplayManagerGlobal;Landroid/hardware/display/DisplayManagerGlobal$DisplayManagerCallback-IA;)V

    iput-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mCallback:Landroid/hardware/display/DisplayManagerGlobal$DisplayManagerCallback;

    .line 411
    invoke-direct {p0}, Landroid/hardware/display/DisplayManagerGlobal;->updateCallbackIfNeededLocked()V

    .line 413
    :cond_f
    return-void
.end method

.method private updateCallbackIfNeededLocked()V
    .registers 6

    .line 416
    invoke-direct {p0}, Landroid/hardware/display/DisplayManagerGlobal;->calculateEventsMaskLocked()I

    move-result v0

    .line 417
    .local v0, "mask":I
    int-to-long v1, v0

    iget-wide v3, p0, Landroid/hardware/display/DisplayManagerGlobal;->mRegisteredEventsMask:J

    cmp-long v1, v1, v3

    if-eqz v1, :cond_1d

    .line 419
    :try_start_b
    iget-object v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    iget-object v2, p0, Landroid/hardware/display/DisplayManagerGlobal;->mCallback:Landroid/hardware/display/DisplayManagerGlobal$DisplayManagerCallback;

    int-to-long v3, v0

    invoke-interface {v1, v2, v3, v4}, Landroid/hardware/display/IDisplayManager;->registerCallbackWithEventMask(Landroid/hardware/display/IDisplayManagerCallback;J)V

    .line 420
    int-to-long v1, v0

    iput-wide v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mRegisteredEventsMask:J
    :try_end_16
    .catch Landroid/os/RemoteException; {:try_start_b .. :try_end_16} :catch_17

    .line 423
    goto :goto_1d

    .line 421
    :catch_17
    move-exception v1

    .line 422
    .local v1, "ex":Landroid/os/RemoteException;
    invoke-virtual {v1}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v2

    throw v2

    .line 425
    .end local v1    # "ex":Landroid/os/RemoteException;
    :cond_1d
    :goto_1d
    return-void
.end method


# virtual methods
.method public areUserDisabledHdrTypesAllowed()Z
    .registers 3

    .line 596
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->areUserDisabledHdrTypesAllowed()Z

    move-result v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return v0

    .line 597
    :catch_7
    move-exception v0

    .line 598
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public connectWifiDisplay(Ljava/lang/String;)V
    .registers 4
    .param p1, "deviceAddress"    # Ljava/lang/String;

    .line 492
    if-eqz p1, :cond_f

    .line 497
    :try_start_2
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->connectWifiDisplay(Ljava/lang/String;)V
    :try_end_7
    .catch Landroid/os/RemoteException; {:try_start_2 .. :try_end_7} :catch_9

    .line 500
    nop

    .line 501
    return-void

    .line 498
    :catch_9
    move-exception v0

    .line 499
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1

    .line 493
    .end local v0    # "ex":Landroid/os/RemoteException;
    :cond_f
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "deviceAddress must not be null"

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method public createVirtualDisplay(Landroid/content/Context;Landroid/media/projection/MediaProjection;Landroid/hardware/display/VirtualDisplayConfig;Landroid/hardware/display/VirtualDisplay$Callback;Ljava/util/concurrent/Executor;)Landroid/hardware/display/VirtualDisplay;
    .registers 10
    .param p1, "context"    # Landroid/content/Context;
    .param p2, "projection"    # Landroid/media/projection/MediaProjection;
    .param p3, "virtualDisplayConfig"    # Landroid/hardware/display/VirtualDisplayConfig;
    .param p4, "callback"    # Landroid/hardware/display/VirtualDisplay$Callback;
    .param p5, "executor"    # Ljava/util/concurrent/Executor;

    .line 639
    new-instance v0, Landroid/hardware/display/DisplayManagerGlobal$VirtualDisplayCallback;

    invoke-direct {v0, p4, p5}, Landroid/hardware/display/DisplayManagerGlobal$VirtualDisplayCallback;-><init>(Landroid/hardware/display/VirtualDisplay$Callback;Ljava/util/concurrent/Executor;)V

    .line 640
    .local v0, "callbackWrapper":Landroid/hardware/display/DisplayManagerGlobal$VirtualDisplayCallback;
    if-eqz p2, :cond_c

    invoke-virtual {p2}, Landroid/media/projection/MediaProjection;->getProjection()Landroid/media/projection/IMediaProjection;

    move-result-object v1

    goto :goto_d

    :cond_c
    const/4 v1, 0x0

    .line 643
    .local v1, "projectionToken":Landroid/media/projection/IMediaProjection;
    :goto_d
    :try_start_d
    iget-object v2, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    .line 644
    invoke-virtual {p1}, Landroid/content/Context;->getPackageName()Ljava/lang/String;

    move-result-object v3

    .line 643
    invoke-interface {v2, p3, v0, v1, v3}, Landroid/hardware/display/IDisplayManager;->createVirtualDisplay(Landroid/hardware/display/VirtualDisplayConfig;Landroid/hardware/display/IVirtualDisplayCallback;Landroid/media/projection/IMediaProjection;Ljava/lang/String;)I

    move-result v2
    :try_end_17
    .catch Landroid/os/RemoteException; {:try_start_d .. :try_end_17} :catch_1d

    .line 647
    .local v2, "displayId":I
    nop

    .line 648
    invoke-virtual {p0, p3, v0, v2}, Landroid/hardware/display/DisplayManagerGlobal;->createVirtualDisplayWrapper(Landroid/hardware/display/VirtualDisplayConfig;Landroid/hardware/display/IVirtualDisplayCallback;I)Landroid/hardware/display/VirtualDisplay;

    move-result-object v3

    return-object v3

    .line 645
    .end local v2    # "displayId":I
    :catch_1d
    move-exception v2

    .line 646
    .local v2, "ex":Landroid/os/RemoteException;
    invoke-virtual {v2}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v3

    throw v3
.end method

.method public createVirtualDisplayWrapper(Landroid/hardware/display/VirtualDisplayConfig;Landroid/hardware/display/IVirtualDisplayCallback;I)Landroid/hardware/display/VirtualDisplay;
    .registers 9
    .param p1, "virtualDisplayConfig"    # Landroid/hardware/display/VirtualDisplayConfig;
    .param p2, "callbackWrapper"    # Landroid/hardware/display/IVirtualDisplayCallback;
    .param p3, "displayId"    # I

    .line 659
    const/4 v0, 0x0

    const-string v1, "DisplayManager"

    if-gez p3, :cond_20

    .line 660
    new-instance v2, Ljava/lang/StringBuilder;

    invoke-direct {v2}, Ljava/lang/StringBuilder;-><init>()V

    const-string v3, "Could not create virtual display: "

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v2

    invoke-virtual {p1}, Landroid/hardware/display/VirtualDisplayConfig;->getName()Ljava/lang/String;

    move-result-object v3

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v2

    invoke-virtual {v2}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v2

    invoke-static {v1, v2}, Landroid/util/Log;->e(Ljava/lang/String;Ljava/lang/String;)I

    .line 661
    return-object v0

    .line 663
    :cond_20
    invoke-virtual {p0, p3}, Landroid/hardware/display/DisplayManagerGlobal;->getRealDisplay(I)Landroid/view/Display;

    move-result-object v2

    .line 664
    .local v2, "display":Landroid/view/Display;
    if-nez v2, :cond_4d

    .line 665
    new-instance v3, Ljava/lang/StringBuilder;

    invoke-direct {v3}, Ljava/lang/StringBuilder;-><init>()V

    const-string v4, "Could not obtain display info for newly created virtual display: "

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    .line 666
    invoke-virtual {p1}, Landroid/hardware/display/VirtualDisplayConfig;->getName()Ljava/lang/String;

    move-result-object v4

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v3

    .line 665
    invoke-static {v1, v3}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;)I

    .line 668
    :try_start_40
    iget-object v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v1, p2}, Landroid/hardware/display/IDisplayManager;->releaseVirtualDisplay(Landroid/hardware/display/IVirtualDisplayCallback;)V
    :try_end_45
    .catch Landroid/os/RemoteException; {:try_start_40 .. :try_end_45} :catch_47

    .line 671
    nop

    .line 672
    return-object v0

    .line 669
    :catch_47
    move-exception v0

    .line 670
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1

    .line 674
    .end local v0    # "ex":Landroid/os/RemoteException;
    :cond_4d
    new-instance v0, Landroid/hardware/display/VirtualDisplay;

    .line 675
    invoke-virtual {p1}, Landroid/hardware/display/VirtualDisplayConfig;->getSurface()Landroid/view/Surface;

    move-result-object v1

    invoke-direct {v0, p0, v2, p2, v1}, Landroid/hardware/display/VirtualDisplay;-><init>(Landroid/hardware/display/DisplayManagerGlobal;Landroid/view/Display;Landroid/hardware/display/IVirtualDisplayCallback;Landroid/view/Surface;)V

    .line 674
    return-object v0
.end method

.method public disableLocalDisplayInfoCaches()V
    .registers 2

    .line 1254
    const/4 v0, 0x0

    iput-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayCache:Landroid/app/PropertyInvalidatedCache;

    .line 1255
    return-void
.end method

.method public disconnectWifiDisplay()V
    .registers 3

    .line 522
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->disconnectWifiDisplay()V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 525
    nop

    .line 526
    return-void

    .line 523
    :catch_7
    move-exception v0

    .line 524
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public forgetWifiDisplay(Ljava/lang/String;)V
    .registers 4
    .param p1, "deviceAddress"    # Ljava/lang/String;

    .line 541
    if-eqz p1, :cond_f

    .line 546
    :try_start_2
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->forgetWifiDisplay(Ljava/lang/String;)V
    :try_end_7
    .catch Landroid/os/RemoteException; {:try_start_2 .. :try_end_7} :catch_9

    .line 549
    nop

    .line 550
    return-void

    .line 547
    :catch_9
    move-exception v0

    .line 548
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1

    .line 542
    .end local v0    # "ex":Landroid/os/RemoteException;
    :cond_f
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "deviceAddress must not be null"

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method public getAmbientBrightnessStats()Ljava/util/List;
    .registers 3
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "()",
            "Ljava/util/List<",
            "Landroid/hardware/display/AmbientBrightnessDayStats;",
            ">;"
        }
    .end annotation

    .line 956
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->getAmbientBrightnessStats()Landroid/content/pm/ParceledListSlice;

    move-result-object v0

    .line 957
    .local v0, "stats":Landroid/content/pm/ParceledListSlice;, "Landroid/content/pm/ParceledListSlice<Landroid/hardware/display/AmbientBrightnessDayStats;>;"
    if-nez v0, :cond_d

    .line 958
    invoke-static {}, Ljava/util/Collections;->emptyList()Ljava/util/List;

    move-result-object v1

    return-object v1

    .line 960
    :cond_d
    invoke-virtual {v0}, Landroid/content/pm/ParceledListSlice;->getList()Ljava/util/List;

    move-result-object v1
    :try_end_11
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_11} :catch_12

    return-object v1

    .line 961
    .end local v0    # "stats":Landroid/content/pm/ParceledListSlice;, "Landroid/content/pm/ParceledListSlice<Landroid/hardware/display/AmbientBrightnessDayStats;>;"
    :catch_12
    move-exception v0

    .line 962
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getBrightness(I)F
    .registers 4
    .param p1, "displayId"    # I

    .line 909
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->getBrightness(I)F

    move-result v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return v0

    .line 910
    :catch_7
    move-exception v0

    .line 911
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getBrightnessConfigurationForDisplay(Ljava/lang/String;I)Landroid/hardware/display/BrightnessConfiguration;
    .registers 5
    .param p1, "uniqueDisplayId"    # Ljava/lang/String;
    .param p2, "userId"    # I

    .line 806
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2}, Landroid/hardware/display/IDisplayManager;->getBrightnessConfigurationForDisplay(Ljava/lang/String;I)Landroid/hardware/display/BrightnessConfiguration;

    move-result-object v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return-object v0

    .line 807
    :catch_7
    move-exception v0

    .line 808
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getBrightnessConfigurationForUser(I)Landroid/hardware/display/BrightnessConfiguration;
    .registers 4
    .param p1, "userId"    # I

    .line 819
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->getBrightnessConfigurationForUser(I)Landroid/hardware/display/BrightnessConfiguration;

    move-result-object v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return-object v0

    .line 820
    :catch_7
    move-exception v0

    .line 821
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getBrightnessEvents(Ljava/lang/String;)Ljava/util/List;
    .registers 4
    .param p1, "callingPackage"    # Ljava/lang/String;
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "(",
            "Ljava/lang/String;",
            ")",
            "Ljava/util/List<",
            "Landroid/hardware/display/BrightnessChangeEvent;",
            ">;"
        }
    .end annotation

    .line 728
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    .line 729
    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->getBrightnessEvents(Ljava/lang/String;)Landroid/content/pm/ParceledListSlice;

    move-result-object v0

    .line 730
    .local v0, "events":Landroid/content/pm/ParceledListSlice;, "Landroid/content/pm/ParceledListSlice<Landroid/hardware/display/BrightnessChangeEvent;>;"
    if-nez v0, :cond_d

    .line 731
    invoke-static {}, Ljava/util/Collections;->emptyList()Ljava/util/List;

    move-result-object v1

    return-object v1

    .line 733
    :cond_d
    invoke-virtual {v0}, Landroid/content/pm/ParceledListSlice;->getList()Ljava/util/List;

    move-result-object v1
    :try_end_11
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_11} :catch_12

    return-object v1

    .line 734
    .end local v0    # "events":Landroid/content/pm/ParceledListSlice;, "Landroid/content/pm/ParceledListSlice<Landroid/hardware/display/BrightnessChangeEvent;>;"
    :catch_12
    move-exception v0

    .line 735
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getBrightnessInfo(I)Landroid/hardware/display/BrightnessInfo;
    .registers 4
    .param p1, "displayId"    # I

    .line 744
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->getBrightnessInfo(I)Landroid/hardware/display/BrightnessInfo;

    move-result-object v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return-object v0

    .line 745
    :catch_7
    move-exception v0

    .line 746
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getCompatibleDisplay(ILandroid/content/res/Resources;)Landroid/view/Display;
    .registers 5
    .param p1, "displayId"    # I
    .param p2, "resources"    # Landroid/content/res/Resources;

    .line 294
    invoke-virtual {p0, p1}, Landroid/hardware/display/DisplayManagerGlobal;->getDisplayInfo(I)Landroid/view/DisplayInfo;

    move-result-object v0

    .line 295
    .local v0, "displayInfo":Landroid/view/DisplayInfo;
    if-nez v0, :cond_8

    .line 296
    const/4 v1, 0x0

    return-object v1

    .line 298
    :cond_8
    new-instance v1, Landroid/view/Display;

    invoke-direct {v1, p0, p1, v0, p2}, Landroid/view/Display;-><init>(Landroid/hardware/display/DisplayManagerGlobal;ILandroid/view/DisplayInfo;Landroid/content/res/Resources;)V

    return-object v1
.end method

.method public getCompatibleDisplay(ILandroid/view/DisplayAdjustments;)Landroid/view/Display;
    .registers 5
    .param p1, "displayId"    # I
    .param p2, "daj"    # Landroid/view/DisplayAdjustments;

    .line 276
    invoke-virtual {p0, p1}, Landroid/hardware/display/DisplayManagerGlobal;->getDisplayInfo(I)Landroid/view/DisplayInfo;

    move-result-object v0

    .line 277
    .local v0, "displayInfo":Landroid/view/DisplayInfo;
    if-nez v0, :cond_8

    .line 278
    const/4 v1, 0x0

    return-object v1

    .line 280
    :cond_8
    new-instance v1, Landroid/view/Display;

    invoke-direct {v1, p0, p1, v0, p2}, Landroid/view/Display;-><init>(Landroid/hardware/display/DisplayManagerGlobal;ILandroid/view/DisplayInfo;Landroid/view/DisplayAdjustments;)V

    return-object v1
.end method

.method public getDefaultBrightnessConfiguration()Landroid/hardware/display/BrightnessConfiguration;
    .registers 3

    .line 832
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->getDefaultBrightnessConfiguration()Landroid/hardware/display/BrightnessConfiguration;

    move-result-object v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return-object v0

    .line 833
    :catch_7
    move-exception v0

    .line 834
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getDisplayDecorationSupport(I)Landroid/hardware/graphics/common/DisplayDecorationSupport;
    .registers 4
    .param p1, "displayId"    # I

    .line 894
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->getDisplayDecorationSupport(I)Landroid/hardware/graphics/common/DisplayDecorationSupport;

    move-result-object v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return-object v0

    .line 895
    :catch_7
    move-exception v0

    .line 896
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getDisplayIds()[I
    .registers 2

    .line 220
    const/4 v0, 0x0

    invoke-virtual {p0, v0}, Landroid/hardware/display/DisplayManagerGlobal;->getDisplayIds(Z)[I

    move-result-object v0

    return-object v0
.end method

.method public getDisplayIds(Z)[I
    .registers 4
    .param p1, "includeDisabled"    # Z

    .line 231
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mLock:Ljava/lang/Object;

    monitor-enter v0
    :try_end_3
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_3} :catch_11

    .line 238
    :try_start_3
    iget-object v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v1, p1}, Landroid/hardware/display/IDisplayManager;->getDisplayIds(Z)[I

    move-result-object v1

    .line 242
    .local v1, "displayIds":[I
    invoke-direct {p0}, Landroid/hardware/display/DisplayManagerGlobal;->registerCallbackIfNeededLocked()V

    .line 243
    monitor-exit v0

    return-object v1

    .line 244
    .end local v1    # "displayIds":[I
    :catchall_e
    move-exception v1

    monitor-exit v0
    :try_end_10
    .catchall {:try_start_3 .. :try_end_10} :catchall_e

    .end local p0    # "this":Landroid/hardware/display/DisplayManagerGlobal;
    .end local p1    # "includeDisabled":Z
    :try_start_10
    throw v1
    :try_end_11
    .catch Landroid/os/RemoteException; {:try_start_10 .. :try_end_11} :catch_11

    .line 245
    .restart local p0    # "this":Landroid/hardware/display/DisplayManagerGlobal;
    .restart local p1    # "includeDisabled":Z
    :catch_11
    move-exception v0

    .line 246
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getDisplayInfo(I)Landroid/view/DisplayInfo;
    .registers 4
    .param p1, "displayId"    # I

    .line 181
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mLock:Ljava/lang/Object;

    monitor-enter v0

    .line 182
    :try_start_3
    invoke-direct {p0, p1}, Landroid/hardware/display/DisplayManagerGlobal;->getDisplayInfoLocked(I)Landroid/view/DisplayInfo;

    move-result-object v1

    monitor-exit v0

    return-object v1

    .line 183
    :catchall_9
    move-exception v1

    monitor-exit v0
    :try_end_b
    .catchall {:try_start_3 .. :try_end_b} :catchall_9

    throw v1
.end method

.method public getHdrConversionMode()Landroid/hardware/display/HdrConversionMode;
    .registers 3

    .line 1029
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->getHdrConversionMode()Landroid/hardware/display/HdrConversionMode;

    move-result-object v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return-object v0

    .line 1030
    :catch_7
    move-exception v0

    .line 1031
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getHdrConversionModeSetting()Landroid/hardware/display/HdrConversionMode;
    .registers 3

    .line 1018
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->getHdrConversionModeSetting()Landroid/hardware/display/HdrConversionMode;

    move-result-object v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return-object v0

    .line 1019
    :catch_7
    move-exception v0

    .line 1020
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getMinimumBrightnessCurve()Landroid/util/Pair;
    .registers 4
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "()",
            "Landroid/util/Pair<",
            "[F[F>;"
        }
    .end annotation

    .line 944
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->getMinimumBrightnessCurve()Landroid/hardware/display/Curve;

    move-result-object v0

    .line 945
    .local v0, "curve":Landroid/hardware/display/Curve;
    invoke-virtual {v0}, Landroid/hardware/display/Curve;->getX()[F

    move-result-object v1

    invoke-virtual {v0}, Landroid/hardware/display/Curve;->getY()[F

    move-result-object v2

    invoke-static {v1, v2}, Landroid/util/Pair;->create(Ljava/lang/Object;Ljava/lang/Object;)Landroid/util/Pair;

    move-result-object v1
    :try_end_12
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_12} :catch_13

    return-object v1

    .line 946
    .end local v0    # "curve":Landroid/hardware/display/Curve;
    :catch_13
    move-exception v0

    .line 947
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getOverlaySupport()Landroid/hardware/OverlayProperties;
    .registers 2

    .line 767
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mOverlayProperties:Landroid/hardware/OverlayProperties;

    return-object v0
.end method

.method public getPreferredWideGamutColorSpace()Landroid/graphics/ColorSpace;
    .registers 2

    .line 758
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mWideColorSpace:Landroid/graphics/ColorSpace;

    return-object v0
.end method

.method public getRealDisplay(I)Landroid/view/Display;
    .registers 3
    .param p1, "displayId"    # I

    .line 309
    sget-object v0, Landroid/view/DisplayAdjustments;->DEFAULT_DISPLAY_ADJUSTMENTS:Landroid/view/DisplayAdjustments;

    invoke-virtual {p0, p1, v0}, Landroid/hardware/display/DisplayManagerGlobal;->getCompatibleDisplay(ILandroid/view/DisplayAdjustments;)Landroid/view/Display;

    move-result-object v0

    return-object v0
.end method

.method public getRefreshRateSwitchingType()I
    .registers 3

    .line 1093
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->getRefreshRateSwitchingType()I

    move-result v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return v0

    .line 1094
    :catch_7
    move-exception v0

    .line 1095
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getStableDisplaySize()Landroid/graphics/Point;
    .registers 3

    .line 717
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->getStableDisplaySize()Landroid/graphics/Point;

    move-result-object v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return-object v0

    .line 718
    :catch_7
    move-exception v0

    .line 719
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getSupportedHdrOutputTypes()[I
    .registers 3

    .line 1040
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->getSupportedHdrOutputTypes()[I

    move-result-object v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return-object v0

    .line 1041
    :catch_7
    move-exception v0

    .line 1042
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getSystemPreferredDisplayMode(I)Landroid/view/Display$Mode;
    .registers 4
    .param p1, "displayId"    # I

    .line 994
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->getSystemPreferredDisplayMode(I)Landroid/view/Display$Mode;

    move-result-object v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return-object v0

    .line 995
    :catch_7
    move-exception v0

    .line 996
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getUserDisabledHdrTypes()[I
    .registers 3

    .line 608
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->getUserDisabledHdrTypes()[I

    move-result-object v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return-object v0

    .line 609
    :catch_7
    move-exception v0

    .line 610
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getUserPreferredDisplayMode(I)Landroid/view/Display$Mode;
    .registers 4
    .param p1, "displayId"    # I

    .line 983
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->getUserPreferredDisplayMode(I)Landroid/view/Display$Mode;

    move-result-object v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return-object v0

    .line 984
    :catch_7
    move-exception v0

    .line 985
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getWifiDisplayStatus()Landroid/hardware/display/WifiDisplayStatus;
    .registers 2

    new-instance v0, Landroid/hardware/display/WifiDisplayStatus;

    invoke-direct {v0}, Landroid/hardware/display/WifiDisplayStatus;-><init>()V

    return-object v0
.end method

.method public isMinimalPostProcessingRequested(I)Z
    .registers 4
    .param p1, "displayId"    # I

    .line 845
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->isMinimalPostProcessingRequested(I)Z

    move-result v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return v0

    .line 846
    :catch_7
    move-exception v0

    .line 847
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public isUidPresentOnDisplay(II)Z
    .registers 5
    .param p1, "uid"    # I
    .param p2, "displayId"    # I

    .line 259
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2}, Landroid/hardware/display/IDisplayManager;->isUidPresentOnDisplay(II)Z

    move-result v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return v0

    .line 260
    :catch_7
    move-exception v0

    .line 261
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public overrideHdrTypes(I[I)V
    .registers 5
    .param p1, "displayId"    # I
    .param p2, "modes"    # [I

    .line 621
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2}, Landroid/hardware/display/IDisplayManager;->overrideHdrTypes(I[I)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 624
    nop

    .line 625
    return-void

    .line 622
    :catch_7
    move-exception v0

    .line 623
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public pauseWifiDisplay()V
    .registers 3

    .line 505
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->pauseWifiDisplay()V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 508
    nop

    .line 509
    return-void

    .line 506
    :catch_7
    move-exception v0

    .line 507
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public registerDisplayListener(Landroid/hardware/display/DisplayManager$DisplayListener;Landroid/os/Handler;J)V
    .registers 8
    .param p1, "listener"    # Landroid/hardware/display/DisplayManager$DisplayListener;
    .param p2, "handler"    # Landroid/os/Handler;
    .param p3, "eventsMask"    # J

    .line 323
    invoke-static {p2}, Landroid/hardware/display/DisplayManagerGlobal;->getLooperForHandler(Landroid/os/Handler;)Landroid/os/Looper;

    move-result-object v0

    .line 324
    .local v0, "looper":Landroid/os/Looper;
    new-instance v1, Landroid/os/Handler;

    invoke-direct {v1, v0}, Landroid/os/Handler;-><init>(Landroid/os/Looper;)V

    .line 325
    .local v1, "springBoard":Landroid/os/Handler;
    new-instance v2, Landroid/os/HandlerExecutor;

    invoke-direct {v2, v1}, Landroid/os/HandlerExecutor;-><init>(Landroid/os/Handler;)V

    invoke-virtual {p0, p1, v2, p3, p4}, Landroid/hardware/display/DisplayManagerGlobal;->registerDisplayListener(Landroid/hardware/display/DisplayManager$DisplayListener;Ljava/util/concurrent/Executor;J)V

    .line 326
    return-void
.end method

.method public registerDisplayListener(Landroid/hardware/display/DisplayManager$DisplayListener;Ljava/util/concurrent/Executor;J)V
    .registers 9
    .param p1, "listener"    # Landroid/hardware/display/DisplayManager$DisplayListener;
    .param p2, "executor"    # Ljava/util/concurrent/Executor;
    .param p3, "eventsMask"    # J

    .line 336
    if-eqz p1, :cond_3a

    .line 340
    const-wide/16 v0, 0x0

    cmp-long v0, p3, v0

    if-eqz v0, :cond_32

    .line 344
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mLock:Ljava/lang/Object;

    monitor-enter v0

    .line 345
    :try_start_b
    invoke-direct {p0, p1}, Landroid/hardware/display/DisplayManagerGlobal;->findDisplayListenerLocked(Landroid/hardware/display/DisplayManager$DisplayListener;)I

    move-result v1

    .line 346
    .local v1, "index":I
    if-gez v1, :cond_1f

    .line 347
    iget-object v2, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayListeners:Ljava/util/concurrent/CopyOnWriteArrayList;

    new-instance v3, Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;

    invoke-direct {v3, p1, p2, p3, p4}, Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;-><init>(Landroid/hardware/display/DisplayManager$DisplayListener;Ljava/util/concurrent/Executor;J)V

    invoke-virtual {v2, v3}, Ljava/util/concurrent/CopyOnWriteArrayList;->add(Ljava/lang/Object;)Z

    .line 348
    invoke-direct {p0}, Landroid/hardware/display/DisplayManagerGlobal;->registerCallbackIfNeededLocked()V

    goto :goto_2a

    .line 350
    :cond_1f
    iget-object v2, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayListeners:Ljava/util/concurrent/CopyOnWriteArrayList;

    invoke-virtual {v2, v1}, Ljava/util/concurrent/CopyOnWriteArrayList;->get(I)Ljava/lang/Object;

    move-result-object v2

    check-cast v2, Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;

    invoke-virtual {v2, p3, p4}, Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;->setEventsMask(J)V

    .line 352
    :goto_2a
    invoke-direct {p0}, Landroid/hardware/display/DisplayManagerGlobal;->updateCallbackIfNeededLocked()V

    .line 353
    .end local v1    # "index":I
    monitor-exit v0

    .line 354
    return-void

    .line 353
    :catchall_2f
    move-exception v1

    monitor-exit v0
    :try_end_31
    .catchall {:try_start_b .. :try_end_31} :catchall_2f

    throw v1

    .line 341
    :cond_32
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "The set of events to listen to must not be empty."

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v0

    .line 337
    :cond_3a
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "listener must not be null"

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method public registerNativeChoreographerForRefreshRateCallbacks()V
    .registers 4

    .line 1266
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mLock:Ljava/lang/Object;

    monitor-enter v0

    .line 1267
    const/4 v1, 0x1

    :try_start_4
    iput-boolean v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDispatchNativeCallbacks:Z

    .line 1268
    invoke-direct {p0}, Landroid/hardware/display/DisplayManagerGlobal;->registerCallbackIfNeededLocked()V

    .line 1269
    invoke-direct {p0}, Landroid/hardware/display/DisplayManagerGlobal;->updateCallbackIfNeededLocked()V

    .line 1270
    const/4 v1, 0x0

    invoke-direct {p0, v1}, Landroid/hardware/display/DisplayManagerGlobal;->getDisplayInfoLocked(I)Landroid/view/DisplayInfo;

    move-result-object v1

    .line 1271
    .local v1, "display":Landroid/view/DisplayInfo;
    if-eqz v1, :cond_1c

    .line 1274
    invoke-virtual {v1}, Landroid/view/DisplayInfo;->getRefreshRate()F

    move-result v2

    iput v2, p0, Landroid/hardware/display/DisplayManagerGlobal;->mNativeCallbackReportedRefreshRate:F

    .line 1275
    invoke-static {v2}, Landroid/hardware/display/DisplayManagerGlobal;->nSignalNativeCallbacks(F)V

    .line 1277
    .end local v1    # "display":Landroid/view/DisplayInfo;
    :cond_1c
    monitor-exit v0

    .line 1278
    return-void

    .line 1277
    :catchall_1e
    move-exception v1

    monitor-exit v0
    :try_end_20
    .catchall {:try_start_4 .. :try_end_20} :catchall_1e

    throw v1
.end method

.method public releaseVirtualDisplay(Landroid/hardware/display/IVirtualDisplayCallback;)V
    .registers 4
    .param p1, "token"    # Landroid/hardware/display/IVirtualDisplayCallback;

    .line 698
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->releaseVirtualDisplay(Landroid/hardware/display/IVirtualDisplayCallback;)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 701
    nop

    .line 702
    return-void

    .line 699
    :catch_7
    move-exception v0

    .line 700
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public renameWifiDisplay(Ljava/lang/String;Ljava/lang/String;)V
    .registers 5
    .param p1, "deviceAddress"    # Ljava/lang/String;
    .param p2, "alias"    # Ljava/lang/String;

    .line 529
    if-eqz p1, :cond_f

    .line 534
    :try_start_2
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2}, Landroid/hardware/display/IDisplayManager;->renameWifiDisplay(Ljava/lang/String;Ljava/lang/String;)V
    :try_end_7
    .catch Landroid/os/RemoteException; {:try_start_2 .. :try_end_7} :catch_9

    .line 537
    nop

    .line 538
    return-void

    .line 535
    :catch_9
    move-exception v0

    .line 536
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1

    .line 530
    .end local v0    # "ex":Landroid/os/RemoteException;
    :cond_f
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "deviceAddress must not be null"

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method public requestColorMode(II)V
    .registers 5
    .param p1, "displayId"    # I
    .param p2, "colorMode"    # I

    .line 630
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2}, Landroid/hardware/display/IDisplayManager;->requestColorMode(II)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 633
    nop

    .line 634
    return-void

    .line 631
    :catch_7
    move-exception v0

    .line 632
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public resizeVirtualDisplay(Landroid/hardware/display/IVirtualDisplayCallback;III)V
    .registers 7
    .param p1, "token"    # Landroid/hardware/display/IVirtualDisplayCallback;
    .param p2, "width"    # I
    .param p3, "height"    # I
    .param p4, "densityDpi"    # I

    .line 690
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2, p3, p4}, Landroid/hardware/display/IDisplayManager;->resizeVirtualDisplay(Landroid/hardware/display/IVirtualDisplayCallback;III)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 693
    nop

    .line 694
    return-void

    .line 691
    :catch_7
    move-exception v0

    .line 692
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public resumeWifiDisplay()V
    .registers 3

    .line 513
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->resumeWifiDisplay()V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 516
    nop

    .line 517
    return-void

    .line 514
    :catch_7
    move-exception v0

    .line 515
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public setAreUserDisabledHdrTypesAllowed(Z)V
    .registers 4
    .param p1, "areUserDisabledHdrTypesAllowed"    # Z

    .line 584
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->setAreUserDisabledHdrTypesAllowed(Z)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 587
    nop

    .line 588
    return-void

    .line 585
    :catch_7
    move-exception v0

    .line 586
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public setBrightness(IF)V
    .registers 5
    .param p1, "displayId"    # I
    .param p2, "brightness"    # F

    .line 879
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2}, Landroid/hardware/display/IDisplayManager;->setBrightness(IF)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 882
    nop

    .line 883
    return-void

    .line 880
    :catch_7
    move-exception v0

    .line 881
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public setBrightnessConfigurationForDisplay(Landroid/hardware/display/BrightnessConfiguration;Ljava/lang/String;ILjava/lang/String;)V
    .registers 7
    .param p1, "c"    # Landroid/hardware/display/BrightnessConfiguration;
    .param p2, "uniqueDisplayId"    # Ljava/lang/String;
    .param p3, "userId"    # I
    .param p4, "packageName"    # Ljava/lang/String;

    .line 792
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2, p3, p4}, Landroid/hardware/display/IDisplayManager;->setBrightnessConfigurationForDisplay(Landroid/hardware/display/BrightnessConfiguration;Ljava/lang/String;ILjava/lang/String;)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 795
    nop

    .line 796
    return-void

    .line 793
    :catch_7
    move-exception v0

    .line 794
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public setBrightnessConfigurationForUser(Landroid/hardware/display/BrightnessConfiguration;ILjava/lang/String;)V
    .registers 6
    .param p1, "c"    # Landroid/hardware/display/BrightnessConfiguration;
    .param p2, "userId"    # I
    .param p3, "packageName"    # Ljava/lang/String;

    .line 778
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2, p3}, Landroid/hardware/display/IDisplayManager;->setBrightnessConfigurationForUser(Landroid/hardware/display/BrightnessConfiguration;ILjava/lang/String;)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 781
    nop

    .line 782
    return-void

    .line 779
    :catch_7
    move-exception v0

    .line 780
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public setHdrConversionMode(Landroid/hardware/display/HdrConversionMode;)V
    .registers 4
    .param p1, "hdrConversionMode"    # Landroid/hardware/display/HdrConversionMode;

    .line 1005
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->setHdrConversionMode(Landroid/hardware/display/HdrConversionMode;)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 1008
    nop

    .line 1009
    return-void

    .line 1006
    :catch_7
    move-exception v0

    .line 1007
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public setRefreshRateSwitchingType(I)V
    .registers 4
    .param p1, "newValue"    # I

    .line 1079
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->setRefreshRateSwitchingType(I)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 1082
    nop

    .line 1083
    return-void

    .line 1080
    :catch_7
    move-exception v0

    .line 1081
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public setShouldAlwaysRespectAppRequestedMode(Z)V
    .registers 4
    .param p1, "enabled"    # Z

    .line 1053
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->setShouldAlwaysRespectAppRequestedMode(Z)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 1056
    nop

    .line 1057
    return-void

    .line 1054
    :catch_7
    move-exception v0

    .line 1055
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public setTemporaryAutoBrightnessAdjustment(F)V
    .registers 4
    .param p1, "adjustment"    # F

    .line 927
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->setTemporaryAutoBrightnessAdjustment(F)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 930
    nop

    .line 931
    return-void

    .line 928
    :catch_7
    move-exception v0

    .line 929
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public setTemporaryBrightness(IF)V
    .registers 5
    .param p1, "displayId"    # I
    .param p2, "brightness"    # F

    .line 863
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2}, Landroid/hardware/display/IDisplayManager;->setTemporaryBrightness(IF)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 866
    nop

    .line 867
    return-void

    .line 864
    :catch_7
    move-exception v0

    .line 865
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public setUserDisabledHdrTypes([I)V
    .registers 4
    .param p1, "userDisabledHdrTypes"    # [I

    .line 567
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1}, Landroid/hardware/display/IDisplayManager;->setUserDisabledHdrTypes([I)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 570
    nop

    .line 571
    return-void

    .line 568
    :catch_7
    move-exception v0

    .line 569
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public setUserPreferredDisplayMode(ILandroid/view/Display$Mode;)V
    .registers 5
    .param p1, "displayId"    # I
    .param p2, "mode"    # Landroid/view/Display$Mode;

    .line 972
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2}, Landroid/hardware/display/IDisplayManager;->setUserPreferredDisplayMode(ILandroid/view/Display$Mode;)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 975
    nop

    .line 976
    return-void

    .line 973
    :catch_7
    move-exception v0

    .line 974
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method setVirtualDisplayState(Landroid/hardware/display/IVirtualDisplayCallback;Z)V
    .registers 5
    .param p1, "token"    # Landroid/hardware/display/IVirtualDisplayCallback;
    .param p2, "isOn"    # Z

    .line 706
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2}, Landroid/hardware/display/IDisplayManager;->setVirtualDisplayState(Landroid/hardware/display/IVirtualDisplayCallback;Z)V
    :try_end_5
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_5} :catch_7

    .line 709
    nop

    .line 710
    return-void

    .line 707
    :catch_7
    move-exception v0

    .line 708
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public setVirtualDisplaySurface(Landroid/hardware/display/IVirtualDisplayCallback;Landroid/view/Surface;)V
    .registers 5
    .param p1, "token"    # Landroid/hardware/display/IVirtualDisplayCallback;
    .param p2, "surface"    # Landroid/view/Surface;

    .line 680
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0, p1, p2}, Landroid/hardware/display/IDisplayManager;->setVirtualDisplaySurface(Landroid/hardware/display/IVirtualDisplayCallback;Landroid/view/Surface;)V

    .line 681
    if-eqz p2, :cond_9

    const/4 v0, 0x1

    goto :goto_a

    :cond_9
    const/4 v0, 0x0

    :goto_a
    invoke-virtual {p0, p1, v0}, Landroid/hardware/display/DisplayManagerGlobal;->setVirtualDisplayState(Landroid/hardware/display/IVirtualDisplayCallback;Z)V
    :try_end_d
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_d} :catch_f

    .line 684
    nop

    .line 685
    return-void

    .line 682
    :catch_f
    move-exception v0

    .line 683
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public shouldAlwaysRespectAppRequestedMode()Z
    .registers 3

    .line 1066
    :try_start_0
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v0}, Landroid/hardware/display/IDisplayManager;->shouldAlwaysRespectAppRequestedMode()Z

    move-result v0
    :try_end_6
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_6} :catch_7

    return v0

    .line 1067
    :catch_7
    move-exception v0

    .line 1068
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public startWifiDisplayScan()V
    .registers 4

    .line 463
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mLock:Ljava/lang/Object;

    monitor-enter v0

    .line 464
    :try_start_3
    iget v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mWifiDisplayScanNestCount:I

    add-int/lit8 v2, v1, 0x1

    iput v2, p0, Landroid/hardware/display/DisplayManagerGlobal;->mWifiDisplayScanNestCount:I

    if-nez v1, :cond_1a

    .line 465
    invoke-direct {p0}, Landroid/hardware/display/DisplayManagerGlobal;->registerCallbackIfNeededLocked()V
    :try_end_e
    .catchall {:try_start_3 .. :try_end_e} :catchall_1c

    .line 467
    :try_start_e
    iget-object v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v1}, Landroid/hardware/display/IDisplayManager;->startWifiDisplayScan()V
    :try_end_13
    .catch Landroid/os/RemoteException; {:try_start_e .. :try_end_13} :catch_14
    .catchall {:try_start_e .. :try_end_13} :catchall_1c

    .line 470
    goto :goto_1a

    .line 468
    :catch_14
    move-exception v1

    .line 469
    .local v1, "ex":Landroid/os/RemoteException;
    :try_start_15
    invoke-virtual {v1}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v2

    .end local p0    # "this":Landroid/hardware/display/DisplayManagerGlobal;
    throw v2

    .line 472
    .end local v1    # "ex":Landroid/os/RemoteException;
    .restart local p0    # "this":Landroid/hardware/display/DisplayManagerGlobal;
    :cond_1a
    :goto_1a
    monitor-exit v0

    .line 473
    return-void

    .line 472
    :catchall_1c
    move-exception v1

    monitor-exit v0
    :try_end_1e
    .catchall {:try_start_15 .. :try_end_1e} :catchall_1c

    throw v1
.end method

.method public stopWifiDisplayScan()V
    .registers 5

    .line 476
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mLock:Ljava/lang/Object;

    monitor-enter v0

    .line 477
    :try_start_3
    iget v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mWifiDisplayScanNestCount:I

    add-int/lit8 v1, v1, -0x1

    iput v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mWifiDisplayScanNestCount:I
    :try_end_9
    .catchall {:try_start_3 .. :try_end_9} :catchall_38

    if-nez v1, :cond_17

    .line 479
    :try_start_b
    iget-object v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDm:Landroid/hardware/display/IDisplayManager;

    invoke-interface {v1}, Landroid/hardware/display/IDisplayManager;->stopWifiDisplayScan()V
    :try_end_10
    .catch Landroid/os/RemoteException; {:try_start_b .. :try_end_10} :catch_11
    .catchall {:try_start_b .. :try_end_10} :catchall_38

    .line 482
    goto :goto_36

    .line 480
    :catch_11
    move-exception v1

    .line 481
    .local v1, "ex":Landroid/os/RemoteException;
    :try_start_12
    invoke-virtual {v1}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v2

    .end local p0    # "this":Landroid/hardware/display/DisplayManagerGlobal;
    throw v2

    .line 483
    .end local v1    # "ex":Landroid/os/RemoteException;
    .restart local p0    # "this":Landroid/hardware/display/DisplayManagerGlobal;
    :cond_17
    if-gez v1, :cond_36

    .line 484
    const-string v1, "DisplayManager"

    new-instance v2, Ljava/lang/StringBuilder;

    invoke-direct {v2}, Ljava/lang/StringBuilder;-><init>()V

    const-string v3, "Wifi display scan nest count became negative: "

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v2

    iget v3, p0, Landroid/hardware/display/DisplayManagerGlobal;->mWifiDisplayScanNestCount:I

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v2

    invoke-virtual {v2}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v2

    invoke-static {v1, v2}, Landroid/util/Log;->wtf(Ljava/lang/String;Ljava/lang/String;)I

    .line 486
    const/4 v1, 0x0

    iput v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mWifiDisplayScanNestCount:I

    .line 488
    :cond_36
    :goto_36
    monitor-exit v0

    .line 489
    return-void

    .line 488
    :catchall_38
    move-exception v1

    monitor-exit v0
    :try_end_3a
    .catchall {:try_start_12 .. :try_end_3a} :catchall_38

    throw v1
.end method

.method public unregisterDisplayListener(Landroid/hardware/display/DisplayManager$DisplayListener;)V
    .registers 6
    .param p1, "listener"    # Landroid/hardware/display/DisplayManager$DisplayListener;

    .line 357
    if-eqz p1, :cond_23

    .line 361
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mLock:Ljava/lang/Object;

    monitor-enter v0

    .line 362
    :try_start_5
    invoke-direct {p0, p1}, Landroid/hardware/display/DisplayManagerGlobal;->findDisplayListenerLocked(Landroid/hardware/display/DisplayManager$DisplayListener;)I

    move-result v1

    .line 363
    .local v1, "index":I
    if-ltz v1, :cond_1e

    .line 364
    iget-object v2, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayListeners:Ljava/util/concurrent/CopyOnWriteArrayList;

    invoke-virtual {v2, v1}, Ljava/util/concurrent/CopyOnWriteArrayList;->get(I)Ljava/lang/Object;

    move-result-object v2

    check-cast v2, Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;

    .line 365
    .local v2, "d":Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;
    invoke-virtual {v2}, Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;->clearEvents()V

    .line 366
    iget-object v3, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDisplayListeners:Ljava/util/concurrent/CopyOnWriteArrayList;

    invoke-virtual {v3, v1}, Ljava/util/concurrent/CopyOnWriteArrayList;->remove(I)Ljava/lang/Object;

    .line 367
    invoke-direct {p0}, Landroid/hardware/display/DisplayManagerGlobal;->updateCallbackIfNeededLocked()V

    .line 369
    .end local v1    # "index":I
    .end local v2    # "d":Landroid/hardware/display/DisplayManagerGlobal$DisplayListenerDelegate;
    :cond_1e
    monitor-exit v0

    .line 370
    return-void

    .line 369
    :catchall_20
    move-exception v1

    monitor-exit v0
    :try_end_22
    .catchall {:try_start_5 .. :try_end_22} :catchall_20

    throw v1

    .line 358
    :cond_23
    new-instance v0, Ljava/lang/IllegalArgumentException;

    const-string v1, "listener must not be null"

    invoke-direct {v0, v1}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method public unregisterNativeChoreographerForRefreshRateCallbacks()V
    .registers 3

    .line 1287
    iget-object v0, p0, Landroid/hardware/display/DisplayManagerGlobal;->mLock:Ljava/lang/Object;

    monitor-enter v0

    .line 1288
    const/4 v1, 0x0

    :try_start_4
    iput-boolean v1, p0, Landroid/hardware/display/DisplayManagerGlobal;->mDispatchNativeCallbacks:Z

    .line 1289
    invoke-direct {p0}, Landroid/hardware/display/DisplayManagerGlobal;->updateCallbackIfNeededLocked()V

    .line 1290
    monitor-exit v0

    .line 1291
    return-void

    .line 1290
    :catchall_b
    move-exception v1

    monitor-exit v0
    :try_end_d
    .catchall {:try_start_4 .. :try_end_d} :catchall_b

    throw v1
.end method
