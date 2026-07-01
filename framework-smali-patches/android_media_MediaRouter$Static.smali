.class Landroid/media/MediaRouter$Static;
.super Ljava/lang/Object;
.source "MediaRouter.java"

# interfaces
.implements Landroid/hardware/display/DisplayManager$DisplayListener;


# annotations
.annotation system Ldalvik/annotation/EnclosingClass;
    value = Landroid/media/MediaRouter;
.end annotation

.annotation system Ldalvik/annotation/InnerClass;
    accessFlags = 0x8
    name = "Static"
.end annotation

.annotation system Ldalvik/annotation/MemberClasses;
    value = {
        Landroid/media/MediaRouter$Static$Client;
    }
.end annotation


# instance fields
.field mActivelyScanningWifiDisplays:Z

.field final mAudioRoutesObserver:Landroid/media/IAudioRoutesObserver$Stub;

.field final mAudioService:Landroid/media/IAudioService;

.field mBluetoothA2dpRoute:Landroid/media/MediaRouter$RouteInfo;

.field final mCallbacks:Ljava/util/concurrent/CopyOnWriteArrayList;
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "Ljava/util/concurrent/CopyOnWriteArrayList<",
            "Landroid/media/MediaRouter$CallbackInfo;",
            ">;"
        }
    .end annotation
.end field

.field final mCanConfigureWifiDisplays:Z

.field final mCategories:Ljava/util/ArrayList;
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "Ljava/util/ArrayList<",
            "Landroid/media/MediaRouter$RouteCategory;",
            ">;"
        }
    .end annotation
.end field

.field mClient:Landroid/media/IMediaRouterClient;

.field mClientState:Landroid/media/MediaRouterClientState;

.field final mCurAudioRoutesInfo:Landroid/media/AudioRoutesInfo;

.field mCurrentUserId:I

.field mDefaultAudioVideo:Landroid/media/MediaRouter$RouteInfo;

.field mDiscoverRequestActiveScan:Z

.field mDiscoveryRequestRouteTypes:I

.field final mDisplayService:Landroid/hardware/display/DisplayManager;

.field final mHandler:Landroid/os/Handler;

.field mIsBluetoothA2dpOn:Z

.field final mMediaRouterService:Landroid/media/IMediaRouterService;

.field final mPackageName:Ljava/lang/String;

.field mPreviousActiveWifiDisplayAddress:Ljava/lang/String;

.field final mResources:Landroid/content/res/Resources;

.field final mRoutes:Ljava/util/ArrayList;
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "Ljava/util/ArrayList<",
            "Landroid/media/MediaRouter$RouteInfo;",
            ">;"
        }
    .end annotation
.end field

.field mSelectedRoute:Landroid/media/MediaRouter$RouteInfo;

.field mStreamVolume:Landroid/util/SparseIntArray;

.field final mSystemCategory:Landroid/media/MediaRouter$RouteCategory;


# direct methods
.method constructor <init>(Landroid/content/Context;)V
    .registers 8
    .param p1, "appContext"    # Landroid/content/Context;

    .line 138
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 91
    new-instance v0, Ljava/util/concurrent/CopyOnWriteArrayList;

    invoke-direct {v0}, Ljava/util/concurrent/CopyOnWriteArrayList;-><init>()V

    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mCallbacks:Ljava/util/concurrent/CopyOnWriteArrayList;

    .line 94
    new-instance v0, Ljava/util/ArrayList;

    invoke-direct {v0}, Ljava/util/ArrayList;-><init>()V

    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mRoutes:Ljava/util/ArrayList;

    .line 95
    new-instance v0, Ljava/util/ArrayList;

    invoke-direct {v0}, Ljava/util/ArrayList;-><init>()V

    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mCategories:Ljava/util/ArrayList;

    .line 99
    new-instance v0, Landroid/media/AudioRoutesInfo;

    invoke-direct {v0}, Landroid/media/AudioRoutesInfo;-><init>()V

    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mCurAudioRoutesInfo:Landroid/media/AudioRoutesInfo;

    .line 114
    const/4 v0, -0x1

    iput v0, p0, Landroid/media/MediaRouter$Static;->mCurrentUserId:I

    .line 118
    new-instance v0, Landroid/util/SparseIntArray;

    invoke-direct {v0}, Landroid/util/SparseIntArray;-><init>()V

    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mStreamVolume:Landroid/util/SparseIntArray;

    .line 120
    new-instance v0, Landroid/media/MediaRouter$Static$1;

    invoke-direct {v0, p0}, Landroid/media/MediaRouter$Static$1;-><init>(Landroid/media/MediaRouter$Static;)V

    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mAudioRoutesObserver:Landroid/media/IAudioRoutesObserver$Stub;

    .line 139
    invoke-virtual {p1}, Landroid/content/Context;->getPackageName()Ljava/lang/String;

    move-result-object v0

    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mPackageName:Ljava/lang/String;

    .line 140
    invoke-virtual {p1}, Landroid/content/Context;->getResources()Landroid/content/res/Resources;

    move-result-object v0

    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mResources:Landroid/content/res/Resources;

    .line 141
    new-instance v0, Landroid/os/Handler;

    invoke-virtual {p1}, Landroid/content/Context;->getMainLooper()Landroid/os/Looper;

    move-result-object v1

    invoke-direct {v0, v1}, Landroid/os/Handler;-><init>(Landroid/os/Looper;)V

    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mHandler:Landroid/os/Handler;

    .line 143
    const-string v0, "audio"

    invoke-static {v0}, Landroid/os/ServiceManager;->getService(Ljava/lang/String;)Landroid/os/IBinder;

    move-result-object v0

    .line 144
    .local v0, "b":Landroid/os/IBinder;
    invoke-static {v0}, Landroid/media/IAudioService$Stub;->asInterface(Landroid/os/IBinder;)Landroid/media/IAudioService;

    move-result-object v1

    iput-object v1, p0, Landroid/media/MediaRouter$Static;->mAudioService:Landroid/media/IAudioService;

    .line 146
    const-string v1, "display"

    invoke-virtual {p1, v1}, Landroid/content/Context;->getSystemService(Ljava/lang/String;)Ljava/lang/Object;

    move-result-object v1

    check-cast v1, Landroid/hardware/display/DisplayManager;

    iput-object v1, p0, Landroid/media/MediaRouter$Static;->mDisplayService:Landroid/hardware/display/DisplayManager;

    .line 148
    nop

    .line 149
    const-string v1, "media_router"

    invoke-static {v1}, Landroid/os/ServiceManager;->getService(Ljava/lang/String;)Landroid/os/IBinder;

    move-result-object v1

    .line 148
    invoke-static {v1}, Landroid/media/IMediaRouterService$Stub;->asInterface(Landroid/os/IBinder;)Landroid/media/IMediaRouterService;

    move-result-object v1

    if-nez v1, :cond_73

    new-instance v1, Landroid/os/Binder;

    invoke-direct {v1}, Landroid/os/Binder;-><init>()V

    invoke-static {v1}, Landroid/media/IMediaRouterService$Stub;->asInterface(Landroid/os/IBinder;)Landroid/media/IMediaRouterService;

    move-result-object v1

    :cond_73
    iput-object v1, p0, Landroid/media/MediaRouter$Static;->mMediaRouterService:Landroid/media/IMediaRouterService;

    .line 151
    new-instance v1, Landroid/media/MediaRouter$RouteCategory;

    const v2, 0x10402f2

    const/4 v3, 0x3

    const/4 v4, 0x0

    invoke-direct {v1, v2, v3, v4}, Landroid/media/MediaRouter$RouteCategory;-><init>(IIZ)V

    iput-object v1, p0, Landroid/media/MediaRouter$Static;->mSystemCategory:Landroid/media/MediaRouter$RouteCategory;

    .line 154
    const/4 v2, 0x1

    iput-boolean v2, v1, Landroid/media/MediaRouter$RouteCategory;->mIsSystem:Z

    .line 159
    nop

    .line 161
    invoke-static {}, Landroid/os/Process;->myPid()I

    move-result v1

    invoke-static {}, Landroid/os/Process;->myUid()I

    move-result v3

    .line 159
    const-string v5, "android.permission.CONFIGURE_WIFI_DISPLAY"

    invoke-virtual {p1, v5, v1, v3}, Landroid/content/Context;->checkPermission(Ljava/lang/String;II)I

    move-result v1

    if-nez v1, :cond_96

    move v4, v2

    :cond_96
    iput-boolean v4, p0, Landroid/media/MediaRouter$Static;->mCanConfigureWifiDisplays:Z

    .line 162
    return-void
.end method

