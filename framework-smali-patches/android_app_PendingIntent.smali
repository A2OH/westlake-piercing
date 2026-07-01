.class public final Landroid/app/PendingIntent;
.super Ljava/lang/Object;
.source "PendingIntent.java"

# interfaces
.implements Landroid/os/Parcelable;


# annotations
.annotation system Ldalvik/annotation/MemberClasses;
    value = {
        Landroid/app/PendingIntent$OnFinished;,
        Landroid/app/PendingIntent$CanceledException;,
        Landroid/app/PendingIntent$FinishedDispatcher;,
        Landroid/app/PendingIntent$CancelListener;,
        Landroid/app/PendingIntent$CancelListerInfo;,
        Landroid/app/PendingIntent$OnMarshaledListener;,
        Landroid/app/PendingIntent$Flags;
    }
.end annotation


# static fields
.field public static final BLOCK_MUTABLE_IMPLICIT_PENDING_INTENT:J = 0xe1bd1a4L

.field public static final CREATOR:Landroid/os/Parcelable$Creator;
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "Landroid/os/Parcelable$Creator<",
            "Landroid/app/PendingIntent;",
            ">;"
        }
    .end annotation
.end field

.field public static final FLAG_ALLOW_UNSAFE_IMPLICIT_INTENT:I = 0x1000000

.field public static final FLAG_CANCEL_CURRENT:I = 0x10000000

.field public static final FLAG_IMMUTABLE:I = 0x4000000

.field public static final FLAG_MUTABLE:I = 0x2000000

.field public static final FLAG_MUTABLE_UNAUDITED:I = 0x2000000
    .annotation runtime Ljava/lang/Deprecated;
    .end annotation
.end field

.field public static final FLAG_NO_CREATE:I = 0x20000000

.field public static final FLAG_ONE_SHOT:I = 0x40000000

.field public static final FLAG_UPDATE_CURRENT:I = 0x8000000

.field static final PENDING_INTENT_EXPLICIT_MUTABILITY_REQUIRED:J = 0x9958763L

.field private static final TAG:Ljava/lang/String; = "PendingIntent"

.field private static final sOnMarshaledListener:Ljava/lang/ThreadLocal;
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "Ljava/lang/ThreadLocal<",
            "Landroid/app/PendingIntent$OnMarshaledListener;",
            ">;"
        }
    .end annotation
.end field


# instance fields
.field private mCachedInfo:Landroid/app/ActivityManager$PendingIntentInfo;

.field private mCancelListerInfo:Landroid/app/PendingIntent$CancelListerInfo;

.field private final mTarget:Landroid/content/IIntentSender;

.field private mWhitelistToken:Landroid/os/IBinder;


# direct methods
.method public static synthetic $r8$lambda$QO6-NNaHiy4IcxfwuD4-kyVEcUY(Landroid/app/PendingIntent;Landroid/util/Pair;)V
    .registers 2

    invoke-direct {p0, p1}, Landroid/app/PendingIntent;->lambda$notifyCancelListeners$0(Landroid/util/Pair;)V

    return-void
.end method

.method static bridge synthetic -$$Nest$mnotifyCancelListeners(Landroid/app/PendingIntent;)V
    .registers 1

    invoke-direct {p0}, Landroid/app/PendingIntent;->notifyCancelListeners()V

    return-void
.end method

.method static constructor <clinit>()V
    .registers 1

    .line 394
    new-instance v0, Ljava/lang/ThreadLocal;

    invoke-direct {v0}, Ljava/lang/ThreadLocal;-><init>()V

    sput-object v0, Landroid/app/PendingIntent;->sOnMarshaledListener:Ljava/lang/ThreadLocal;

    .line 1461
    new-instance v0, Landroid/app/PendingIntent$1;

    invoke-direct {v0}, Landroid/app/PendingIntent$1;-><init>()V

    sput-object v0, Landroid/app/PendingIntent;->CREATOR:Landroid/os/Parcelable$Creator;

    return-void
.end method

.method public constructor <init>(Landroid/content/IIntentSender;)V
    .registers 3
    .param p1, "target"    # Landroid/content/IIntentSender;

    .line 1514
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 1515
    invoke-static {p1}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/content/IIntentSender;

    iput-object v0, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    .line 1516
    return-void
.end method

.method constructor <init>(Landroid/os/IBinder;Ljava/lang/Object;)V
    .registers 4
    .param p1, "target"    # Landroid/os/IBinder;
    .param p2, "cookie"    # Ljava/lang/Object;

    .line 1518
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 1519
    invoke-static {p1}, Landroid/content/IIntentSender$Stub;->asInterface(Landroid/os/IBinder;)Landroid/content/IIntentSender;

    move-result-object v0

    invoke-static {v0}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/content/IIntentSender;

    iput-object v0, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    .line 1520
    if-eqz p2, :cond_16

    .line 1521
    move-object v0, p2

    check-cast v0, Landroid/os/IBinder;

    iput-object v0, p0, Landroid/app/PendingIntent;->mWhitelistToken:Landroid/os/IBinder;

    .line 1523
    :cond_16
    return-void
.end method