.method private updatePresentationDisplays(I)V
    .registers 6
    .param p1, "changedDisplayId"    # I

    .line 405
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mRoutes:Ljava/util/ArrayList;

    invoke-virtual {v0}, Ljava/util/ArrayList;->size()I

    move-result v0

    .line 406
    .local v0, "count":I
    const/4 v1, 0x0

    .local v1, "i":I
    :goto_7
    if-ge v1, v0, :cond_29

    .line 407
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mRoutes:Ljava/util/ArrayList;

    invoke-virtual {v2, v1}, Ljava/util/ArrayList;->get(I)Ljava/lang/Object;

    move-result-object v2

    check-cast v2, Landroid/media/MediaRouter$RouteInfo;

    .line 408
    .local v2, "route":Landroid/media/MediaRouter$RouteInfo;
    invoke-virtual {v2}, Landroid/media/MediaRouter$RouteInfo;->updatePresentationDisplay()Z

    move-result v3

    if-nez v3, :cond_23

    iget-object v3, v2, Landroid/media/MediaRouter$RouteInfo;->mPresentationDisplay:Landroid/view/Display;

    if-eqz v3, :cond_26

    iget-object v3, v2, Landroid/media/MediaRouter$RouteInfo;->mPresentationDisplay:Landroid/view/Display;

    .line 409
    invoke-virtual {v3}, Landroid/view/Display;->getDisplayId()I

    move-result v3

    if-ne v3, p1, :cond_26

    .line 410
    :cond_23
    invoke-static {v2}, Landroid/media/MediaRouter;->dispatchRoutePresentationDisplayChanged(Landroid/media/MediaRouter$RouteInfo;)V

    .line 406
    .end local v2    # "route":Landroid/media/MediaRouter$RouteInfo;
    :cond_26
    add-int/lit8 v1, v1, 0x1

    goto :goto_7

    .line 413
    .end local v1    # "i":I
    :cond_29
    return-void
.end method


# virtual methods
.method findGlobalRoute(Ljava/lang/String;)Landroid/media/MediaRouter$RouteInfo;
    .registers 6
    .param p1, "globalRouteId"    # Ljava/lang/String;

    .line 641
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mRoutes:Ljava/util/ArrayList;

    invoke-virtual {v0}, Ljava/util/ArrayList;->size()I

    move-result v0

    .line 642
    .local v0, "count":I
    const/4 v1, 0x0

    .local v1, "i":I
    :goto_7
    if-ge v1, v0, :cond_1d

    .line 643
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mRoutes:Ljava/util/ArrayList;

    invoke-virtual {v2, v1}, Ljava/util/ArrayList;->get(I)Ljava/lang/Object;

    move-result-object v2

    check-cast v2, Landroid/media/MediaRouter$RouteInfo;

    .line 644
    .local v2, "route":Landroid/media/MediaRouter$RouteInfo;
    iget-object v3, v2, Landroid/media/MediaRouter$RouteInfo;->mGlobalRouteId:Ljava/lang/String;

    invoke-virtual {p1, v3}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v3

    if-eqz v3, :cond_1a

    .line 645
    return-object v2

    .line 642
    .end local v2    # "route":Landroid/media/MediaRouter$RouteInfo;
    :cond_1a
    add-int/lit8 v1, v1, 0x1

    goto :goto_7

    .line 648
    .end local v1    # "i":I
    :cond_1d
    const/4 v1, 0x0

    return-object v1
.end method