.method private static buildServicePendingIntent(Landroid/content/Context;ILandroid/content/Intent;II)Landroid/app/PendingIntent;
    .registers 23
    .param p0, "context"    # Landroid/content/Context;
    .param p1, "requestCode"    # I
    .param p2, "intent"    # Landroid/content/Intent;
    .param p3, "flags"    # I
    .param p4, "serviceKind"    # I

    .line 808
    move-object/from16 v1, p0

    move-object/from16 v2, p2

    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getPackageName()Ljava/lang/String;

    move-result-object v15

    .line 809
    .local v15, "packageName":Ljava/lang/String;
    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getContentResolver()Landroid/content/ContentResolver;

    move-result-object v0

    invoke-virtual {v2, v0}, Landroid/content/Intent;->resolveTypeIfNeeded(Landroid/content/ContentResolver;)Ljava/lang/String;

    move-result-object v16

    .line 810
    .local v16, "resolvedType":Ljava/lang/String;
    const/4 v0, 0x0

    move/from16 v14, p3

    invoke-static {v14, v2, v1, v0}, Landroid/app/PendingIntent;->checkPendingIntent(ILandroid/content/Intent;Landroid/content/Context;Z)V

    .line 812
    :try_start_16
    invoke-virtual {v2, v1}, Landroid/content/Intent;->prepareToLeaveProcess(Landroid/content/Context;)V

    .line 814
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v3

    .line 815
    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getAttributionTag()Ljava/lang/String;

    move-result-object v6

    const/4 v7, 0x0

    const/4 v8, 0x0

    filled-new-array/range {p2 .. p2}, [Landroid/content/Intent;

    move-result-object v10

    .line 817
    const/4 v0, 0x0

    if-eqz v16, :cond_30

    filled-new-array/range {v16 .. v16}, [Ljava/lang/String;

    move-result-object v4

    move-object v11, v4

    goto :goto_31

    :cond_30
    move-object v11, v0

    :goto_31
    const/4 v13, 0x0

    .line 818
    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getUserId()I

    move-result v17

    .line 814
    move/from16 v4, p4

    move-object v5, v15

    move/from16 v9, p1

    move/from16 v12, p3

    move/from16 v14, v17

    invoke-interface/range {v3 .. v14}, Landroid/app/IActivityManager;->getIntentSenderWithFeature(ILjava/lang/String;Ljava/lang/String;Landroid/os/IBinder;Ljava/lang/String;I[Landroid/content/Intent;[Ljava/lang/String;ILandroid/os/Bundle;I)Landroid/content/IIntentSender;

    move-result-object v3

    if-nez v3, :cond_4e

    new-instance v3, Landroid/os/Binder;

    invoke-direct {v3}, Landroid/os/Binder;-><init>()V

    invoke-static {v3}, Landroid/content/IIntentSender$Stub;->asInterface(Landroid/os/IBinder;)Landroid/content/IIntentSender;

    move-result-object v3

    .line 819
    .local v3, "target":Landroid/content/IIntentSender;
    :cond_4e
    if-eqz v3, :cond_55

    new-instance v0, Landroid/app/PendingIntent;

    invoke-direct {v0, v3}, Landroid/app/PendingIntent;-><init>(Landroid/content/IIntentSender;)V
    :try_end_55
    .catch Landroid/os/RemoteException; {:try_start_16 .. :try_end_55} :catch_56

    :cond_55
    return-object v0

    .line 820
    .end local v3    # "target":Landroid/content/IIntentSender;
    :catch_56
    move-exception v0

    .line 821
    .local v0, "e":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v3

    throw v3
.end method

.method private static checkPendingIntent(ILandroid/content/Intent;Landroid/content/Context;Z)V
    .registers 10
    .param p0, "flags"    # I
    .param p1, "intent"    # Landroid/content/Intent;
    .param p2, "context"    # Landroid/content/Context;
    .param p3, "isActivityResultType"    # Z

    .line 411
    const/high16 v0, 0x4000000

    and-int/2addr v0, p0

    const/4 v1, 0x1

    const/4 v2, 0x0

    if-eqz v0, :cond_9

    move v0, v1

    goto :goto_a

    :cond_9
    move v0, v2

    .line 412
    .local v0, "isFlagImmutableSet":Z
    :goto_a
    const/high16 v3, 0x2000000

    and-int/2addr v3, p0

    if-eqz v3, :cond_10

    goto :goto_11

    :cond_10
    move v1, v2

    .line 413
    .local v1, "isFlagMutableSet":Z
    :goto_11
    invoke-virtual {p2}, Landroid/content/Context;->getPackageName()Ljava/lang/String;

    move-result-object v2

    .line 415
    .local v2, "packageName":Ljava/lang/String;
    if-eqz v0, :cond_22

    if-nez v1, :cond_1a

    goto :goto_22

    .line 416
    :cond_1a
    new-instance v3, Ljava/lang/IllegalArgumentException;

    const-string v4, "Cannot set both FLAG_IMMUTABLE and FLAG_MUTABLE for PendingIntent"

    invoke-direct {v3, v4}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v3

    .line 420
    :cond_22
    :goto_22
    const-wide/32 v3, 0x9958763

    invoke-static {v3, v4}, Landroid/compat/Compatibility;->isChangeEnabled(J)Z

    move-result v3

    if-eqz v3, :cond_55

    if-nez v0, :cond_55

    if-eqz v1, :cond_30

    goto :goto_55

    .line 422
    :cond_30
    new-instance v3, Ljava/lang/StringBuilder;

    invoke-direct {v3}, Ljava/lang/StringBuilder;-><init>()V

    invoke-virtual {v3, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    const-string v4, ": Targeting S+ (version "

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    const/16 v4, 0x1f

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v3

    const-string v4, " and above) requires that one of FLAG_IMMUTABLE or FLAG_MUTABLE be specified when creating a PendingIntent.\nStrongly consider using FLAG_IMMUTABLE, only use FLAG_MUTABLE if some functionality depends on the PendingIntent being mutable, e.g. if it needs to be used with inline replies or bubbles."

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v3

    .line 428
    .local v3, "msg":Ljava/lang/String;
    new-instance v4, Ljava/lang/IllegalArgumentException;

    invoke-direct {v4, v3}, Ljava/lang/IllegalArgumentException;-><init>(Ljava/lang/String;)V

    throw v4

    .line 436
    .end local v3    # "msg":Ljava/lang/String;
    :cond_55
    :goto_55
    invoke-static {p0, p1, p3}, Landroid/app/PendingIntent;->isNewMutableDisallowedImplicitPendingIntent(ILandroid/content/Intent;Z)Z

    move-result v3

    if-eqz v3, :cond_a3

    .line 437
    const-wide/32 v3, 0xe1bd1a4

    invoke-static {v3, v4}, Landroid/compat/Compatibility;->isChangeEnabled(J)Z

    move-result v3

    if-nez v3, :cond_a3

    .line 438
    new-instance v3, Ljava/lang/StringBuilder;

    invoke-direct {v3}, Ljava/lang/StringBuilder;-><init>()V

    const-string v4, "New mutable implicit PendingIntent: pkg="

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    invoke-virtual {v3, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    const-string v4, ", action="

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    .line 439
    invoke-virtual {p1}, Landroid/content/Intent;->getAction()Ljava/lang/String;

    move-result-object v4

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    const-string v4, ", featureId="

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    .line 440
    invoke-virtual {p2}, Landroid/content/Context;->getAttributionTag()Ljava/lang/String;

    move-result-object v4

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    const-string v4, ". This will be blocked once the app targets U+ for security reasons."

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v3

    .line 443
    .restart local v3    # "msg":Ljava/lang/String;
    new-instance v4, Landroid/app/StackTrace;

    invoke-direct {v4, v3}, Landroid/app/StackTrace;-><init>(Ljava/lang/String;)V

    const-string v5, "PendingIntent"

    invoke-static {v5, v4}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/Throwable;)I

    .line 445
    .end local v3    # "msg":Ljava/lang/String;
    :cond_a3
    return-void
.end method

.method public static getActivities(Landroid/content/Context;I[Landroid/content/Intent;I)Landroid/app/PendingIntent;
    .registers 5
    .param p0, "context"    # Landroid/content/Context;
    .param p1, "requestCode"    # I
    .param p2, "intents"    # [Landroid/content/Intent;
    .param p3, "flags"    # I

    .line 608
    const/4 v0, 0x0

    invoke-static {p0, p1, p2, p3, v0}, Landroid/app/PendingIntent;->getActivities(Landroid/content/Context;I[Landroid/content/Intent;ILandroid/os/Bundle;)Landroid/app/PendingIntent;

    move-result-object v0

    return-object v0
.end method

.method public static getActivities(Landroid/content/Context;I[Landroid/content/Intent;ILandroid/os/Bundle;)Landroid/app/PendingIntent;
    .registers 13
    .param p0, "context"    # Landroid/content/Context;
    .param p1, "requestCode"    # I
    .param p2, "intents"    # [Landroid/content/Intent;
    .param p3, "flags"    # I
    .param p4, "options"    # Landroid/os/Bundle;

    .line 660
    invoke-virtual {p0}, Landroid/content/Context;->getUser()Landroid/os/UserHandle;

    move-result-object v0

    .line 661
    .local v0, "user":Landroid/os/UserHandle;
    nop

    .line 662
    if-eqz v0, :cond_9

    move-object v7, v0

    goto :goto_12

    :cond_9
    invoke-virtual {p0}, Landroid/content/Context;->getUserId()I

    move-result v1

    invoke-static {v1}, Landroid/os/UserHandle;->of(I)Landroid/os/UserHandle;

    move-result-object v1

    move-object v7, v1

    .line 661
    :goto_12
    move-object v2, p0

    move v3, p1

    move-object v4, p2

    move v5, p3

    move-object v6, p4

    invoke-static/range {v2 .. v7}, Landroid/app/PendingIntent;->getActivitiesAsUser(Landroid/content/Context;I[Landroid/content/Intent;ILandroid/os/Bundle;Landroid/os/UserHandle;)Landroid/app/PendingIntent;

    move-result-object v1

    return-object v1
.end method

.method public static getActivitiesAsUser(Landroid/content/Context;I[Landroid/content/Intent;ILandroid/os/Bundle;Landroid/os/UserHandle;)Landroid/app/PendingIntent;
    .registers 23
    .param p0, "context"    # Landroid/content/Context;
    .param p1, "requestCode"    # I
    .param p2, "intents"    # [Landroid/content/Intent;
    .param p3, "flags"    # I
    .param p4, "options"    # Landroid/os/Bundle;
    .param p5, "user"    # Landroid/os/UserHandle;

    .line 672
    move-object/from16 v1, p0

    move-object/from16 v14, p2

    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getPackageName()Ljava/lang/String;

    move-result-object v15

    .line 673
    .local v15, "packageName":Ljava/lang/String;
    array-length v0, v14

    new-array v13, v0, [Ljava/lang/String;

    .line 674
    .local v13, "resolvedTypes":[Ljava/lang/String;
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_c
    array-length v2, v14

    if-ge v0, v2, :cond_30

    .line 675
    aget-object v2, v14, v0

    invoke-virtual {v2, v1}, Landroid/content/Intent;->migrateExtraStreamToClipData(Landroid/content/Context;)Z

    .line 676
    aget-object v2, v14, v0

    invoke-virtual {v2, v1}, Landroid/content/Intent;->prepareToLeaveProcess(Landroid/content/Context;)V

    .line 677
    aget-object v2, v14, v0

    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getContentResolver()Landroid/content/ContentResolver;

    move-result-object v3

    invoke-virtual {v2, v3}, Landroid/content/Intent;->resolveTypeIfNeeded(Landroid/content/ContentResolver;)Ljava/lang/String;

    move-result-object v2

    aput-object v2, v13, v0

    .line 678
    aget-object v2, v14, v0

    const/4 v3, 0x0

    move/from16 v12, p3

    invoke-static {v12, v2, v1, v3}, Landroid/app/PendingIntent;->checkPendingIntent(ILandroid/content/Intent;Landroid/content/Context;Z)V

    .line 674
    add-int/lit8 v0, v0, 0x1

    goto :goto_c

    :cond_30
    move/from16 v12, p3

    .line 682
    .end local v0    # "i":I
    :try_start_32
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v2

    const/4 v3, 0x2

    .line 684
    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getAttributionTag()Ljava/lang/String;

    move-result-object v5

    const/4 v6, 0x0

    const/4 v7, 0x0

    .line 685
    invoke-virtual/range {p5 .. p5}, Landroid/os/UserHandle;->getIdentifier()I

    move-result v0
    :try_end_41
    .catch Landroid/os/RemoteException; {:try_start_32 .. :try_end_41} :catch_69

    .line 682
    move-object v4, v15

    move/from16 v8, p1

    move-object/from16 v9, p2

    move-object v10, v13

    move/from16 v11, p3

    move-object/from16 v12, p4

    move-object/from16 v16, v13

    .end local v13    # "resolvedTypes":[Ljava/lang/String;
    .local v16, "resolvedTypes":[Ljava/lang/String;
    move v13, v0

    :try_start_4e
    invoke-interface/range {v2 .. v13}, Landroid/app/IActivityManager;->getIntentSenderWithFeature(ILjava/lang/String;Ljava/lang/String;Landroid/os/IBinder;Ljava/lang/String;I[Landroid/content/Intent;[Ljava/lang/String;ILandroid/os/Bundle;I)Landroid/content/IIntentSender;

    move-result-object v0

    if-nez v0, :cond_5d

    new-instance v0, Landroid/os/Binder;

    invoke-direct {v0}, Landroid/os/Binder;-><init>()V

    invoke-static {v0}, Landroid/content/IIntentSender$Stub;->asInterface(Landroid/os/IBinder;)Landroid/content/IIntentSender;

    move-result-object v0

    .line 686
    .local v0, "target":Landroid/content/IIntentSender;
    :cond_5d
    if-eqz v0, :cond_65

    new-instance v2, Landroid/app/PendingIntent;

    invoke-direct {v2, v0}, Landroid/app/PendingIntent;-><init>(Landroid/content/IIntentSender;)V
    :try_end_64
    .catch Landroid/os/RemoteException; {:try_start_4e .. :try_end_64} :catch_67

    goto :goto_66

    :cond_65
    const/4 v2, 0x0

    :goto_66
    return-object v2

    .line 687
    .end local v0    # "target":Landroid/content/IIntentSender;
    :catch_67
    move-exception v0

    goto :goto_6c

    .end local v16    # "resolvedTypes":[Ljava/lang/String;
    .restart local v13    # "resolvedTypes":[Ljava/lang/String;
    :catch_69
    move-exception v0

    move-object/from16 v16, v13

    .line 688
    .end local v13    # "resolvedTypes":[Ljava/lang/String;
    .local v0, "e":Landroid/os/RemoteException;
    .restart local v16    # "resolvedTypes":[Ljava/lang/String;
    :goto_6c
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v2

    throw v2
.end method

.method public static getActivity(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;
    .registers 5
    .param p0, "context"    # Landroid/content/Context;
    .param p1, "requestCode"    # I
    .param p2, "intent"    # Landroid/content/Intent;
    .param p3, "flags"    # I

    .line 494
    const/4 v0, 0x0

    invoke-static {p0, p1, p2, p3, v0}, Landroid/app/PendingIntent;->getActivity(Landroid/content/Context;ILandroid/content/Intent;ILandroid/os/Bundle;)Landroid/app/PendingIntent;

    move-result-object v0

    return-object v0
.end method

.method public static getActivity(Landroid/content/Context;ILandroid/content/Intent;ILandroid/os/Bundle;)Landroid/app/PendingIntent;
    .registers 13
    .param p0, "context"    # Landroid/content/Context;
    .param p1, "requestCode"    # I
    .param p2, "intent"    # Landroid/content/Intent;
    .param p3, "flags"    # I
    .param p4, "options"    # Landroid/os/Bundle;

    .line 529
    invoke-virtual {p0}, Landroid/content/Context;->getUser()Landroid/os/UserHandle;

    move-result-object v0

    .line 530
    .local v0, "user":Landroid/os/UserHandle;
    nop

    .line 531
    if-eqz v0, :cond_9

    move-object v7, v0

    goto :goto_12

    :cond_9
    invoke-virtual {p0}, Landroid/content/Context;->getUserId()I

    move-result v1

    invoke-static {v1}, Landroid/os/UserHandle;->of(I)Landroid/os/UserHandle;

    move-result-object v1

    move-object v7, v1

    .line 530
    :goto_12
    move-object v2, p0

    move v3, p1

    move-object v4, p2

    move v5, p3

    move-object v6, p4

    invoke-static/range {v2 .. v7}, Landroid/app/PendingIntent;->getActivityAsUser(Landroid/content/Context;ILandroid/content/Intent;ILandroid/os/Bundle;Landroid/os/UserHandle;)Landroid/app/PendingIntent;

    move-result-object v1

    return-object v1
.end method

.method public static getActivityAsUser(Landroid/content/Context;ILandroid/content/Intent;ILandroid/os/Bundle;Landroid/os/UserHandle;)Landroid/app/PendingIntent;
    .registers 24
    .param p0, "context"    # Landroid/content/Context;
    .param p1, "requestCode"    # I
    .param p2, "intent"    # Landroid/content/Intent;
    .param p3, "flags"    # I
    .param p4, "options"    # Landroid/os/Bundle;
    .param p5, "user"    # Landroid/os/UserHandle;

    .line 542
    move-object/from16 v1, p0

    move-object/from16 v2, p2

    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getPackageName()Ljava/lang/String;

    move-result-object v15

    .line 543
    .local v15, "packageName":Ljava/lang/String;
    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getContentResolver()Landroid/content/ContentResolver;

    move-result-object v0

    invoke-virtual {v2, v0}, Landroid/content/Intent;->resolveTypeIfNeeded(Landroid/content/ContentResolver;)Ljava/lang/String;

    move-result-object v16

    .line 544
    .local v16, "resolvedType":Ljava/lang/String;
    const/4 v0, 0x0

    move/from16 v14, p3

    invoke-static {v14, v2, v1, v0}, Landroid/app/PendingIntent;->checkPendingIntent(ILandroid/content/Intent;Landroid/content/Context;Z)V

    .line 546
    :try_start_16
    invoke-virtual {v2, v1}, Landroid/content/Intent;->migrateExtraStreamToClipData(Landroid/content/Context;)Z

    .line 547
    invoke-virtual {v2, v1}, Landroid/content/Intent;->prepareToLeaveProcess(Landroid/content/Context;)V

    .line 549
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v3

    const/4 v4, 0x2

    .line 551
    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getAttributionTag()Ljava/lang/String;

    move-result-object v6

    const/4 v7, 0x0

    const/4 v8, 0x0

    filled-new-array/range {p2 .. p2}, [Landroid/content/Intent;

    move-result-object v10

    .line 552
    const/4 v0, 0x0

    if-eqz v16, :cond_34

    filled-new-array/range {v16 .. v16}, [Ljava/lang/String;

    move-result-object v5

    move-object v11, v5

    goto :goto_35

    :cond_34
    move-object v11, v0

    .line 553
    :goto_35
    invoke-virtual/range {p5 .. p5}, Landroid/os/UserHandle;->getIdentifier()I

    move-result v17

    .line 549
    move-object v5, v15

    move/from16 v9, p1

    move/from16 v12, p3

    move-object/from16 v13, p4

    move/from16 v14, v17

    invoke-interface/range {v3 .. v14}, Landroid/app/IActivityManager;->getIntentSenderWithFeature(ILjava/lang/String;Ljava/lang/String;Landroid/os/IBinder;Ljava/lang/String;I[Landroid/content/Intent;[Ljava/lang/String;ILandroid/os/Bundle;I)Landroid/content/IIntentSender;

    move-result-object v3

    if-nez v3, :cond_51

    new-instance v3, Landroid/os/Binder;

    invoke-direct {v3}, Landroid/os/Binder;-><init>()V

    invoke-static {v3}, Landroid/content/IIntentSender$Stub;->asInterface(Landroid/os/IBinder;)Landroid/content/IIntentSender;

    move-result-object v3

    .line 554
    .local v3, "target":Landroid/content/IIntentSender;
    :cond_51
    if-eqz v3, :cond_58

    new-instance v0, Landroid/app/PendingIntent;

    invoke-direct {v0, v3}, Landroid/app/PendingIntent;-><init>(Landroid/content/IIntentSender;)V
    :try_end_58
    .catch Landroid/os/RemoteException; {:try_start_16 .. :try_end_58} :catch_59

    :cond_58
    return-object v0

    .line 555
    .end local v3    # "target":Landroid/content/IIntentSender;
    :catch_59
    move-exception v0

    .line 556
    .local v0, "e":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v3

    throw v3
.end method

.method public static getBroadcast(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;
    .registers 5
    .param p0, "context"    # Landroid/content/Context;
    .param p1, "requestCode"    # I
    .param p2, "intent"    # Landroid/content/Intent;
    .param p3, "flags"    # I

    .line 718
    invoke-virtual {p0}, Landroid/content/Context;->getUser()Landroid/os/UserHandle;

    move-result-object v0

    invoke-static {p0, p1, p2, p3, v0}, Landroid/app/PendingIntent;->getBroadcastAsUser(Landroid/content/Context;ILandroid/content/Intent;ILandroid/os/UserHandle;)Landroid/app/PendingIntent;

    move-result-object v0

    return-object v0
.end method

.method public static getBroadcastAsUser(Landroid/content/Context;ILandroid/content/Intent;ILandroid/os/UserHandle;)Landroid/app/PendingIntent;
    .registers 23
    .param p0, "context"    # Landroid/content/Context;
    .param p1, "requestCode"    # I
    .param p2, "intent"    # Landroid/content/Intent;
    .param p3, "flags"    # I
    .param p4, "userHandle"    # Landroid/os/UserHandle;

    .line 729
    move-object/from16 v1, p0

    move-object/from16 v2, p2

    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getPackageName()Ljava/lang/String;

    move-result-object v15

    .line 730
    .local v15, "packageName":Ljava/lang/String;
    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getContentResolver()Landroid/content/ContentResolver;

    move-result-object v0

    invoke-virtual {v2, v0}, Landroid/content/Intent;->resolveTypeIfNeeded(Landroid/content/ContentResolver;)Ljava/lang/String;

    move-result-object v16

    .line 731
    .local v16, "resolvedType":Ljava/lang/String;
    const/4 v0, 0x0

    move/from16 v14, p3

    invoke-static {v14, v2, v1, v0}, Landroid/app/PendingIntent;->checkPendingIntent(ILandroid/content/Intent;Landroid/content/Context;Z)V

    .line 733
    :try_start_16
    invoke-virtual {v2, v1}, Landroid/content/Intent;->prepareToLeaveProcess(Landroid/content/Context;)V

    .line 735
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v3

    const/4 v4, 0x1

    .line 737
    invoke-virtual/range {p0 .. p0}, Landroid/content/Context;->getAttributionTag()Ljava/lang/String;

    move-result-object v6

    const/4 v7, 0x0

    const/4 v8, 0x0

    filled-new-array/range {p2 .. p2}, [Landroid/content/Intent;

    move-result-object v10

    .line 738
    const/4 v0, 0x0

    if-eqz v16, :cond_31

    filled-new-array/range {v16 .. v16}, [Ljava/lang/String;

    move-result-object v5

    move-object v11, v5

    goto :goto_32

    :cond_31
    move-object v11, v0

    :goto_32
    const/4 v13, 0x0

    .line 739
    invoke-virtual/range {p4 .. p4}, Landroid/os/UserHandle;->getIdentifier()I

    move-result v17

    .line 735
    move-object v5, v15

    move/from16 v9, p1

    move/from16 v12, p3

    move/from16 v14, v17

    invoke-interface/range {v3 .. v14}, Landroid/app/IActivityManager;->getIntentSenderWithFeature(ILjava/lang/String;Ljava/lang/String;Landroid/os/IBinder;Ljava/lang/String;I[Landroid/content/Intent;[Ljava/lang/String;ILandroid/os/Bundle;I)Landroid/content/IIntentSender;

    move-result-object v3

    if-nez v3, :cond_4d

    new-instance v3, Landroid/os/Binder;

    invoke-direct {v3}, Landroid/os/Binder;-><init>()V

    invoke-static {v3}, Landroid/content/IIntentSender$Stub;->asInterface(Landroid/os/IBinder;)Landroid/content/IIntentSender;

    move-result-object v3

    .line 740
    .local v3, "target":Landroid/content/IIntentSender;
    :cond_4d
    if-eqz v3, :cond_54

    new-instance v0, Landroid/app/PendingIntent;

    invoke-direct {v0, v3}, Landroid/app/PendingIntent;-><init>(Landroid/content/IIntentSender;)V
    :try_end_54
    .catch Landroid/os/RemoteException; {:try_start_16 .. :try_end_54} :catch_55

    :cond_54
    return-object v0

    .line 741
    .end local v3    # "target":Landroid/content/IIntentSender;
    :catch_55
    move-exception v0

    .line 742
    .local v0, "e":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v3

    throw v3
.end method

.method private getCachedInfo()Landroid/app/ActivityManager$PendingIntentInfo;
    .registers 3

    .line 1552
    iget-object v0, p0, Landroid/app/PendingIntent;->mCachedInfo:Landroid/app/ActivityManager$PendingIntentInfo;

    if-nez v0, :cond_17

    .line 1554
    :try_start_4
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v0

    iget-object v1, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    invoke-interface {v0, v1}, Landroid/app/IActivityManager;->getInfoForIntentSender(Landroid/content/IIntentSender;)Landroid/app/ActivityManager$PendingIntentInfo;

    move-result-object v0

    iput-object v0, p0, Landroid/app/PendingIntent;->mCachedInfo:Landroid/app/ActivityManager$PendingIntentInfo;
    :try_end_10
    .catch Landroid/os/RemoteException; {:try_start_4 .. :try_end_10} :catch_11

    .line 1557
    goto :goto_17

    .line 1555
    :catch_11
    move-exception v0

    .line 1556
    .local v0, "e":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1

    .line 1560
    .end local v0    # "e":Landroid/os/RemoteException;
    :cond_17
    :goto_17
    iget-object v0, p0, Landroid/app/PendingIntent;->mCachedInfo:Landroid/app/ActivityManager$PendingIntentInfo;

    return-object v0
.end method

.method public static getForegroundService(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;
    .registers 5
    .param p0, "context"    # Landroid/content/Context;
    .param p1, "requestCode"    # I
    .param p2, "intent"    # Landroid/content/Intent;
    .param p3, "flags"    # I

    .line 802
    const/4 v0, 0x5

    invoke-static {p0, p1, p2, p3, v0}, Landroid/app/PendingIntent;->buildServicePendingIntent(Landroid/content/Context;ILandroid/content/Intent;II)Landroid/app/PendingIntent;

    move-result-object v0

    return-object v0
.end method

.method public static getService(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;
    .registers 5
    .param p0, "context"    # Landroid/content/Context;
    .param p1, "requestCode"    # I
    .param p2, "intent"    # Landroid/content/Intent;
    .param p3, "flags"    # I

    .line 772
    const/4 v0, 0x4

    invoke-static {p0, p1, p2, p3, v0}, Landroid/app/PendingIntent;->buildServicePendingIntent(Landroid/content/Context;ILandroid/content/Intent;II)Landroid/app/PendingIntent;

    move-result-object v0

    return-object v0
.end method

.method public static isNewMutableDisallowedImplicitPendingIntent(ILandroid/content/Intent;Z)Z
    .registers 9
    .param p0, "flags"    # I
    .param p1, "intent"    # Landroid/content/Intent;
    .param p2, "isActivityResultType"    # Z

    .line 450
    const/4 v0, 0x0

    if-eqz p2, :cond_4

    .line 454
    return v0

    .line 456
    :cond_4
    const/high16 v1, 0x20000000

    and-int/2addr v1, p0

    const/4 v2, 0x1

    if-eqz v1, :cond_c

    move v1, v2

    goto :goto_d

    :cond_c
    move v1, v0

    .line 457
    .local v1, "isFlagNoCreateSet":Z
    :goto_d
    const/high16 v3, 0x2000000

    and-int/2addr v3, p0

    if-eqz v3, :cond_14

    move v3, v2

    goto :goto_15

    :cond_14
    move v3, v0

    .line 458
    .local v3, "isFlagMutableSet":Z
    :goto_15
    invoke-virtual {p1}, Landroid/content/Intent;->getComponent()Landroid/content/ComponentName;

    move-result-object v4

    if-nez v4, :cond_23

    invoke-virtual {p1}, Landroid/content/Intent;->getPackage()Ljava/lang/String;

    move-result-object v4

    if-nez v4, :cond_23

    move v4, v2

    goto :goto_24

    :cond_23
    move v4, v0

    .line 459
    .local v4, "isImplicit":Z
    :goto_24
    const/high16 v5, 0x1000000

    and-int/2addr v5, p0

    if-eqz v5, :cond_2b

    move v5, v2

    goto :goto_2c

    :cond_2b
    move v5, v0

    .line 461
    .local v5, "isFlagAllowUnsafeImplicitIntentSet":Z
    :goto_2c
    if-nez v1, :cond_35

    if-eqz v3, :cond_35

    if-eqz v4, :cond_35

    if-nez v5, :cond_35

    move v0, v2

    :cond_35
    return v0
.end method

.method private synthetic lambda$notifyCancelListeners$0(Landroid/util/Pair;)V
    .registers 3
    .param p1, "pair"    # Landroid/util/Pair;

    .line 1212
    iget-object v0, p1, Landroid/util/Pair;->second:Ljava/lang/Object;

    check-cast v0, Landroid/app/PendingIntent$CancelListener;

    invoke-interface {v0, p0}, Landroid/app/PendingIntent$CancelListener;->onCanceled(Landroid/app/PendingIntent;)V

    return-void
.end method

.method private notifyCancelListeners()V
    .registers 7

    .line 1202
    iget-object v0, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    monitor-enter v0

    .line 1204
    :try_start_3
    iget-object v1, p0, Landroid/app/PendingIntent;->mCancelListerInfo:Landroid/app/PendingIntent$CancelListerInfo;

    .line 1205
    .local v1, "cli":Landroid/app/PendingIntent$CancelListerInfo;
    const/4 v2, 0x1

    invoke-static {v1, v2}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fputmCanceled(Landroid/app/PendingIntent$CancelListerInfo;Z)V

    .line 1206
    new-instance v2, Landroid/util/ArraySet;

    invoke-static {v1}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fgetmCancelListeners(Landroid/app/PendingIntent$CancelListerInfo;)Landroid/util/ArraySet;

    move-result-object v3

    invoke-direct {v2, v3}, Landroid/util/ArraySet;-><init>(Landroid/util/ArraySet;)V

    .line 1207
    .local v2, "cancelListeners":Landroid/util/ArraySet;, "Landroid/util/ArraySet<Landroid/util/Pair<Ljava/util/concurrent/Executor;Landroid/app/PendingIntent$CancelListener;>;>;"
    invoke-static {v1}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fgetmCancelListeners(Landroid/app/PendingIntent$CancelListerInfo;)Landroid/util/ArraySet;

    move-result-object v3

    invoke-virtual {v3}, Landroid/util/ArraySet;->clear()V

    .line 1208
    .end local v1    # "cli":Landroid/app/PendingIntent$CancelListerInfo;
    monitor-exit v0
    :try_end_1a
    .catchall {:try_start_3 .. :try_end_1a} :catchall_37

    .line 1209
    invoke-virtual {v2}, Landroid/util/ArraySet;->size()I

    move-result v0

    .line 1210
    .local v0, "size":I
    const/4 v1, 0x0

    .local v1, "i":I
    :goto_1f
    if-ge v1, v0, :cond_36

    .line 1211
    invoke-virtual {v2, v1}, Landroid/util/ArraySet;->valueAt(I)Ljava/lang/Object;

    move-result-object v3

    check-cast v3, Landroid/util/Pair;

    .line 1212
    .local v3, "pair":Landroid/util/Pair;, "Landroid/util/Pair<Ljava/util/concurrent/Executor;Landroid/app/PendingIntent$CancelListener;>;"
    iget-object v4, v3, Landroid/util/Pair;->first:Ljava/lang/Object;

    check-cast v4, Ljava/util/concurrent/Executor;

    new-instance v5, Landroid/app/PendingIntent$$ExternalSyntheticLambda0;

    invoke-direct {v5, p0, v3}, Landroid/app/PendingIntent$$ExternalSyntheticLambda0;-><init>(Landroid/app/PendingIntent;Landroid/util/Pair;)V

    invoke-interface {v4, v5}, Ljava/util/concurrent/Executor;->execute(Ljava/lang/Runnable;)V

    .line 1210
    .end local v3    # "pair":Landroid/util/Pair;, "Landroid/util/Pair<Ljava/util/concurrent/Executor;Landroid/app/PendingIntent$CancelListener;>;"
    add-int/lit8 v1, v1, 0x1

    goto :goto_1f

    .line 1214
    .end local v1    # "i":I
    :cond_36
    return-void

    .line 1208
    .end local v0    # "size":I
    .end local v2    # "cancelListeners":Landroid/util/ArraySet;, "Landroid/util/ArraySet<Landroid/util/Pair<Ljava/util/concurrent/Executor;Landroid/app/PendingIntent$CancelListener;>;>;"
    :catchall_37
    move-exception v1

    :try_start_38
    monitor-exit v0
    :try_end_39
    .catchall {:try_start_38 .. :try_end_39} :catchall_37

    throw v1
.end method

.method public static readPendingIntentOrNullFromParcel(Landroid/os/Parcel;)Landroid/app/PendingIntent;
    .registers 4
    .param p0, "in"    # Landroid/os/Parcel;

    .line 1505
    invoke-virtual {p0}, Landroid/os/Parcel;->readStrongBinder()Landroid/os/IBinder;

    move-result-object v0

    .line 1506
    .local v0, "b":Landroid/os/IBinder;
    if-eqz v0, :cond_12

    new-instance v1, Landroid/app/PendingIntent;

    const-class v2, Landroid/app/PendingIntent;

    invoke-virtual {p0, v2}, Landroid/os/Parcel;->getClassCookie(Ljava/lang/Class;)Ljava/lang/Object;

    move-result-object v2

    invoke-direct {v1, v0, v2}, Landroid/app/PendingIntent;-><init>(Landroid/os/IBinder;Ljava/lang/Object;)V

    goto :goto_13

    :cond_12
    const/4 v1, 0x0

    :goto_13
    return-object v1
.end method

.method public static setOnMarshaledListener(Landroid/app/PendingIntent$OnMarshaledListener;)V
    .registers 2
    .param p0, "listener"    # Landroid/app/PendingIntent$OnMarshaledListener;

    .line 406
    sget-object v0, Landroid/app/PendingIntent;->sOnMarshaledListener:Ljava/lang/ThreadLocal;

    invoke-virtual {v0, p0}, Ljava/lang/ThreadLocal;->set(Ljava/lang/Object;)V

    .line 407
    return-void
.end method

.method public static writePendingIntentOrNullToParcel(Landroid/app/PendingIntent;Landroid/os/Parcel;)V
    .registers 4
    .param p0, "sender"    # Landroid/app/PendingIntent;
    .param p1, "out"    # Landroid/os/Parcel;

    .line 1484
    if-eqz p0, :cond_9

    iget-object v0, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    invoke-interface {v0}, Landroid/content/IIntentSender;->asBinder()Landroid/os/IBinder;

    move-result-object v0

    goto :goto_a

    :cond_9
    const/4 v0, 0x0

    :goto_a
    invoke-virtual {p1, v0}, Landroid/os/Parcel;->writeStrongBinder(Landroid/os/IBinder;)V

    .line 1485
    if-eqz p0, :cond_1d

    .line 1486
    sget-object v0, Landroid/app/PendingIntent;->sOnMarshaledListener:Ljava/lang/ThreadLocal;

    invoke-virtual {v0}, Ljava/lang/ThreadLocal;->get()Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/app/PendingIntent$OnMarshaledListener;

    .line 1487
    .local v0, "listener":Landroid/app/PendingIntent$OnMarshaledListener;
    if-eqz v0, :cond_1d

    .line 1488
    const/4 v1, 0x0

    invoke-interface {v0, p0, p1, v1}, Landroid/app/PendingIntent$OnMarshaledListener;->onMarshaled(Landroid/app/PendingIntent;Landroid/os/Parcel;I)V

    .line 1491
    .end local v0    # "listener":Landroid/app/PendingIntent$OnMarshaledListener;
    :cond_1d
    return-void
.end method


# virtual methods
.method public addCancelListener(Ljava/util/concurrent/Executor;Landroid/app/PendingIntent$CancelListener;)Z
    .registers 9
    .param p1, "executor"    # Ljava/util/concurrent/Executor;
    .param p2, "cancelListener"    # Landroid/app/PendingIntent$CancelListener;
    .annotation runtime Landroid/annotation/SystemApi;
        client = .enum Landroid/annotation/SystemApi$Client;->MODULE_LIBRARIES:Landroid/annotation/SystemApi$Client;
    .end annotation

    .line 1171
    iget-object v0, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    monitor-enter v0

    .line 1172
    :try_start_3
    iget-object v1, p0, Landroid/app/PendingIntent;->mCancelListerInfo:Landroid/app/PendingIntent$CancelListerInfo;

    const/4 v2, 0x0

    if-eqz v1, :cond_10

    invoke-static {v1}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fgetmCanceled(Landroid/app/PendingIntent$CancelListerInfo;)Z

    move-result v1

    if-eqz v1, :cond_10

    .line 1173
    monitor-exit v0

    return v2

    .line 1175
    :cond_10
    iget-object v1, p0, Landroid/app/PendingIntent;->mCancelListerInfo:Landroid/app/PendingIntent$CancelListerInfo;

    if-nez v1, :cond_1c

    .line 1176
    new-instance v1, Landroid/app/PendingIntent$CancelListerInfo;

    const/4 v3, 0x0

    invoke-direct {v1, p0, v3}, Landroid/app/PendingIntent$CancelListerInfo;-><init>(Landroid/app/PendingIntent;Landroid/app/PendingIntent$CancelListerInfo-IA;)V

    iput-object v1, p0, Landroid/app/PendingIntent;->mCancelListerInfo:Landroid/app/PendingIntent$CancelListerInfo;

    .line 1178
    :cond_1c
    iget-object v1, p0, Landroid/app/PendingIntent;->mCancelListerInfo:Landroid/app/PendingIntent$CancelListerInfo;

    .line 1180
    .local v1, "cli":Landroid/app/PendingIntent$CancelListerInfo;
    invoke-static {v1}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fgetmCancelListeners(Landroid/app/PendingIntent$CancelListerInfo;)Landroid/util/ArraySet;

    move-result-object v3

    invoke-virtual {v3}, Landroid/util/ArraySet;->isEmpty()Z

    move-result v3

    .line 1181
    .local v3, "wasEmpty":Z
    invoke-static {v1}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fgetmCancelListeners(Landroid/app/PendingIntent$CancelListerInfo;)Landroid/util/ArraySet;

    move-result-object v4

    invoke-static {p1, p2}, Landroid/util/Pair;->create(Ljava/lang/Object;Ljava/lang/Object;)Landroid/util/Pair;

    move-result-object v5

    invoke-virtual {v4, v5}, Landroid/util/ArraySet;->add(Ljava/lang/Object;)Z
    :try_end_31
    .catchall {:try_start_3 .. :try_end_31} :catchall_55

    .line 1182
    const/4 v4, 0x1

    if-eqz v3, :cond_4c

    .line 1185
    :try_start_34
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v2

    iget-object v5, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    invoke-interface {v2, v5, v1}, Landroid/app/IActivityManager;->registerIntentSenderCancelListenerEx(Landroid/content/IIntentSender;Lcom/android/internal/os/IResultReceiver;)Z

    move-result v2
    :try_end_3e
    .catch Landroid/os/RemoteException; {:try_start_34 .. :try_end_3e} :catch_46
    .catchall {:try_start_34 .. :try_end_3e} :catchall_55

    .line 1189
    .local v2, "success":Z
    nop

    .line 1190
    if-nez v2, :cond_44

    .line 1191
    :try_start_41
    invoke-static {v1, v4}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fputmCanceled(Landroid/app/PendingIntent$CancelListerInfo;Z)V

    .line 1193
    :cond_44
    monitor-exit v0

    return v2

    .line 1187
    .end local v2    # "success":Z
    :catch_46
    move-exception v2

    .line 1188
    .local v2, "e":Landroid/os/RemoteException;
    invoke-virtual {v2}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v4

    .end local p0    # "this":Landroid/app/PendingIntent;
    .end local p1    # "executor":Ljava/util/concurrent/Executor;
    .end local p2    # "cancelListener":Landroid/app/PendingIntent$CancelListener;
    throw v4

    .line 1195
    .end local v2    # "e":Landroid/os/RemoteException;
    .restart local p0    # "this":Landroid/app/PendingIntent;
    .restart local p1    # "executor":Ljava/util/concurrent/Executor;
    .restart local p2    # "cancelListener":Landroid/app/PendingIntent$CancelListener;
    :cond_4c
    invoke-static {v1}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fgetmCanceled(Landroid/app/PendingIntent$CancelListerInfo;)Z

    move-result v5

    if-nez v5, :cond_53

    move v2, v4

    :cond_53
    monitor-exit v0

    return v2

    .line 1197
    .end local v1    # "cli":Landroid/app/PendingIntent$CancelListerInfo;
    .end local v3    # "wasEmpty":Z
    :catchall_55
    move-exception v1

    monitor-exit v0
    :try_end_57
    .catchall {:try_start_41 .. :try_end_57} :catchall_55

    throw v1
.end method

.method public cancel()V
    .registers 3

    .line 842
    :try_start_0
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v0

    iget-object v1, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    invoke-interface {v0, v1}, Landroid/app/IActivityManager;->cancelIntentSender(Landroid/content/IIntentSender;)V
    :try_end_9
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_9} :catch_b

    .line 845
    nop

    .line 846
    return-void

    .line 843
    :catch_b
    move-exception v0

    .line 844
    .local v0, "e":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public describeContents()I
    .registers 2

    .line 1449
    const/4 v0, 0x0

    return v0
.end method

.method public dumpDebug(Landroid/util/proto/ProtoOutputStream;J)V
    .registers 9
    .param p1, "proto"    # Landroid/util/proto/ProtoOutputStream;
    .param p2, "fieldId"    # J

    .line 1443
    invoke-virtual {p1, p2, p3}, Landroid/util/proto/ProtoOutputStream;->start(J)J

    move-result-wide v0

    .line 1444
    .local v0, "token":J
    iget-object v2, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    invoke-interface {v2}, Landroid/content/IIntentSender;->asBinder()Landroid/os/IBinder;

    move-result-object v2

    invoke-virtual {v2}, Ljava/lang/Object;->toString()Ljava/lang/String;

    move-result-object v2

    const-wide v3, 0x10900000001L

    invoke-virtual {p1, v3, v4, v2}, Landroid/util/proto/ProtoOutputStream;->write(JLjava/lang/String;)V

    .line 1445
    invoke-virtual {p1, v0, v1}, Landroid/util/proto/ProtoOutputStream;->end(J)V

    .line 1446
    return-void
.end method

.method public equals(Ljava/lang/Object;)Z
    .registers 4
    .param p1, "otherObj"    # Ljava/lang/Object;

    .line 1418
    instance-of v0, p1, Landroid/app/PendingIntent;

    if-eqz v0, :cond_18

    .line 1419
    iget-object v0, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    invoke-interface {v0}, Landroid/content/IIntentSender;->asBinder()Landroid/os/IBinder;

    move-result-object v0

    move-object v1, p1

    check-cast v1, Landroid/app/PendingIntent;

    iget-object v1, v1, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    .line 1420
    invoke-interface {v1}, Landroid/content/IIntentSender;->asBinder()Landroid/os/IBinder;

    move-result-object v1

    .line 1419
    invoke-virtual {v0, v1}, Ljava/lang/Object;->equals(Ljava/lang/Object;)Z

    move-result v0

    return v0

    .line 1422
    :cond_18
    const/4 v0, 0x0

    return v0
.end method

.method public getCreatorPackage()Ljava/lang/String;
    .registers 2

    .line 1122
    invoke-direct {p0}, Landroid/app/PendingIntent;->getCachedInfo()Landroid/app/ActivityManager$PendingIntentInfo;

    move-result-object v0

    invoke-virtual {v0}, Landroid/app/ActivityManager$PendingIntentInfo;->getCreatorPackage()Ljava/lang/String;

    move-result-object v0

    return-object v0
.end method

.method public getCreatorUid()I
    .registers 2

    .line 1144
    invoke-direct {p0}, Landroid/app/PendingIntent;->getCachedInfo()Landroid/app/ActivityManager$PendingIntentInfo;

    move-result-object v0

    invoke-virtual {v0}, Landroid/app/ActivityManager$PendingIntentInfo;->getCreatorUid()I

    move-result v0

    return v0
.end method

.method public getCreatorUserHandle()Landroid/os/UserHandle;
    .registers 3

    .line 1275
    invoke-direct {p0}, Landroid/app/PendingIntent;->getCachedInfo()Landroid/app/ActivityManager$PendingIntentInfo;

    move-result-object v0

    invoke-virtual {v0}, Landroid/app/ActivityManager$PendingIntentInfo;->getCreatorUid()I

    move-result v0

    .line 1276
    .local v0, "uid":I
    invoke-static {v0}, Landroid/os/UserHandle;->getUserHandleForUid(I)Landroid/os/UserHandle;

    move-result-object v1

    return-object v1
.end method

.method public getIntent()Landroid/content/Intent;
    .registers 3

    .line 1335
    :try_start_0
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v0

    iget-object v1, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    .line 1336
    invoke-interface {v0, v1}, Landroid/app/IActivityManager;->getIntentForIntentSender(Landroid/content/IIntentSender;)Landroid/content/Intent;

    move-result-object v0
    :try_end_a
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_a} :catch_b

    .line 1335
    return-object v0

    .line 1337
    :catch_b
    move-exception v0

    .line 1338
    .local v0, "e":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getIntentSender()Landroid/content/IntentSender;
    .registers 4

    .line 833
    new-instance v0, Landroid/content/IntentSender;

    iget-object v1, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    iget-object v2, p0, Landroid/app/PendingIntent;->mWhitelistToken:Landroid/os/IBinder;

    invoke-direct {v0, v1, v2}, Landroid/content/IntentSender;-><init>(Landroid/content/IIntentSender;Landroid/os/IBinder;)V

    return-object v0
.end method

.method public getTag(Ljava/lang/String;)Ljava/lang/String;
    .registers 4
    .param p1, "prefix"    # Ljava/lang/String;

    .line 1349
    :try_start_0
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v0

    iget-object v1, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    .line 1350
    invoke-interface {v0, v1, p1}, Landroid/app/IActivityManager;->getTagForIntentSender(Landroid/content/IIntentSender;Ljava/lang/String;)Ljava/lang/String;

    move-result-object v0
    :try_end_a
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_a} :catch_b

    .line 1349
    return-object v0

    .line 1351
    :catch_b
    move-exception v0

    .line 1352
    .local v0, "e":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public getTarget()Landroid/content/IIntentSender;
    .registers 2

    .line 1527
    iget-object v0, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    return-object v0
.end method

.method public getTargetPackage()Ljava/lang/String;
    .registers 2
    .annotation runtime Ljava/lang/Deprecated;
    .end annotation

    .line 1100
    invoke-virtual {p0}, Landroid/app/PendingIntent;->getCreatorPackage()Ljava/lang/String;

    move-result-object v0

    return-object v0
.end method

.method public getWhitelistToken()Landroid/os/IBinder;
    .registers 2

    .line 1532
    iget-object v0, p0, Landroid/app/PendingIntent;->mWhitelistToken:Landroid/os/IBinder;

    return-object v0
.end method

.method public hashCode()I
    .registers 2

    .line 1427
    iget-object v0, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    invoke-interface {v0}, Landroid/content/IIntentSender;->asBinder()Landroid/os/IBinder;

    move-result-object v0

    invoke-virtual {v0}, Ljava/lang/Object;->hashCode()I

    move-result v0

    return v0
.end method

.method public intentFilterEquals(Landroid/app/PendingIntent;)Z
    .registers 4
    .param p1, "other"    # Landroid/app/PendingIntent;
    .annotation runtime Landroid/annotation/SystemApi;
        client = .enum Landroid/annotation/SystemApi$Client;->MODULE_LIBRARIES:Landroid/annotation/SystemApi$Client;
    .end annotation

    .line 1396
    if-nez p1, :cond_4

    .line 1397
    const/4 v0, 0x0

    return v0

    .line 1400
    :cond_4
    :try_start_4
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v0

    iget-object v1, p1, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    invoke-interface {v0, v1}, Landroid/app/IActivityManager;->getIntentForIntentSender(Landroid/content/IIntentSender;)Landroid/content/Intent;

    move-result-object v0

    .line 1401
    invoke-virtual {p0}, Landroid/app/PendingIntent;->getIntent()Landroid/content/Intent;

    move-result-object v1

    invoke-virtual {v0, v1}, Landroid/content/Intent;->filterEquals(Landroid/content/Intent;)Z

    move-result v0
    :try_end_16
    .catch Landroid/os/RemoteException; {:try_start_4 .. :try_end_16} :catch_17

    .line 1400
    return v0

    .line 1402
    :catch_17
    move-exception v0

    .line 1403
    .local v0, "e":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public isActivity()Z
    .registers 3

    .line 1304
    invoke-direct {p0}, Landroid/app/PendingIntent;->getCachedInfo()Landroid/app/ActivityManager$PendingIntentInfo;

    move-result-object v0

    invoke-virtual {v0}, Landroid/app/ActivityManager$PendingIntentInfo;->getIntentSenderType()I

    move-result v0

    const/4 v1, 0x2

    if-ne v0, v1, :cond_d

    const/4 v0, 0x1

    goto :goto_e

    :cond_d
    const/4 v0, 0x0

    :goto_e
    return v0
.end method

.method public isBroadcast()Z
    .registers 3

    .line 1325
    invoke-direct {p0}, Landroid/app/PendingIntent;->getCachedInfo()Landroid/app/ActivityManager$PendingIntentInfo;

    move-result-object v0

    invoke-virtual {v0}, Landroid/app/ActivityManager$PendingIntentInfo;->getIntentSenderType()I

    move-result v0

    const/4 v1, 0x1

    if-ne v0, v1, :cond_c

    goto :goto_d

    :cond_c
    const/4 v1, 0x0

    :goto_d
    return v1
.end method

.method public isForegroundService()Z
    .registers 3

    .line 1311
    invoke-direct {p0}, Landroid/app/PendingIntent;->getCachedInfo()Landroid/app/ActivityManager$PendingIntentInfo;

    move-result-object v0

    invoke-virtual {v0}, Landroid/app/ActivityManager$PendingIntentInfo;->getIntentSenderType()I

    move-result v0

    const/4 v1, 0x5

    if-ne v0, v1, :cond_d

    const/4 v0, 0x1

    goto :goto_e

    :cond_d
    const/4 v0, 0x0

    :goto_e
    return v0
.end method

.method public isImmutable()Z
    .registers 2

    .line 1296
    invoke-direct {p0}, Landroid/app/PendingIntent;->getCachedInfo()Landroid/app/ActivityManager$PendingIntentInfo;

    move-result-object v0

    invoke-virtual {v0}, Landroid/app/ActivityManager$PendingIntentInfo;->isImmutable()Z

    move-result v0

    return v0
.end method

.method public isService()Z
    .registers 3

    .line 1318
    invoke-direct {p0}, Landroid/app/PendingIntent;->getCachedInfo()Landroid/app/ActivityManager$PendingIntentInfo;

    move-result-object v0

    invoke-virtual {v0}, Landroid/app/ActivityManager$PendingIntentInfo;->getIntentSenderType()I

    move-result v0

    const/4 v1, 0x4

    if-ne v0, v1, :cond_d

    const/4 v0, 0x1

    goto :goto_e

    :cond_d
    const/4 v0, 0x0

    :goto_e
    return v0
.end method

.method public isTargetedToPackage()Z
    .registers 3

    .line 1285
    :try_start_0
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v0

    iget-object v1, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    .line 1286
    invoke-interface {v0, v1}, Landroid/app/IActivityManager;->isIntentSenderTargetedToPackage(Landroid/content/IIntentSender;)Z

    move-result v0
    :try_end_a
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_a} :catch_b

    .line 1285
    return v0

    .line 1287
    :catch_b
    move-exception v0

    .line 1288
    .local v0, "e":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public queryIntentComponents(I)Ljava/util/List;
    .registers 4
    .param p1, "flags"    # I
    .annotation runtime Landroid/annotation/SystemApi;
        client = .enum Landroid/annotation/SystemApi$Client;->MODULE_LIBRARIES:Landroid/annotation/SystemApi$Client;
    .end annotation

    .annotation system Ldalvik/annotation/Signature;
        value = {
            "(I)",
            "Ljava/util/List<",
            "Landroid/content/pm/ResolveInfo;",
            ">;"
        }
    .end annotation

    .line 1372
    :try_start_0
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v0

    iget-object v1, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    .line 1373
    invoke-interface {v0, v1, p1}, Landroid/app/IActivityManager;->queryIntentComponentsForIntentSender(Landroid/content/IIntentSender;I)Landroid/content/pm/ParceledListSlice;

    move-result-object v0

    .line 1374
    .local v0, "parceledList":Landroid/content/pm/ParceledListSlice;, "Landroid/content/pm/ParceledListSlice<Landroid/content/pm/ResolveInfo;>;"
    if-nez v0, :cond_11

    .line 1375
    invoke-static {}, Ljava/util/Collections;->emptyList()Ljava/util/List;

    move-result-object v1

    return-object v1

    .line 1377
    :cond_11
    invoke-virtual {v0}, Landroid/content/pm/ParceledListSlice;->getList()Ljava/util/List;

    move-result-object v1
    :try_end_15
    .catch Landroid/os/RemoteException; {:try_start_0 .. :try_end_15} :catch_16

    return-object v1

    .line 1378
    .end local v0    # "parceledList":Landroid/content/pm/ParceledListSlice;, "Landroid/content/pm/ParceledListSlice<Landroid/content/pm/ResolveInfo;>;"
    :catch_16
    move-exception v0

    .line 1379
    .local v0, "e":Landroid/os/RemoteException;
    invoke-virtual {v0}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v1

    throw v1
.end method

.method public registerCancelListener(Landroid/app/PendingIntent$CancelListener;)V
    .registers 3
    .param p1, "cancelListener"    # Landroid/app/PendingIntent$CancelListener;
    .annotation runtime Ljava/lang/Deprecated;
    .end annotation

    .line 1153
    new-instance v0, Landroid/app/PendingIntent$$ExternalSyntheticLambda1;

    invoke-direct {v0}, Landroid/app/PendingIntent$$ExternalSyntheticLambda1;-><init>()V

    invoke-virtual {p0, v0, p1}, Landroid/app/PendingIntent;->addCancelListener(Ljava/util/concurrent/Executor;Landroid/app/PendingIntent$CancelListener;)Z

    move-result v0

    if-nez v0, :cond_e

    .line 1155
    invoke-interface {p1, p0}, Landroid/app/PendingIntent$CancelListener;->onCanceled(Landroid/app/PendingIntent;)V

    .line 1157
    :cond_e
    return-void
.end method

.method public removeCancelListener(Landroid/app/PendingIntent$CancelListener;)V
    .registers 6
    .param p1, "cancelListener"    # Landroid/app/PendingIntent$CancelListener;
    .annotation runtime Landroid/annotation/SystemApi;
        client = .enum Landroid/annotation/SystemApi$Client;->MODULE_LIBRARIES:Landroid/annotation/SystemApi$Client;
    .end annotation

    .line 1233
    iget-object v0, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    monitor-enter v0

    .line 1234
    :try_start_3
    iget-object v1, p0, Landroid/app/PendingIntent;->mCancelListerInfo:Landroid/app/PendingIntent$CancelListerInfo;

    .line 1235
    .local v1, "cli":Landroid/app/PendingIntent$CancelListerInfo;
    if-eqz v1, :cond_52

    invoke-static {v1}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fgetmCancelListeners(Landroid/app/PendingIntent$CancelListerInfo;)Landroid/util/ArraySet;

    move-result-object v2

    invoke-virtual {v2}, Landroid/util/ArraySet;->size()I

    move-result v2

    if-nez v2, :cond_12

    goto :goto_52

    .line 1238
    :cond_12
    invoke-static {v1}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fgetmCancelListeners(Landroid/app/PendingIntent$CancelListerInfo;)Landroid/util/ArraySet;

    move-result-object v2

    invoke-virtual {v2}, Landroid/util/ArraySet;->size()I

    move-result v2

    add-int/lit8 v2, v2, -0x1

    .local v2, "i":I
    :goto_1c
    if-ltz v2, :cond_36

    .line 1239
    invoke-static {v1}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fgetmCancelListeners(Landroid/app/PendingIntent$CancelListerInfo;)Landroid/util/ArraySet;

    move-result-object v3

    invoke-virtual {v3, v2}, Landroid/util/ArraySet;->valueAt(I)Ljava/lang/Object;

    move-result-object v3

    check-cast v3, Landroid/util/Pair;

    iget-object v3, v3, Landroid/util/Pair;->second:Ljava/lang/Object;

    if-ne v3, p1, :cond_33

    .line 1240
    invoke-static {v1}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fgetmCancelListeners(Landroid/app/PendingIntent$CancelListerInfo;)Landroid/util/ArraySet;

    move-result-object v3

    invoke-virtual {v3, v2}, Landroid/util/ArraySet;->removeAt(I)Ljava/lang/Object;

    .line 1238
    :cond_33
    add-int/lit8 v2, v2, -0x1

    goto :goto_1c

    .line 1243
    .end local v2    # "i":I
    :cond_36
    invoke-static {v1}, Landroid/app/PendingIntent$CancelListerInfo;->-$$Nest$fgetmCancelListeners(Landroid/app/PendingIntent$CancelListerInfo;)Landroid/util/ArraySet;

    move-result-object v2

    invoke-virtual {v2}, Landroid/util/ArraySet;->isEmpty()Z

    move-result v2
    :try_end_3e
    .catchall {:try_start_3 .. :try_end_3e} :catchall_54

    if-eqz v2, :cond_50

    .line 1245
    :try_start_40
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v2

    iget-object v3, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    invoke-interface {v2, v3, v1}, Landroid/app/IActivityManager;->unregisterIntentSenderCancelListener(Landroid/content/IIntentSender;Lcom/android/internal/os/IResultReceiver;)V
    :try_end_49
    .catch Landroid/os/RemoteException; {:try_start_40 .. :try_end_49} :catch_4a
    .catchall {:try_start_40 .. :try_end_49} :catchall_54

    .line 1249
    goto :goto_50

    .line 1247
    :catch_4a
    move-exception v2

    .line 1248
    .local v2, "e":Landroid/os/RemoteException;
    :try_start_4b
    invoke-virtual {v2}, Landroid/os/RemoteException;->rethrowFromSystemServer()Ljava/lang/RuntimeException;

    move-result-object v3

    .end local p0    # "this":Landroid/app/PendingIntent;
    .end local p1    # "cancelListener":Landroid/app/PendingIntent$CancelListener;
    throw v3

    .line 1251
    .end local v1    # "cli":Landroid/app/PendingIntent$CancelListerInfo;
    .end local v2    # "e":Landroid/os/RemoteException;
    .restart local p0    # "this":Landroid/app/PendingIntent;
    .restart local p1    # "cancelListener":Landroid/app/PendingIntent$CancelListener;
    :cond_50
    :goto_50
    monitor-exit v0

    .line 1252
    return-void

    .line 1236
    .restart local v1    # "cli":Landroid/app/PendingIntent$CancelListerInfo;
    :cond_52
    :goto_52
    monitor-exit v0

    return-void

    .line 1251
    .end local v1    # "cli":Landroid/app/PendingIntent$CancelListerInfo;
    :catchall_54
    move-exception v1

    monitor-exit v0
    :try_end_56
    .catchall {:try_start_4b .. :try_end_56} :catchall_54

    throw v1
.end method

.method public send()V
    .registers 9
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Landroid/app/PendingIntent$CanceledException;
        }
    .end annotation

    .line 858
    const/4 v1, 0x0

    const/4 v2, 0x0

    const/4 v3, 0x0

    const/4 v4, 0x0

    const/4 v5, 0x0

    const/4 v6, 0x0

    const/4 v7, 0x0

    move-object v0, p0

    invoke-virtual/range {v0 .. v7}, Landroid/app/PendingIntent;->send(Landroid/content/Context;ILandroid/content/Intent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;Ljava/lang/String;Landroid/os/Bundle;)V

    .line 859
    return-void
.end method

.method public send(I)V
    .registers 10
    .param p1, "code"    # I
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Landroid/app/PendingIntent$CanceledException;
        }
    .end annotation

    .line 873
    const/4 v1, 0x0

    const/4 v3, 0x0

    const/4 v4, 0x0

    const/4 v5, 0x0

    const/4 v6, 0x0

    const/4 v7, 0x0

    move-object v0, p0

    move v2, p1

    invoke-virtual/range {v0 .. v7}, Landroid/app/PendingIntent;->send(Landroid/content/Context;ILandroid/content/Intent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;Ljava/lang/String;Landroid/os/Bundle;)V

    .line 874
    return-void
.end method

.method public send(ILandroid/app/PendingIntent$OnFinished;Landroid/os/Handler;)V
    .registers 12
    .param p1, "code"    # I
    .param p2, "onFinished"    # Landroid/app/PendingIntent$OnFinished;
    .param p3, "handler"    # Landroid/os/Handler;
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Landroid/app/PendingIntent$CanceledException;
        }
    .end annotation

    .line 934
    const/4 v1, 0x0

    const/4 v3, 0x0

    const/4 v6, 0x0

    const/4 v7, 0x0

    move-object v0, p0

    move v2, p1

    move-object v4, p2

    move-object v5, p3

    invoke-virtual/range {v0 .. v7}, Landroid/app/PendingIntent;->send(Landroid/content/Context;ILandroid/content/Intent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;Ljava/lang/String;Landroid/os/Bundle;)V

    .line 935
    return-void
.end method

.method public send(Landroid/content/Context;ILandroid/content/Intent;)V
    .registers 12
    .param p1, "context"    # Landroid/content/Context;
    .param p2, "code"    # I
    .param p3, "intent"    # Landroid/content/Intent;
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Landroid/app/PendingIntent$CanceledException;
        }
    .end annotation

    .line 895
    const/4 v4, 0x0

    const/4 v5, 0x0

    const/4 v6, 0x0

    const/4 v7, 0x0

    move-object v0, p0

    move-object v1, p1

    move v2, p2

    move-object v3, p3

    invoke-virtual/range {v0 .. v7}, Landroid/app/PendingIntent;->send(Landroid/content/Context;ILandroid/content/Intent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;Ljava/lang/String;Landroid/os/Bundle;)V

    .line 896
    return-void
.end method

.method public send(Landroid/content/Context;ILandroid/content/Intent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;)V
    .registers 14
    .param p1, "context"    # Landroid/content/Context;
    .param p2, "code"    # I
    .param p3, "intent"    # Landroid/content/Intent;
    .param p4, "onFinished"    # Landroid/app/PendingIntent$OnFinished;
    .param p5, "handler"    # Landroid/os/Handler;
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Landroid/app/PendingIntent$CanceledException;
        }
    .end annotation

    .line 969
    const/4 v6, 0x0

    const/4 v7, 0x0

    move-object v0, p0

    move-object v1, p1

    move v2, p2

    move-object v3, p3

    move-object v4, p4

    move-object v5, p5

    invoke-virtual/range {v0 .. v7}, Landroid/app/PendingIntent;->send(Landroid/content/Context;ILandroid/content/Intent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;Ljava/lang/String;Landroid/os/Bundle;)V

    .line 970
    return-void
.end method

.method public send(Landroid/content/Context;ILandroid/content/Intent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;Ljava/lang/String;)V
    .registers 15
    .param p1, "context"    # Landroid/content/Context;
    .param p2, "code"    # I
    .param p3, "intent"    # Landroid/content/Intent;
    .param p4, "onFinished"    # Landroid/app/PendingIntent$OnFinished;
    .param p5, "handler"    # Landroid/os/Handler;
    .param p6, "requiredPermission"    # Ljava/lang/String;
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Landroid/app/PendingIntent$CanceledException;
        }
    .end annotation

    .line 1011
    const/4 v7, 0x0

    move-object v0, p0

    move-object v1, p1

    move v2, p2

    move-object v3, p3

    move-object v4, p4

    move-object v5, p5

    move-object v6, p6

    invoke-virtual/range {v0 .. v7}, Landroid/app/PendingIntent;->send(Landroid/content/Context;ILandroid/content/Intent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;Ljava/lang/String;Landroid/os/Bundle;)V

    .line 1012
    return-void
.end method

.method public send(Landroid/content/Context;ILandroid/content/Intent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;Ljava/lang/String;Landroid/os/Bundle;)V
    .registers 9
    .param p1, "context"    # Landroid/content/Context;
    .param p2, "code"    # I
    .param p3, "intent"    # Landroid/content/Intent;
    .param p4, "onFinished"    # Landroid/app/PendingIntent$OnFinished;
    .param p5, "handler"    # Landroid/os/Handler;
    .param p6, "requiredPermission"    # Ljava/lang/String;
    .param p7, "options"    # Landroid/os/Bundle;
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Landroid/app/PendingIntent$CanceledException;
        }
    .end annotation

    .line 1052
    invoke-virtual/range {p0 .. p7}, Landroid/app/PendingIntent;->sendAndReturnResult(Landroid/content/Context;ILandroid/content/Intent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;Ljava/lang/String;Landroid/os/Bundle;)I

    move-result v0

    if-ltz v0, :cond_7

    .line 1056
    return-void

    .line 1054
    :cond_7
    new-instance v0, Landroid/app/PendingIntent$CanceledException;

    invoke-direct {v0}, Landroid/app/PendingIntent$CanceledException;-><init>()V

    throw v0
.end method

.method public send(Landroid/os/Bundle;)V
    .registers 10
    .param p1, "options"    # Landroid/os/Bundle;
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Landroid/app/PendingIntent$CanceledException;
        }
    .end annotation

    .line 912
    const/4 v1, 0x0

    const/4 v2, 0x0

    const/4 v3, 0x0

    const/4 v4, 0x0

    const/4 v5, 0x0

    const/4 v6, 0x0

    move-object v0, p0

    move-object v7, p1

    invoke-virtual/range {v0 .. v7}, Landroid/app/PendingIntent;->send(Landroid/content/Context;ILandroid/content/Intent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;Ljava/lang/String;Landroid/os/Bundle;)V

    .line 913
    return-void
.end method

.method public sendAndReturnResult(Landroid/content/Context;ILandroid/content/Intent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;Ljava/lang/String;Landroid/os/Bundle;)I
    .registers 24
    .param p1, "context"    # Landroid/content/Context;
    .param p2, "code"    # I
    .param p3, "intent"    # Landroid/content/Intent;
    .param p4, "onFinished"    # Landroid/app/PendingIntent$OnFinished;
    .param p5, "handler"    # Landroid/os/Handler;
    .param p6, "requiredPermission"    # Ljava/lang/String;
    .param p7, "options"    # Landroid/os/Bundle;
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Landroid/app/PendingIntent$CanceledException;
        }
    .end annotation

    .line 1067
    move-object/from16 v1, p0

    move-object/from16 v12, p3

    move-object/from16 v13, p4

    move-object/from16 v2, p7

    const/4 v0, 0x0

    if-eqz v12, :cond_19

    .line 1068
    :try_start_b
    invoke-virtual/range {p1 .. p1}, Landroid/content/Context;->getContentResolver()Landroid/content/ContentResolver;

    move-result-object v3

    invoke-virtual {v12, v3}, Landroid/content/Intent;->resolveTypeIfNeeded(Landroid/content/ContentResolver;)Ljava/lang/String;

    move-result-object v3

    move-object v8, v3

    goto :goto_1a

    .line 1089
    :catch_15
    move-exception v0

    move-object/from16 v15, p5

    goto :goto_6f

    .line 1069
    :cond_19
    move-object v8, v0

    :goto_1a
    nop

    .line 1071
    .local v8, "resolvedType":Ljava/lang/String;
    if-eqz p1, :cond_3e

    invoke-virtual/range {p0 .. p0}, Landroid/app/PendingIntent;->isActivity()Z

    move-result v3

    if-eqz v3, :cond_3e

    .line 1075
    if-eqz v2, :cond_2b

    new-instance v3, Landroid/app/ActivityOptions;

    invoke-direct {v3, v2}, Landroid/app/ActivityOptions;-><init>(Landroid/os/Bundle;)V

    goto :goto_2f

    .line 1076
    :cond_2b
    invoke-static {}, Landroid/app/ActivityOptions;->makeBasic()Landroid/app/ActivityOptions;

    move-result-object v3

    :goto_2f
    nop

    .line 1077
    .local v3, "activityOptions":Landroid/app/ActivityOptions;
    invoke-virtual/range {p1 .. p1}, Landroid/content/Context;->getDisplayId()I

    move-result v4

    invoke-virtual {v3, v4}, Landroid/app/ActivityOptions;->setCallerDisplayId(I)Landroid/app/ActivityOptions;

    .line 1078
    invoke-virtual {v3}, Landroid/app/ActivityOptions;->toBundle()Landroid/os/Bundle;

    move-result-object v4
    :try_end_3b
    .catch Landroid/os/RemoteException; {:try_start_b .. :try_end_3b} :catch_15

    move-object v2, v4

    move-object v14, v2

    .end local p7    # "options":Landroid/os/Bundle;
    .local v2, "options":Landroid/os/Bundle;
    goto :goto_3f

    .line 1081
    .end local v2    # "options":Landroid/os/Bundle;
    .end local v3    # "activityOptions":Landroid/app/ActivityOptions;
    .restart local p7    # "options":Landroid/os/Bundle;
    :cond_3e
    move-object v14, v2

    .end local p7    # "options":Landroid/os/Bundle;
    .local v14, "options":Landroid/os/Bundle;
    :goto_3f
    :try_start_3f
    invoke-static {}, Landroid/app/ActivityThread;->currentActivityThread()Landroid/app/ActivityThread;

    move-result-object v2

    .line 1082
    invoke-virtual {v2}, Landroid/app/ActivityThread;->getApplicationThread()Landroid/app/ActivityThread$ApplicationThread;

    move-result-object v3

    .line 1083
    .local v3, "app":Landroid/app/IApplicationThread;
    invoke-static {}, Landroid/app/ActivityManager;->getService()Landroid/app/IActivityManager;

    move-result-object v2

    iget-object v4, v1, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    iget-object v5, v1, Landroid/app/PendingIntent;->mWhitelistToken:Landroid/os/IBinder;

    .line 1085
    if-eqz v13, :cond_5a

    .line 1086
    new-instance v0, Landroid/app/PendingIntent$FinishedDispatcher;
    :try_end_53
    .catch Landroid/os/RemoteException; {:try_start_3f .. :try_end_53} :catch_6b

    move-object/from16 v15, p5

    :try_start_55
    invoke-direct {v0, v1, v13, v15}, Landroid/app/PendingIntent$FinishedDispatcher;-><init>(Landroid/app/PendingIntent;Landroid/app/PendingIntent$OnFinished;Landroid/os/Handler;)V

    move-object v9, v0

    goto :goto_5d

    .line 1087
    :cond_5a
    move-object/from16 v15, p5

    move-object v9, v0

    .line 1083
    :goto_5d
    move/from16 v6, p2

    move-object/from16 v7, p3

    move-object/from16 v10, p6

    move-object v11, v14

    invoke-interface/range {v2 .. v11}, Landroid/app/IActivityManager;->sendIntentSender(Landroid/app/IApplicationThread;Landroid/content/IIntentSender;Landroid/os/IBinder;ILandroid/content/Intent;Ljava/lang/String;Landroid/content/IIntentReceiver;Ljava/lang/String;Landroid/os/Bundle;)I

    move-result v0
    :try_end_68
    .catch Landroid/os/RemoteException; {:try_start_55 .. :try_end_68} :catch_69

    return v0

    .line 1089
    .end local v3    # "app":Landroid/app/IApplicationThread;
    .end local v8    # "resolvedType":Ljava/lang/String;
    :catch_69
    move-exception v0

    goto :goto_6e

    :catch_6b
    move-exception v0

    move-object/from16 v15, p5

    :goto_6e
    move-object v2, v14

    .line 1090
    .end local v14    # "options":Landroid/os/Bundle;
    .local v0, "e":Landroid/os/RemoteException;
    .restart local v2    # "options":Landroid/os/Bundle;
    :goto_6f
    new-instance v3, Landroid/app/PendingIntent$CanceledException;

    invoke-direct {v3, v0}, Landroid/app/PendingIntent$CanceledException;-><init>(Ljava/lang/Exception;)V

    throw v3
.end method

.method public toString()Ljava/lang/String;
    .registers 3

    .line 1432
    new-instance v0, Ljava/lang/StringBuilder;

    const/16 v1, 0x80

    invoke-direct {v0, v1}, Ljava/lang/StringBuilder;-><init>(I)V

    .line 1433
    .local v0, "sb":Ljava/lang/StringBuilder;
    const-string v1, "PendingIntent{"

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 1434
    invoke-static {p0}, Ljava/lang/System;->identityHashCode(Ljava/lang/Object;)I

    move-result v1

    invoke-static {v1}, Ljava/lang/Integer;->toHexString(I)Ljava/lang/String;

    move-result-object v1

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 1435
    const-string v1, ": "

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 1436
    iget-object v1, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    invoke-interface {v1}, Landroid/content/IIntentSender;->asBinder()Landroid/os/IBinder;

    move-result-object v1

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/Object;)Ljava/lang/StringBuilder;

    .line 1437
    const/16 v1, 0x7d

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(C)Ljava/lang/StringBuilder;

    .line 1438
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v1

    return-object v1
.end method

.method public unregisterCancelListener(Landroid/app/PendingIntent$CancelListener;)V
    .registers 2
    .param p1, "cancelListener"    # Landroid/app/PendingIntent$CancelListener;
    .annotation runtime Ljava/lang/Deprecated;
    .end annotation

    .line 1222
    invoke-virtual {p0, p1}, Landroid/app/PendingIntent;->removeCancelListener(Landroid/app/PendingIntent$CancelListener;)V

    .line 1223
    return-void
.end method

.method public writeToParcel(Landroid/os/Parcel;I)V
    .registers 4
    .param p1, "out"    # Landroid/os/Parcel;
    .param p2, "flags"    # I

    .line 1453
    iget-object v0, p0, Landroid/app/PendingIntent;->mTarget:Landroid/content/IIntentSender;

    invoke-interface {v0}, Landroid/content/IIntentSender;->asBinder()Landroid/os/IBinder;

    move-result-object v0

    invoke-virtual {p1, v0}, Landroid/os/Parcel;->writeStrongBinder(Landroid/os/IBinder;)V

    .line 1454
    sget-object v0, Landroid/app/PendingIntent;->sOnMarshaledListener:Ljava/lang/ThreadLocal;

    invoke-virtual {v0}, Ljava/lang/ThreadLocal;->get()Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/app/PendingIntent$OnMarshaledListener;

    .line 1455
    .local v0, "listener":Landroid/app/PendingIntent$OnMarshaledListener;
    if-eqz v0, :cond_16

    .line 1456
    invoke-interface {v0, p0, p1, p2}, Landroid/app/PendingIntent$OnMarshaledListener;->onMarshaled(Landroid/app/PendingIntent;Landroid/os/Parcel;I)V

    .line 1459
    :cond_16
    return-void
.end method