.method public getAllPresentationDisplays()[Landroid/view/Display;
    .registers 4

    .line 397
    :try_start_0
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mDisplayService:Landroid/hardware/display/DisplayManager;

    const-string v1, "android.hardware.display.category.PRESENTATION"

    invoke-virtual {v0, v1}, Landroid/hardware/display/DisplayManager;->getDisplays(Ljava/lang/String;)[Landroid/view/Display;

    move-result-object v0
    :try_end_8
    .catch Ljava/lang/RuntimeException; {:try_start_0 .. :try_end_8} :catch_9

    return-object v0

    .line 398
    :catch_9
    move-exception v0

    .line 399
    .local v0, "ex":Ljava/lang/RuntimeException;
    const-string v1, "MediaRouter"

    const-string v2, "Unable to get displays."

    invoke-static {v1, v2, v0}, Landroid/util/Log;->e(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 400
    const/4 v1, 0x0

    return-object v1
.end method

.method getStreamVolume(I)I
    .registers 7
    .param p1, "streamType"    # I

    .line 284
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mStreamVolume:Landroid/util/SparseIntArray;

    invoke-virtual {v0, p1}, Landroid/util/SparseIntArray;->indexOfKey(I)I

    move-result v0

    .line 285
    .local v0, "idx":I
    if-gez v0, :cond_22

    .line 286
    const/4 v1, 0x0

    .line 288
    .local v1, "volume":I
    :try_start_9
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mAudioService:Landroid/media/IAudioService;

    invoke-interface {v2, p1}, Landroid/media/IAudioService;->getStreamVolume(I)I

    move-result v2

    move v1, v2

    .line 289
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mStreamVolume:Landroid/util/SparseIntArray;

    invoke-virtual {v2, p1, v1}, Landroid/util/SparseIntArray;->put(II)V
    :try_end_15
    .catch Landroid/os/RemoteException; {:try_start_9 .. :try_end_15} :catch_18
    .catchall {:try_start_9 .. :try_end_15} :catchall_16

    .line 293
    return v1

    :catchall_16
    move-exception v2

    goto :goto_21

    .line 290
    :catch_18
    move-exception v2

    .line 291
    .local v2, "e":Landroid/os/RemoteException;
    :try_start_19
    const-string v3, "MediaRouter"

    const-string v4, "Error getting local stream volume"

    invoke-static {v3, v4, v2}, Landroid/util/Log;->e(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I
    :try_end_20
    .catchall {:try_start_19 .. :try_end_20} :catchall_16

    .line 293
    nop

    .end local v2    # "e":Landroid/os/RemoteException;
    :goto_21
    return v1

    .line 296
    .end local v1    # "volume":I
    :cond_22
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mStreamVolume:Landroid/util/SparseIntArray;

    invoke-virtual {v1, v0}, Landroid/util/SparseIntArray;->valueAt(I)I

    move-result v1

    return v1
.end method

.method handleGroupRouteSelected(Ljava/lang/String;)V
    .registers 7
    .param p1, "routeId"    # Ljava/lang/String;

    .line 416
    invoke-virtual {p0}, Landroid/media/MediaRouter$Static;->isBluetoothA2dpOn()Z

    move-result v0

    if-eqz v0, :cond_9

    .line 417
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mBluetoothA2dpRoute:Landroid/media/MediaRouter$RouteInfo;

    goto :goto_b

    :cond_9
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mDefaultAudioVideo:Landroid/media/MediaRouter$RouteInfo;

    .line 418
    .local v0, "routeToSelect":Landroid/media/MediaRouter$RouteInfo;
    :goto_b
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mRoutes:Ljava/util/ArrayList;

    invoke-virtual {v1}, Ljava/util/ArrayList;->size()I

    move-result v1

    .line 419
    .local v1, "count":I
    const/4 v2, 0x0

    .local v2, "i":I
    :goto_12
    if-ge v2, v1, :cond_28

    .line 420
    iget-object v3, p0, Landroid/media/MediaRouter$Static;->mRoutes:Ljava/util/ArrayList;

    invoke-virtual {v3, v2}, Ljava/util/ArrayList;->get(I)Ljava/lang/Object;

    move-result-object v3

    check-cast v3, Landroid/media/MediaRouter$RouteInfo;

    .line 421
    .local v3, "route":Landroid/media/MediaRouter$RouteInfo;
    iget-object v4, v3, Landroid/media/MediaRouter$RouteInfo;->mGlobalRouteId:Ljava/lang/String;

    invoke-static {v4, p1}, Landroid/text/TextUtils;->equals(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Z

    move-result v4

    if-eqz v4, :cond_25

    .line 422
    move-object v0, v3

    .line 419
    .end local v3    # "route":Landroid/media/MediaRouter$RouteInfo;
    :cond_25
    add-int/lit8 v2, v2, 0x1

    goto :goto_12

    .line 425
    .end local v2    # "i":I
    :cond_28
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mSelectedRoute:Landroid/media/MediaRouter$RouteInfo;

    if-eq v0, v2, :cond_32

    .line 426
    iget v2, v0, Landroid/media/MediaRouter$RouteInfo;->mSupportedTypes:I

    const/4 v3, 0x0

    invoke-static {v2, v0, v3}, Landroid/media/MediaRouter;->selectRouteStatic(ILandroid/media/MediaRouter$RouteInfo;Z)V

    .line 428
    :cond_32
    return-void
.end method

.method isBluetoothA2dpOn()Z
    .registers 2

    .line 300
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mBluetoothA2dpRoute:Landroid/media/MediaRouter$RouteInfo;

    if-eqz v0, :cond_a

    iget-boolean v0, p0, Landroid/media/MediaRouter$Static;->mIsBluetoothA2dpOn:Z

    if-eqz v0, :cond_a

    const/4 v0, 0x1

    goto :goto_b

    :cond_a
    const/4 v0, 0x0

    :goto_b
    return v0
.end method

.method isPlaybackActive()Z
    .registers 3

    .line 652
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mClient:Landroid/media/IMediaRouterClient;

    if-eqz v0, :cond_f

    .line 654
    :try_start_4
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mMediaRouterService:Landroid/media/IMediaRouterService;

    invoke-interface {v1, v0}, Landroid/media/IMediaRouterService;->isPlaybackActive(Landroid/media/IMediaRouterClient;)Z

    move-result v0
    :try_end_a
    .catch Landroid/os/RemoteException; {:try_start_4 .. :try_end_a} :catch_b

    return v0

    .line 655
    :catch_b
    move-exception v0

    .line 656
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    .line 659
    .end local v0    # "ex":Landroid/os/RemoteException;
    :cond_f
    const/4 v0, 0x0

    return v0
.end method

.method makeGlobalRoute(Landroid/media/MediaRouterClientState$RouteInfo;)Landroid/media/MediaRouter$RouteInfo;
    .registers 4
    .param p1, "globalRoute"    # Landroid/media/MediaRouterClientState$RouteInfo;

    .line 555
    new-instance v0, Landroid/media/MediaRouter$RouteInfo;

    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mSystemCategory:Landroid/media/MediaRouter$RouteCategory;

    invoke-direct {v0, v1}, Landroid/media/MediaRouter$RouteInfo;-><init>(Landroid/media/MediaRouter$RouteCategory;)V

    .line 556
    .local v0, "route":Landroid/media/MediaRouter$RouteInfo;
    iget-object v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->id:Ljava/lang/String;

    iput-object v1, v0, Landroid/media/MediaRouter$RouteInfo;->mGlobalRouteId:Ljava/lang/String;

    .line 557
    iget-object v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->name:Ljava/lang/String;

    iput-object v1, v0, Landroid/media/MediaRouter$RouteInfo;->mName:Ljava/lang/CharSequence;

    .line 558
    iget-object v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->description:Ljava/lang/String;

    iput-object v1, v0, Landroid/media/MediaRouter$RouteInfo;->mDescription:Ljava/lang/CharSequence;

    .line 559
    iget v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->supportedTypes:I

    iput v1, v0, Landroid/media/MediaRouter$RouteInfo;->mSupportedTypes:I

    .line 560
    iget v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->deviceType:I

    iput v1, v0, Landroid/media/MediaRouter$RouteInfo;->mDeviceType:I

    .line 561
    iget-boolean v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->enabled:Z

    iput-boolean v1, v0, Landroid/media/MediaRouter$RouteInfo;->mEnabled:Z

    .line 562
    iget v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->statusCode:I

    invoke-virtual {v0, v1}, Landroid/media/MediaRouter$RouteInfo;->setRealStatusCode(I)Z

    .line 563
    iget v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->playbackType:I

    iput v1, v0, Landroid/media/MediaRouter$RouteInfo;->mPlaybackType:I

    .line 564
    iget v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->playbackStream:I

    iput v1, v0, Landroid/media/MediaRouter$RouteInfo;->mPlaybackStream:I

    .line 565
    iget v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->volume:I

    iput v1, v0, Landroid/media/MediaRouter$RouteInfo;->mVolume:I

    .line 566
    iget v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->volumeMax:I

    iput v1, v0, Landroid/media/MediaRouter$RouteInfo;->mVolumeMax:I

    .line 567
    iget v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->volumeHandling:I

    iput v1, v0, Landroid/media/MediaRouter$RouteInfo;->mVolumeHandling:I

    .line 568
    iget v1, p1, Landroid/media/MediaRouterClientState$RouteInfo;->presentationDisplayId:I

    iput v1, v0, Landroid/media/MediaRouter$RouteInfo;->mPresentationDisplayId:I

    .line 569
    invoke-virtual {v0}, Landroid/media/MediaRouter$RouteInfo;->updatePresentationDisplay()Z

    .line 570
    return-object v0
.end method

.method public onDisplayAdded(I)V
    .registers 2
    .param p1, "displayId"    # I

    .line 372
    invoke-direct {p0, p1}, Landroid/media/MediaRouter$Static;->updatePresentationDisplays(I)V

    .line 373
    return-void
.end method

.method public onDisplayChanged(I)V
    .registers 2
    .param p1, "displayId"    # I

    .line 377
    invoke-direct {p0, p1}, Landroid/media/MediaRouter$Static;->updatePresentationDisplays(I)V

    .line 378
    return-void
.end method

.method public onDisplayRemoved(I)V
    .registers 2
    .param p1, "displayId"    # I

    .line 382
    invoke-direct {p0, p1}, Landroid/media/MediaRouter$Static;->updatePresentationDisplays(I)V

    .line 383
    return-void
.end method

.method publishClientDiscoveryRequest()V
    .registers 5

    .line 464
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mClient:Landroid/media/IMediaRouterClient;

    if-eqz v0, :cond_12

    .line 466
    :try_start_4
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mMediaRouterService:Landroid/media/IMediaRouterService;

    iget v2, p0, Landroid/media/MediaRouter$Static;->mDiscoveryRequestRouteTypes:I

    iget-boolean v3, p0, Landroid/media/MediaRouter$Static;->mDiscoverRequestActiveScan:Z

    invoke-interface {v1, v0, v2, v3}, Landroid/media/IMediaRouterService;->setDiscoveryRequest(Landroid/media/IMediaRouterClient;IZ)V
    :try_end_d
    .catch Landroid/os/RemoteException; {:try_start_4 .. :try_end_d} :catch_e

    .line 470
    goto :goto_12

    .line 468
    :catch_e
    move-exception v0

    .line 469
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    .line 472
    .end local v0    # "ex":Landroid/os/RemoteException;
    :cond_12
    :goto_12
    return-void
.end method

.method publishClientSelectedRoute(Z)V
    .registers 5
    .param p1, "explicit"    # Z

    .line 475
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mClient:Landroid/media/IMediaRouterClient;

    if-eqz v0, :cond_16

    .line 477
    :try_start_4
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mMediaRouterService:Landroid/media/IMediaRouterService;

    .line 478
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mSelectedRoute:Landroid/media/MediaRouter$RouteInfo;

    if-eqz v2, :cond_d

    iget-object v2, v2, Landroid/media/MediaRouter$RouteInfo;->mGlobalRouteId:Ljava/lang/String;

    goto :goto_e

    :cond_d
    const/4 v2, 0x0

    .line 477
    :goto_e
    invoke-interface {v1, v0, v2, p1}, Landroid/media/IMediaRouterService;->setSelectedRoute(Landroid/media/IMediaRouterClient;Ljava/lang/String;Z)V
    :try_end_11
    .catch Landroid/os/RemoteException; {:try_start_4 .. :try_end_11} :catch_12

    .line 482
    goto :goto_16

    .line 480
    :catch_12
    move-exception v0

    .line 481
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    .line 484
    .end local v0    # "ex":Landroid/os/RemoteException;
    :cond_16
    :goto_16
    return-void
.end method

.method rebindAsUser(I)V
    .registers 5
    .param p1, "userId"    # I

    .line 437
    iget v0, p0, Landroid/media/MediaRouter$Static;->mCurrentUserId:I

    if-ne v0, p1, :cond_a

    if-ltz p1, :cond_a

    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mClient:Landroid/media/IMediaRouterClient;

    if-nez v0, :cond_3e

    .line 438
    :cond_a
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mClient:Landroid/media/IMediaRouterClient;

    if-eqz v0, :cond_1b

    .line 440
    :try_start_e
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mMediaRouterService:Landroid/media/IMediaRouterService;

    invoke-interface {v1, v0}, Landroid/media/IMediaRouterService;->unregisterClient(Landroid/media/IMediaRouterClient;)V
    :try_end_13
    .catch Landroid/os/RemoteException; {:try_start_e .. :try_end_13} :catch_14

    .line 443
    goto :goto_18

    .line 441
    :catch_14
    move-exception v0

    .line 442
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    .line 444
    .end local v0    # "ex":Landroid/os/RemoteException;
    :goto_18
    const/4 v0, 0x0

    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mClient:Landroid/media/IMediaRouterClient;

    .line 447
    :cond_1b
    iput p1, p0, Landroid/media/MediaRouter$Static;->mCurrentUserId:I

    .line 450
    :try_start_1d
    new-instance v0, Landroid/media/MediaRouter$Static$Client;

    invoke-direct {v0, p0}, Landroid/media/MediaRouter$Static$Client;-><init>(Landroid/media/MediaRouter$Static;)V

    .line 451
    .local v0, "client":Landroid/media/MediaRouter$Static$Client;
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mMediaRouterService:Landroid/media/IMediaRouterService;

    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mPackageName:Ljava/lang/String;

    invoke-interface {v1, v0, v2, p1}, Landroid/media/IMediaRouterService;->registerClientAsUser(Landroid/media/IMediaRouterClient;Ljava/lang/String;I)V

    .line 452
    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mClient:Landroid/media/IMediaRouterClient;
    :try_end_2b
    .catch Landroid/os/RemoteException; {:try_start_1d .. :try_end_2b} :catch_2c

    .line 455
    .end local v0    # "client":Landroid/media/MediaRouter$Static$Client;
    goto :goto_34

    .line 453
    :catch_2c
    move-exception v0

    .line 454
    .local v0, "ex":Landroid/os/RemoteException;
    const-string v1, "MediaRouter"

    const-string v2, "Unable to register media router client."

    invoke-static {v1, v2, v0}, Landroid/util/Log;->e(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 457
    .end local v0    # "ex":Landroid/os/RemoteException;
    :goto_34
    invoke-virtual {p0}, Landroid/media/MediaRouter$Static;->publishClientDiscoveryRequest()V

    .line 458
    const/4 v0, 0x0

    invoke-virtual {p0, v0}, Landroid/media/MediaRouter$Static;->publishClientSelectedRoute(Z)V

    .line 459
    invoke-virtual {p0}, Landroid/media/MediaRouter$Static;->updateClientState()V

    .line 461
    :cond_3e
    return-void
.end method

.method requestSetVolume(Landroid/media/MediaRouter$RouteInfo;I)V
    .registers 6
    .param p1, "route"    # Landroid/media/MediaRouter$RouteInfo;
    .param p2, "volume"    # I

    .line 533
    iget-object v0, p1, Landroid/media/MediaRouter$RouteInfo;->mGlobalRouteId:Ljava/lang/String;

    if-eqz v0, :cond_14

    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mClient:Landroid/media/IMediaRouterClient;

    if-eqz v0, :cond_14

    .line 535
    :try_start_8
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mMediaRouterService:Landroid/media/IMediaRouterService;

    iget-object v2, p1, Landroid/media/MediaRouter$RouteInfo;->mGlobalRouteId:Ljava/lang/String;

    invoke-interface {v1, v0, v2, p2}, Landroid/media/IMediaRouterService;->requestSetVolume(Landroid/media/IMediaRouterClient;Ljava/lang/String;I)V
    :try_end_f
    .catch Landroid/os/RemoteException; {:try_start_8 .. :try_end_f} :catch_10

    .line 539
    goto :goto_14

    .line 537
    :catch_10
    move-exception v0

    .line 538
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    .line 541
    .end local v0    # "ex":Landroid/os/RemoteException;
    :cond_14
    :goto_14
    return-void
.end method

.method requestUpdateVolume(Landroid/media/MediaRouter$RouteInfo;I)V
    .registers 6
    .param p1, "route"    # Landroid/media/MediaRouter$RouteInfo;
    .param p2, "direction"    # I

    .line 544
    iget-object v0, p1, Landroid/media/MediaRouter$RouteInfo;->mGlobalRouteId:Ljava/lang/String;

    if-eqz v0, :cond_14

    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mClient:Landroid/media/IMediaRouterClient;

    if-eqz v0, :cond_14

    .line 546
    :try_start_8
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mMediaRouterService:Landroid/media/IMediaRouterService;

    iget-object v2, p1, Landroid/media/MediaRouter$RouteInfo;->mGlobalRouteId:Ljava/lang/String;

    invoke-interface {v1, v0, v2, p2}, Landroid/media/IMediaRouterService;->requestUpdateVolume(Landroid/media/IMediaRouterClient;Ljava/lang/String;I)V
    :try_end_f
    .catch Landroid/os/RemoteException; {:try_start_8 .. :try_end_f} :catch_10

    .line 550
    goto :goto_14

    .line 548
    :catch_10
    move-exception v0

    .line 549
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    .line 552
    .end local v0    # "ex":Landroid/os/RemoteException;
    :cond_14
    :goto_14
    return-void
.end method

.method public setRouterGroupId(Ljava/lang/String;)V
    .registers 4
    .param p1, "groupId"    # Ljava/lang/String;

    .line 386
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mClient:Landroid/media/IMediaRouterClient;

    if-eqz v0, :cond_e

    .line 388
    :try_start_4
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mMediaRouterService:Landroid/media/IMediaRouterService;

    invoke-interface {v1, v0, p1}, Landroid/media/IMediaRouterService;->registerClientGroupId(Landroid/media/IMediaRouterClient;Ljava/lang/String;)V
    :try_end_9
    .catch Landroid/os/RemoteException; {:try_start_4 .. :try_end_9} :catch_a

    .line 391
    goto :goto_e

    .line 389
    :catch_a
    move-exception v0

    .line 390
    .local v0, "ex":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    .line 393
    .end local v0    # "ex":Landroid/os/RemoteException;
    :cond_e
    :goto_e
    return-void
.end method

.method setSelectedRoute(Landroid/media/MediaRouter$RouteInfo;Z)V
    .registers 3
    .param p1, "info"    # Landroid/media/MediaRouter$RouteInfo;
    .param p2, "explicit"    # Z

    .line 432
    iput-object p1, p0, Landroid/media/MediaRouter$Static;->mSelectedRoute:Landroid/media/MediaRouter$RouteInfo;

    .line 433
    invoke-virtual {p0, p2}, Landroid/media/MediaRouter$Static;->publishClientSelectedRoute(Z)V

    .line 434
    return-void
.end method

.method startMonitoringRoutes(Landroid/content/Context;)V
    .registers 5
    .param p1, "appContext"    # Landroid/content/Context;

    .line 166
    new-instance v0, Landroid/media/MediaRouter$RouteInfo;

    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mSystemCategory:Landroid/media/MediaRouter$RouteCategory;

    invoke-direct {v0, v1}, Landroid/media/MediaRouter$RouteInfo;-><init>(Landroid/media/MediaRouter$RouteCategory;)V

    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mDefaultAudioVideo:Landroid/media/MediaRouter$RouteInfo;

    .line 167
    const v1, 0x10402f4

    iput v1, v0, Landroid/media/MediaRouter$RouteInfo;->mNameResId:I

    .line 168
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mDefaultAudioVideo:Landroid/media/MediaRouter$RouteInfo;

    const/4 v1, 0x3

    iput v1, v0, Landroid/media/MediaRouter$RouteInfo;->mSupportedTypes:I

    .line 169
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mDefaultAudioVideo:Landroid/media/MediaRouter$RouteInfo;

    invoke-virtual {v0}, Landroid/media/MediaRouter$RouteInfo;->updatePresentationDisplay()Z

    .line 170
    const-string v0, "audio"

    invoke-virtual {p1, v0}, Landroid/content/Context;->getSystemService(Ljava/lang/String;)Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/media/AudioManager;

    .line 171
    invoke-virtual {v0}, Landroid/media/AudioManager;->isVolumeFixed()Z

    move-result v0

    if-eqz v0, :cond_2b

    .line 172
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mDefaultAudioVideo:Landroid/media/MediaRouter$RouteInfo;

    const/4 v1, 0x0

    iput v1, v0, Landroid/media/MediaRouter$RouteInfo;->mVolumeHandling:I

    .line 174
    :cond_2b
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mDefaultAudioVideo:Landroid/media/MediaRouter$RouteInfo;

    sget-object v1, Landroid/media/MediaRouter;->sStatic:Landroid/media/MediaRouter$Static;

    iget-object v1, v1, Landroid/media/MediaRouter$Static;->mResources:Landroid/content/res/Resources;

    const v2, 0x10402f3

    invoke-virtual {v1, v2}, Landroid/content/res/Resources;->getString(I)Ljava/lang/String;

    move-result-object v1

    iput-object v1, v0, Landroid/media/MediaRouter$RouteInfo;->mGlobalRouteId:Ljava/lang/String;

    .line 176
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mDefaultAudioVideo:Landroid/media/MediaRouter$RouteInfo;

    invoke-static {v0}, Landroid/media/MediaRouter;->addRouteStatic(Landroid/media/MediaRouter$RouteInfo;)V

    .line 179
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mDisplayService:Landroid/hardware/display/DisplayManager;

    invoke-virtual {v0}, Landroid/hardware/display/DisplayManager;->getWifiDisplayStatus()Landroid/hardware/display/WifiDisplayStatus;

    move-result-object v0

    invoke-static {v0}, Landroid/media/MediaRouter;->updateWifiDisplayStatus(Landroid/hardware/display/WifiDisplayStatus;)V

    .line 181
    new-instance v0, Landroid/media/MediaRouter$WifiDisplayStatusChangedReceiver;

    invoke-direct {v0}, Landroid/media/MediaRouter$WifiDisplayStatusChangedReceiver;-><init>()V

    new-instance v1, Landroid/content/IntentFilter;

    const-string v2, "android.hardware.display.action.WIFI_DISPLAY_STATUS_CHANGED"

    invoke-direct {v1, v2}, Landroid/content/IntentFilter;-><init>(Ljava/lang/String;)V

    invoke-virtual {p1, v0, v1}, Landroid/content/Context;->registerReceiver(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)Landroid/content/Intent;

    .line 183
    new-instance v0, Landroid/media/MediaRouter$VolumeChangeReceiver;

    invoke-direct {v0}, Landroid/media/MediaRouter$VolumeChangeReceiver;-><init>()V

    new-instance v1, Landroid/content/IntentFilter;

    const-string v2, "android.media.VOLUME_CHANGED_ACTION"

    invoke-direct {v1, v2}, Landroid/content/IntentFilter;-><init>(Ljava/lang/String;)V

    invoke-virtual {p1, v0, v1}, Landroid/content/Context;->registerReceiver(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)Landroid/content/Intent;

    .line 186
    iget-object v0, p0, Landroid/media/MediaRouter$Static;->mDisplayService:Landroid/hardware/display/DisplayManager;

    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mHandler:Landroid/os/Handler;

    invoke-virtual {v0, p0, v1}, Landroid/hardware/display/DisplayManager;->registerDisplayListener(Landroid/hardware/display/DisplayManager$DisplayListener;Landroid/os/Handler;)V

    .line 188
    const/4 v0, 0x0

    .line 190
    .local v0, "newAudioRoutes":Landroid/media/AudioRoutesInfo;
    :try_start_6e
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mAudioService:Landroid/media/IAudioService;

    invoke-interface {v1}, Landroid/media/IAudioService;->isBluetoothA2dpOn()Z

    move-result v1

    iput-boolean v1, p0, Landroid/media/MediaRouter$Static;->mIsBluetoothA2dpOn:Z

    .line 191
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mAudioService:Landroid/media/IAudioService;

    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mAudioRoutesObserver:Landroid/media/IAudioRoutesObserver$Stub;

    invoke-interface {v1, v2}, Landroid/media/IAudioService;->startWatchingRoutes(Landroid/media/IAudioRoutesObserver;)Landroid/media/AudioRoutesInfo;

    move-result-object v1
    :try_end_7e
    .catch Landroid/os/RemoteException; {:try_start_6e .. :try_end_7e} :catch_80

    move-object v0, v1

    .line 193
    goto :goto_81

    .line 192
    :catch_80
    move-exception v1

    .line 194
    :goto_81
    if-eqz v0, :cond_86

    .line 198
    invoke-virtual {p0, v0}, Landroid/media/MediaRouter$Static;->updateAudioRoutes(Landroid/media/AudioRoutesInfo;)V

    .line 202
    :cond_86
    invoke-static {}, Landroid/os/UserHandle;->myUserId()I

    move-result v1

    invoke-virtual {p0, v1}, Landroid/media/MediaRouter$Static;->rebindAsUser(I)V

    .line 206
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mSelectedRoute:Landroid/media/MediaRouter$RouteInfo;

    if-nez v1, :cond_94

    .line 207
    invoke-static {}, Landroid/media/MediaRouter;->selectDefaultRouteStatic()V

    .line 209
    :cond_94
    return-void
.end method

.method updateAudioRoutes(Landroid/media/AudioRoutesInfo;)V
    .registers 8
    .param p1, "newRoutes"    # Landroid/media/AudioRoutesInfo;

    .line 212
    const/4 v0, 0x0

    .line 213
    .local v0, "audioRoutesChanged":Z
    const/4 v1, 0x0

    .line 215
    .local v1, "forceUseDefaultRoute":Z
    iget v2, p1, Landroid/media/AudioRoutesInfo;->mainType:I

    iget-object v3, p0, Landroid/media/MediaRouter$Static;->mCurAudioRoutesInfo:Landroid/media/AudioRoutesInfo;

    iget v3, v3, Landroid/media/AudioRoutesInfo;->mainType:I

    const/4 v4, 0x1

    if-eq v2, v3, :cond_53

    .line 216
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mCurAudioRoutesInfo:Landroid/media/AudioRoutesInfo;

    iget v3, p1, Landroid/media/AudioRoutesInfo;->mainType:I

    iput v3, v2, Landroid/media/AudioRoutesInfo;->mainType:I

    .line 218
    iget v2, p1, Landroid/media/AudioRoutesInfo;->mainType:I

    and-int/lit8 v2, v2, 0x2

    if-nez v2, :cond_3f

    iget v2, p1, Landroid/media/AudioRoutesInfo;->mainType:I

    and-int/2addr v2, v4

    if-eqz v2, :cond_1d

    goto :goto_3f

    .line 221
    :cond_1d
    iget v2, p1, Landroid/media/AudioRoutesInfo;->mainType:I

    and-int/lit8 v2, v2, 0x4

    if-eqz v2, :cond_27

    .line 222
    const v2, 0x10402f5

    .local v2, "name":I
    goto :goto_42

    .line 223
    .end local v2    # "name":I
    :cond_27
    iget v2, p1, Landroid/media/AudioRoutesInfo;->mainType:I

    and-int/lit8 v2, v2, 0x8

    if-eqz v2, :cond_31

    .line 224
    const v2, 0x10402f6

    .restart local v2    # "name":I
    goto :goto_42

    .line 225
    .end local v2    # "name":I
    :cond_31
    iget v2, p1, Landroid/media/AudioRoutesInfo;->mainType:I

    and-int/lit8 v2, v2, 0x10

    if-eqz v2, :cond_3b

    .line 226
    const v2, 0x10402f8

    .restart local v2    # "name":I
    goto :goto_42

    .line 228
    .end local v2    # "name":I
    :cond_3b
    const v2, 0x10402f4

    .restart local v2    # "name":I
    goto :goto_42

    .line 220
    .end local v2    # "name":I
    :cond_3f
    :goto_3f
    const v2, 0x10402f7

    .line 230
    .restart local v2    # "name":I
    :goto_42
    iget-object v3, p0, Landroid/media/MediaRouter$Static;->mDefaultAudioVideo:Landroid/media/MediaRouter$RouteInfo;

    iput v2, v3, Landroid/media/MediaRouter$RouteInfo;->mNameResId:I

    .line 231
    iget-object v3, p0, Landroid/media/MediaRouter$Static;->mDefaultAudioVideo:Landroid/media/MediaRouter$RouteInfo;

    invoke-static {v3}, Landroid/media/MediaRouter;->dispatchRouteChanged(Landroid/media/MediaRouter$RouteInfo;)V

    .line 233
    iget v3, p1, Landroid/media/AudioRoutesInfo;->mainType:I

    and-int/lit8 v3, v3, 0x13

    if-eqz v3, :cond_52

    .line 235
    const/4 v1, 0x1

    .line 237
    :cond_52
    const/4 v0, 0x1

    .line 240
    .end local v2    # "name":I
    :cond_53
    iget-object v2, p1, Landroid/media/AudioRoutesInfo;->bluetoothName:Ljava/lang/CharSequence;

    iget-object v3, p0, Landroid/media/MediaRouter$Static;->mCurAudioRoutesInfo:Landroid/media/AudioRoutesInfo;

    iget-object v3, v3, Landroid/media/AudioRoutesInfo;->bluetoothName:Ljava/lang/CharSequence;

    invoke-static {v2, v3}, Landroid/text/TextUtils;->equals(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Z

    move-result v2

    if-nez v2, :cond_ad

    .line 241
    const/4 v1, 0x0

    .line 242
    iget-object v2, p1, Landroid/media/AudioRoutesInfo;->bluetoothName:Ljava/lang/CharSequence;

    if-eqz v2, :cond_a0

    .line 243
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mBluetoothA2dpRoute:Landroid/media/MediaRouter$RouteInfo;

    if-nez v2, :cond_96

    .line 245
    new-instance v2, Landroid/media/MediaRouter$RouteInfo;

    iget-object v3, p0, Landroid/media/MediaRouter$Static;->mSystemCategory:Landroid/media/MediaRouter$RouteCategory;

    invoke-direct {v2, v3}, Landroid/media/MediaRouter$RouteInfo;-><init>(Landroid/media/MediaRouter$RouteCategory;)V

    .line 246
    .local v2, "info":Landroid/media/MediaRouter$RouteInfo;
    iget-object v3, p1, Landroid/media/AudioRoutesInfo;->bluetoothName:Ljava/lang/CharSequence;

    iput-object v3, v2, Landroid/media/MediaRouter$RouteInfo;->mName:Ljava/lang/CharSequence;

    .line 247
    iget-object v3, p0, Landroid/media/MediaRouter$Static;->mResources:Landroid/content/res/Resources;

    const v5, 0x1040191

    invoke-virtual {v3, v5}, Landroid/content/res/Resources;->getText(I)Ljava/lang/CharSequence;

    move-result-object v3

    iput-object v3, v2, Landroid/media/MediaRouter$RouteInfo;->mDescription:Ljava/lang/CharSequence;

    .line 249
    iput v4, v2, Landroid/media/MediaRouter$RouteInfo;->mSupportedTypes:I

    .line 250
    const/4 v3, 0x3

    iput v3, v2, Landroid/media/MediaRouter$RouteInfo;->mDeviceType:I

    .line 251
    sget-object v3, Landroid/media/MediaRouter;->sStatic:Landroid/media/MediaRouter$Static;

    iget-object v3, v3, Landroid/media/MediaRouter$Static;->mResources:Landroid/content/res/Resources;

    const v5, 0x1040190

    invoke-virtual {v3, v5}, Landroid/content/res/Resources;->getString(I)Ljava/lang/String;

    move-result-object v3

    iput-object v3, v2, Landroid/media/MediaRouter$RouteInfo;->mGlobalRouteId:Ljava/lang/String;

    .line 254
    iput-object v2, p0, Landroid/media/MediaRouter$Static;->mBluetoothA2dpRoute:Landroid/media/MediaRouter$RouteInfo;

    .line 255
    invoke-static {v2}, Landroid/media/MediaRouter;->addRouteStatic(Landroid/media/MediaRouter$RouteInfo;)V

    .line 256
    .end local v2    # "info":Landroid/media/MediaRouter$RouteInfo;
    goto :goto_ac

    .line 257
    :cond_96
    iget-object v3, p1, Landroid/media/AudioRoutesInfo;->bluetoothName:Ljava/lang/CharSequence;

    iput-object v3, v2, Landroid/media/MediaRouter$RouteInfo;->mName:Ljava/lang/CharSequence;

    .line 258
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mBluetoothA2dpRoute:Landroid/media/MediaRouter$RouteInfo;

    invoke-static {v2}, Landroid/media/MediaRouter;->dispatchRouteChanged(Landroid/media/MediaRouter$RouteInfo;)V

    goto :goto_ac

    .line 260
    :cond_a0
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mBluetoothA2dpRoute:Landroid/media/MediaRouter$RouteInfo;

    if-eqz v2, :cond_ac

    .line 262
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mBluetoothA2dpRoute:Landroid/media/MediaRouter$RouteInfo;

    .line 263
    .local v2, "btRoute":Landroid/media/MediaRouter$RouteInfo;
    const/4 v3, 0x0

    iput-object v3, p0, Landroid/media/MediaRouter$Static;->mBluetoothA2dpRoute:Landroid/media/MediaRouter$RouteInfo;

    .line 264
    invoke-static {v2}, Landroid/media/MediaRouter;->removeRouteStatic(Landroid/media/MediaRouter$RouteInfo;)V

    .line 266
    .end local v2    # "btRoute":Landroid/media/MediaRouter$RouteInfo;
    :cond_ac
    :goto_ac
    const/4 v0, 0x1

    .line 269
    :cond_ad
    if-eqz v0, :cond_f8

    .line 270
    new-instance v2, Ljava/lang/StringBuilder;

    invoke-direct {v2}, Ljava/lang/StringBuilder;-><init>()V

    const-string v3, "Audio routes updated: "

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v2

    invoke-virtual {v2, p1}, Ljava/lang/StringBuilder;->append(Ljava/lang/Object;)Ljava/lang/StringBuilder;

    move-result-object v2

    const-string v3, ", a2dp="

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v2

    invoke-virtual {p0}, Landroid/media/MediaRouter$Static;->isBluetoothA2dpOn()Z

    move-result v3

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Z)Ljava/lang/StringBuilder;

    move-result-object v2

    invoke-virtual {v2}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v2

    const-string v3, "MediaRouter"

    invoke-static {v3, v2}, Landroid/util/Log;->v(Ljava/lang/String;Ljava/lang/String;)I

    .line 271
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mSelectedRoute:Landroid/media/MediaRouter$RouteInfo;

    if-eqz v2, :cond_e7

    invoke-virtual {v2}, Landroid/media/MediaRouter$RouteInfo;->isDefault()Z

    move-result v2

    if-nez v2, :cond_e7

    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mSelectedRoute:Landroid/media/MediaRouter$RouteInfo;

    .line 272
    invoke-virtual {v2}, Landroid/media/MediaRouter$RouteInfo;->isBluetooth()Z

    move-result v2

    if-eqz v2, :cond_f8

    .line 273
    :cond_e7
    const/4 v2, 0x0

    if-nez v1, :cond_f3

    iget-object v3, p0, Landroid/media/MediaRouter$Static;->mBluetoothA2dpRoute:Landroid/media/MediaRouter$RouteInfo;

    if-nez v3, :cond_ef

    goto :goto_f3

    .line 276
    :cond_ef
    invoke-static {v4, v3, v2}, Landroid/media/MediaRouter;->selectRouteStatic(ILandroid/media/MediaRouter$RouteInfo;Z)V

    goto :goto_f8

    .line 274
    :cond_f3
    :goto_f3
    iget-object v3, p0, Landroid/media/MediaRouter$Static;->mDefaultAudioVideo:Landroid/media/MediaRouter$RouteInfo;

    invoke-static {v4, v3, v2}, Landroid/media/MediaRouter;->selectRouteStatic(ILandroid/media/MediaRouter$RouteInfo;Z)V

    .line 280
    :cond_f8
    :goto_f8
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mCurAudioRoutesInfo:Landroid/media/AudioRoutesInfo;

    iget-object v3, p1, Landroid/media/AudioRoutesInfo;->bluetoothName:Ljava/lang/CharSequence;

    iput-object v3, v2, Landroid/media/AudioRoutesInfo;->bluetoothName:Ljava/lang/CharSequence;

    .line 281
    return-void
.end method

.method updateClientState()V
    .registers 9

    .line 488
    const/4 v0, 0x0

    iput-object v0, p0, Landroid/media/MediaRouter$Static;->mClientState:Landroid/media/MediaRouterClientState;

    .line 489
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mClient:Landroid/media/IMediaRouterClient;

    if-eqz v1, :cond_14

    .line 491
    :try_start_7
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mMediaRouterService:Landroid/media/IMediaRouterService;

    invoke-interface {v2, v1}, Landroid/media/IMediaRouterService;->getState(Landroid/media/IMediaRouterClient;)Landroid/media/MediaRouterClientState;

    move-result-object v1

    iput-object v1, p0, Landroid/media/MediaRouter$Static;->mClientState:Landroid/media/MediaRouterClientState;
    :try_end_f
    .catch Landroid/os/RemoteException; {:try_start_7 .. :try_end_f} :catch_10

    .line 494
    goto :goto_14

    .line 492
    :catch_10
    move-exception v1

    .line 493
    .local v1, "ex":Landroid/os/RemoteException;
    invoke-virtual {v1}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    .line 497
    .end local v1    # "ex":Landroid/os/RemoteException;
    :cond_14
    :goto_14
    iget-object v1, p0, Landroid/media/MediaRouter$Static;->mClientState:Landroid/media/MediaRouterClientState;

    if-eqz v1, :cond_1a

    iget-object v0, v1, Landroid/media/MediaRouterClientState;->routes:Ljava/util/ArrayList;

    .line 500
    .local v0, "globalRoutes":Ljava/util/ArrayList;, "Ljava/util/ArrayList<Landroid/media/MediaRouterClientState$RouteInfo;>;"
    :cond_1a
    if-eqz v0, :cond_21

    invoke-virtual {v0}, Ljava/util/ArrayList;->size()I

    move-result v1

    goto :goto_22

    :cond_21
    const/4 v1, 0x0

    .line 501
    .local v1, "globalRouteCount":I
    :goto_22
    const/4 v2, 0x0

    .local v2, "i":I
    :goto_23
    if-ge v2, v1, :cond_41

    .line 502
    invoke-virtual {v0, v2}, Ljava/util/ArrayList;->get(I)Ljava/lang/Object;

    move-result-object v3

    check-cast v3, Landroid/media/MediaRouterClientState$RouteInfo;

    .line 503
    .local v3, "globalRoute":Landroid/media/MediaRouterClientState$RouteInfo;
    iget-object v4, v3, Landroid/media/MediaRouterClientState$RouteInfo;->id:Ljava/lang/String;

    invoke-virtual {p0, v4}, Landroid/media/MediaRouter$Static;->findGlobalRoute(Ljava/lang/String;)Landroid/media/MediaRouter$RouteInfo;

    move-result-object v4

    .line 504
    .local v4, "route":Landroid/media/MediaRouter$RouteInfo;
    if-nez v4, :cond_3b

    .line 505
    invoke-virtual {p0, v3}, Landroid/media/MediaRouter$Static;->makeGlobalRoute(Landroid/media/MediaRouterClientState$RouteInfo;)Landroid/media/MediaRouter$RouteInfo;

    move-result-object v4

    .line 506
    invoke-static {v4}, Landroid/media/MediaRouter;->addRouteStatic(Landroid/media/MediaRouter$RouteInfo;)V

    goto :goto_3e

    .line 508
    :cond_3b
    invoke-virtual {p0, v4, v3}, Landroid/media/MediaRouter$Static;->updateGlobalRoute(Landroid/media/MediaRouter$RouteInfo;Landroid/media/MediaRouterClientState$RouteInfo;)V

    .line 501
    .end local v3    # "globalRoute":Landroid/media/MediaRouterClientState$RouteInfo;
    .end local v4    # "route":Landroid/media/MediaRouter$RouteInfo;
    :goto_3e
    add-int/lit8 v2, v2, 0x1

    goto :goto_23

    .line 513
    .end local v2    # "i":I
    :cond_41
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mRoutes:Ljava/util/ArrayList;

    invoke-virtual {v2}, Ljava/util/ArrayList;->size()I

    move-result v2

    .restart local v2    # "i":I
    :goto_47
    add-int/lit8 v3, v2, -0x1

    .end local v2    # "i":I
    .local v3, "i":I
    if-lez v2, :cond_7f

    .line 514
    iget-object v2, p0, Landroid/media/MediaRouter$Static;->mRoutes:Ljava/util/ArrayList;

    invoke-virtual {v2, v3}, Ljava/util/ArrayList;->get(I)Ljava/lang/Object;

    move-result-object v2

    check-cast v2, Landroid/media/MediaRouter$RouteInfo;

    .line 515
    .local v2, "route":Landroid/media/MediaRouter$RouteInfo;
    iget-object v4, v2, Landroid/media/MediaRouter$RouteInfo;->mGlobalRouteId:Ljava/lang/String;

    .line 516
    .local v4, "globalRouteId":Ljava/lang/String;
    invoke-virtual {v2}, Landroid/media/MediaRouter$RouteInfo;->isDefault()Z

    move-result v5

    if-nez v5, :cond_7d

    invoke-virtual {v2}, Landroid/media/MediaRouter$RouteInfo;->isBluetooth()Z

    move-result v5

    if-eqz v5, :cond_62

    .line 517
    goto :goto_7d

    .line 519
    :cond_62
    if-eqz v4, :cond_7c

    .line 520
    const/4 v5, 0x0

    .local v5, "j":I
    :goto_65
    if-ge v5, v1, :cond_79

    .line 521
    invoke-virtual {v0, v5}, Ljava/util/ArrayList;->get(I)Ljava/lang/Object;

    move-result-object v6

    check-cast v6, Landroid/media/MediaRouterClientState$RouteInfo;

    .line 522
    .local v6, "globalRoute":Landroid/media/MediaRouterClientState$RouteInfo;
    iget-object v7, v6, Landroid/media/MediaRouterClientState$RouteInfo;->id:Ljava/lang/String;

    invoke-virtual {v4, v7}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v7

    if-eqz v7, :cond_76

    .line 523
    goto :goto_7d

    .line 520
    .end local v6    # "globalRoute":Landroid/media/MediaRouterClientState$RouteInfo;
    :cond_76
    add-int/lit8 v5, v5, 0x1

    goto :goto_65

    .line 527
    .end local v5    # "j":I
    :cond_79
    invoke-static {v2}, Landroid/media/MediaRouter;->removeRouteStatic(Landroid/media/MediaRouter$RouteInfo;)V

    .line 529
    .end local v2    # "route":Landroid/media/MediaRouter$RouteInfo;
    .end local v4    # "globalRouteId":Ljava/lang/String;
    :cond_7c
    nop

    .line 513
    :cond_7d
    :goto_7d
    move v2, v3

    goto :goto_47

    .line 530
    .end local v3    # "i":I
    :cond_7f
    return-void
.end method

.method updateDiscoveryRequest()V
    .registers 11

    .line 305
    const/4 v0, 0x0

    .line 306
    .local v0, "routeTypes":I
    const/4 v1, 0x0

    .line 307
    .local v1, "passiveRouteTypes":I
    const/4 v2, 0x0

    .line 308
    .local v2, "activeScan":Z
    const/4 v3, 0x0

    .line 309
    .local v3, "activeScanWifiDisplay":Z
    iget-object v4, p0, Landroid/media/MediaRouter$Static;->mCallbacks:Ljava/util/concurrent/CopyOnWriteArrayList;

    invoke-virtual {v4}, Ljava/util/concurrent/CopyOnWriteArrayList;->size()I

    move-result v4

    .line 310
    .local v4, "count":I
    const/4 v5, 0x0

    .local v5, "i":I
    :goto_b
    const/4 v6, 0x4

    const/4 v7, 0x1

    if-ge v5, v4, :cond_3d

    .line 311
    iget-object v8, p0, Landroid/media/MediaRouter$Static;->mCallbacks:Ljava/util/concurrent/CopyOnWriteArrayList;

    invoke-virtual {v8, v5}, Ljava/util/concurrent/CopyOnWriteArrayList;->get(I)Ljava/lang/Object;

    move-result-object v8

    check-cast v8, Landroid/media/MediaRouter$CallbackInfo;

    .line 312
    .local v8, "cbi":Landroid/media/MediaRouter$CallbackInfo;
    iget v9, v8, Landroid/media/MediaRouter$CallbackInfo;->flags:I

    and-int/lit8 v9, v9, 0x5

    if-eqz v9, :cond_21

    .line 315
    iget v9, v8, Landroid/media/MediaRouter$CallbackInfo;->type:I

    or-int/2addr v0, v9

    goto :goto_2e

    .line 316
    :cond_21
    iget v9, v8, Landroid/media/MediaRouter$CallbackInfo;->flags:I

    and-int/lit8 v9, v9, 0x8

    if-eqz v9, :cond_2b

    .line 318
    iget v9, v8, Landroid/media/MediaRouter$CallbackInfo;->type:I

    or-int/2addr v1, v9

    goto :goto_2e

    .line 323
    :cond_2b
    iget v9, v8, Landroid/media/MediaRouter$CallbackInfo;->type:I

    or-int/2addr v0, v9

    .line 325
    :goto_2e
    iget v9, v8, Landroid/media/MediaRouter$CallbackInfo;->flags:I

    and-int/2addr v7, v9

    if-eqz v7, :cond_3a

    .line 326
    const/4 v2, 0x1

    .line 327
    iget v7, v8, Landroid/media/MediaRouter$CallbackInfo;->type:I

    and-int/2addr v6, v7

    if-eqz v6, :cond_3a

    .line 328
    const/4 v3, 0x1

    .line 310
    .end local v8    # "cbi":Landroid/media/MediaRouter$CallbackInfo;
    :cond_3a
    add-int/lit8 v5, v5, 0x1

    goto :goto_b

    .line 332
    .end local v5    # "i":I
    :cond_3d
    if-nez v0, :cond_41

    if-eqz v2, :cond_42

    .line 336
    :cond_41
    or-int/2addr v0, v1

    .line 341
    :cond_42
    iget-boolean v5, p0, Landroid/media/MediaRouter$Static;->mCanConfigureWifiDisplays:Z

    if-eqz v5, :cond_6b

    .line 342
    iget-object v5, p0, Landroid/media/MediaRouter$Static;->mSelectedRoute:Landroid/media/MediaRouter$RouteInfo;

    if-eqz v5, :cond_51

    .line 343
    invoke-virtual {v5, v6}, Landroid/media/MediaRouter$RouteInfo;->matchesTypes(I)Z

    move-result v5

    if-eqz v5, :cond_51

    .line 346
    const/4 v3, 0x0

    .line 348
    :cond_51
    if-eqz v3, :cond_5f

    .line 349
    iget-boolean v5, p0, Landroid/media/MediaRouter$Static;->mActivelyScanningWifiDisplays:Z

    if-nez v5, :cond_6b

    .line 350
    iput-boolean v7, p0, Landroid/media/MediaRouter$Static;->mActivelyScanningWifiDisplays:Z

    .line 351
    iget-object v5, p0, Landroid/media/MediaRouter$Static;->mDisplayService:Landroid/hardware/display/DisplayManager;

    invoke-virtual {v5}, Landroid/hardware/display/DisplayManager;->startWifiDisplayScan()V

    goto :goto_6b

    .line 354
    :cond_5f
    iget-boolean v5, p0, Landroid/media/MediaRouter$Static;->mActivelyScanningWifiDisplays:Z

    if-eqz v5, :cond_6b

    .line 355
    const/4 v5, 0x0

    iput-boolean v5, p0, Landroid/media/MediaRouter$Static;->mActivelyScanningWifiDisplays:Z

    .line 356
    iget-object v5, p0, Landroid/media/MediaRouter$Static;->mDisplayService:Landroid/hardware/display/DisplayManager;

    invoke-virtual {v5}, Landroid/hardware/display/DisplayManager;->stopWifiDisplayScan()V

    .line 362
    :cond_6b
    :goto_6b
    iget v5, p0, Landroid/media/MediaRouter$Static;->mDiscoveryRequestRouteTypes:I

    if-ne v0, v5, :cond_73

    iget-boolean v5, p0, Landroid/media/MediaRouter$Static;->mDiscoverRequestActiveScan:Z

    if-eq v2, v5, :cond_7a

    .line 364
    :cond_73
    iput v0, p0, Landroid/media/MediaRouter$Static;->mDiscoveryRequestRouteTypes:I

    .line 365
    iput-boolean v2, p0, Landroid/media/MediaRouter$Static;->mDiscoverRequestActiveScan:Z

    .line 366
    invoke-virtual {p0}, Landroid/media/MediaRouter$Static;->publishClientDiscoveryRequest()V

    .line 368
    :cond_7a
    return-void
.end method

.method updateGlobalRoute(Landroid/media/MediaRouter$RouteInfo;Landroid/media/MediaRouterClientState$RouteInfo;)V
    .registers 9
    .param p1, "route"    # Landroid/media/MediaRouter$RouteInfo;
    .param p2, "globalRoute"    # Landroid/media/MediaRouterClientState$RouteInfo;

    .line 574
    const/4 v0, 0x0

    .line 575
    .local v0, "changed":Z
    const/4 v1, 0x0

    .line 576
    .local v1, "volumeChanged":Z
    const/4 v2, 0x0

    .line 578
    .local v2, "presentationDisplayChanged":Z
    iget-object v3, p1, Landroid/media/MediaRouter$RouteInfo;->mName:Ljava/lang/CharSequence;

    iget-object v4, p2, Landroid/media/MediaRouterClientState$RouteInfo;->name:Ljava/lang/String;

    invoke-static {v3, v4}, Ljava/util/Objects;->equals(Ljava/lang/Object;Ljava/lang/Object;)Z

    move-result v3

    if-nez v3, :cond_12

    .line 579
    iget-object v3, p2, Landroid/media/MediaRouterClientState$RouteInfo;->name:Ljava/lang/String;

    iput-object v3, p1, Landroid/media/MediaRouter$RouteInfo;->mName:Ljava/lang/CharSequence;

    .line 580
    const/4 v0, 0x1

    .line 582
    :cond_12
    iget-object v3, p1, Landroid/media/MediaRouter$RouteInfo;->mDescription:Ljava/lang/CharSequence;

    iget-object v4, p2, Landroid/media/MediaRouterClientState$RouteInfo;->description:Ljava/lang/String;

    invoke-static {v3, v4}, Ljava/util/Objects;->equals(Ljava/lang/Object;Ljava/lang/Object;)Z

    move-result v3

    if-nez v3, :cond_21

    .line 583
    iget-object v3, p2, Landroid/media/MediaRouterClientState$RouteInfo;->description:Ljava/lang/String;

    iput-object v3, p1, Landroid/media/MediaRouter$RouteInfo;->mDescription:Ljava/lang/CharSequence;

    .line 584
    const/4 v0, 0x1

    .line 586
    :cond_21
    iget v3, p1, Landroid/media/MediaRouter$RouteInfo;->mSupportedTypes:I

    .line 587
    .local v3, "oldSupportedTypes":I
    iget v4, p2, Landroid/media/MediaRouterClientState$RouteInfo;->supportedTypes:I

    if-eq v3, v4, :cond_2c

    .line 588
    iget v4, p2, Landroid/media/MediaRouterClientState$RouteInfo;->supportedTypes:I

    iput v4, p1, Landroid/media/MediaRouter$RouteInfo;->mSupportedTypes:I

    .line 589
    const/4 v0, 0x1

    .line 591
    :cond_2c
    iget-boolean v4, p1, Landroid/media/MediaRouter$RouteInfo;->mEnabled:Z

    iget-boolean v5, p2, Landroid/media/MediaRouterClientState$RouteInfo;->enabled:Z

    if-eq v4, v5, :cond_37

    .line 592
    iget-boolean v4, p2, Landroid/media/MediaRouterClientState$RouteInfo;->enabled:Z

    iput-boolean v4, p1, Landroid/media/MediaRouter$RouteInfo;->mEnabled:Z

    .line 593
    const/4 v0, 0x1

    .line 595
    :cond_37
    invoke-static {p1}, Landroid/media/MediaRouter$RouteInfo;->-$$Nest$fgetmRealStatusCode(Landroid/media/MediaRouter$RouteInfo;)I

    move-result v4

    iget v5, p2, Landroid/media/MediaRouterClientState$RouteInfo;->statusCode:I

    if-eq v4, v5, :cond_45

    .line 596
    iget v4, p2, Landroid/media/MediaRouterClientState$RouteInfo;->statusCode:I

    invoke-virtual {p1, v4}, Landroid/media/MediaRouter$RouteInfo;->setRealStatusCode(I)Z

    .line 597
    const/4 v0, 0x1

    .line 599
    :cond_45
    iget v4, p1, Landroid/media/MediaRouter$RouteInfo;->mPlaybackType:I

    iget v5, p2, Landroid/media/MediaRouterClientState$RouteInfo;->playbackType:I

    if-eq v4, v5, :cond_50

    .line 600
    iget v4, p2, Landroid/media/MediaRouterClientState$RouteInfo;->playbackType:I

    iput v4, p1, Landroid/media/MediaRouter$RouteInfo;->mPlaybackType:I

    .line 601
    const/4 v0, 0x1

    .line 603
    :cond_50
    iget v4, p1, Landroid/media/MediaRouter$RouteInfo;->mPlaybackStream:I

    iget v5, p2, Landroid/media/MediaRouterClientState$RouteInfo;->playbackStream:I

    if-eq v4, v5, :cond_5b

    .line 604
    iget v4, p2, Landroid/media/MediaRouterClientState$RouteInfo;->playbackStream:I

    iput v4, p1, Landroid/media/MediaRouter$RouteInfo;->mPlaybackStream:I

    .line 605
    const/4 v0, 0x1

    .line 607
    :cond_5b
    iget v4, p1, Landroid/media/MediaRouter$RouteInfo;->mVolume:I

    iget v5, p2, Landroid/media/MediaRouterClientState$RouteInfo;->volume:I

    if-eq v4, v5, :cond_67

    .line 608
    iget v4, p2, Landroid/media/MediaRouterClientState$RouteInfo;->volume:I

    iput v4, p1, Landroid/media/MediaRouter$RouteInfo;->mVolume:I

    .line 609
    const/4 v0, 0x1

    .line 610
    const/4 v1, 0x1

    .line 612
    :cond_67
    iget v4, p1, Landroid/media/MediaRouter$RouteInfo;->mVolumeMax:I

    iget v5, p2, Landroid/media/MediaRouterClientState$RouteInfo;->volumeMax:I

    if-eq v4, v5, :cond_73

    .line 613
    iget v4, p2, Landroid/media/MediaRouterClientState$RouteInfo;->volumeMax:I

    iput v4, p1, Landroid/media/MediaRouter$RouteInfo;->mVolumeMax:I

    .line 614
    const/4 v0, 0x1

    .line 615
    const/4 v1, 0x1

    .line 617
    :cond_73
    iget v4, p1, Landroid/media/MediaRouter$RouteInfo;->mVolumeHandling:I

    iget v5, p2, Landroid/media/MediaRouterClientState$RouteInfo;->volumeHandling:I

    if-eq v4, v5, :cond_7f

    .line 618
    iget v4, p2, Landroid/media/MediaRouterClientState$RouteInfo;->volumeHandling:I

    iput v4, p1, Landroid/media/MediaRouter$RouteInfo;->mVolumeHandling:I

    .line 619
    const/4 v0, 0x1

    .line 620
    const/4 v1, 0x1

    .line 622
    :cond_7f
    iget v4, p1, Landroid/media/MediaRouter$RouteInfo;->mPresentationDisplayId:I

    iget v5, p2, Landroid/media/MediaRouterClientState$RouteInfo;->presentationDisplayId:I

    if-eq v4, v5, :cond_8e

    .line 623
    iget v4, p2, Landroid/media/MediaRouterClientState$RouteInfo;->presentationDisplayId:I

    iput v4, p1, Landroid/media/MediaRouter$RouteInfo;->mPresentationDisplayId:I

    .line 624
    invoke-virtual {p1}, Landroid/media/MediaRouter$RouteInfo;->updatePresentationDisplay()Z

    .line 625
    const/4 v0, 0x1

    .line 626
    const/4 v2, 0x1

    .line 629
    :cond_8e
    if-eqz v0, :cond_93

    .line 630
    invoke-static {p1, v3}, Landroid/media/MediaRouter;->dispatchRouteChanged(Landroid/media/MediaRouter$RouteInfo;I)V

    .line 632
    :cond_93
    if-eqz v1, :cond_98

    .line 633
    invoke-static {p1}, Landroid/media/MediaRouter;->dispatchRouteVolumeChanged(Landroid/media/MediaRouter$RouteInfo;)V

    .line 635
    :cond_98
    if-eqz v2, :cond_9d

    .line 636
    invoke-static {p1}, Landroid/media/MediaRouter;->dispatchRoutePresentationDisplayChanged(Landroid/media/MediaRouter$RouteInfo;)V

    .line 638
    :cond_9d
    return-void
.end method
