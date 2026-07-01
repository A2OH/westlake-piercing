.class public final Landroid/media/audiopolicy/AudioProductStrategy;
.super Ljava/lang/Object;
.source "AudioProductStrategy.java"

# interfaces
.implements Landroid/os/Parcelable;


# annotations
.annotation runtime Landroid/annotation/SystemApi;
.end annotation

.annotation system Ldalvik/annotation/MemberClasses;
    value = {
        Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    }
.end annotation


# static fields
.field public static final CREATOR:Landroid/os/Parcelable$Creator;
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "Landroid/os/Parcelable$Creator<",
            "Landroid/media/audiopolicy/AudioProductStrategy;",
            ">;"
        }
    .end annotation
.end field

.field private static final DEFAULT_ATTRIBUTES:Landroid/media/AudioAttributes;

.field public static final DEFAULT_GROUP:I = -0x1

.field private static final TAG:Ljava/lang/String; = "AudioProductStrategy"

.field private static sAudioProductStrategies:Ljava/util/List;
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "Ljava/util/List<",
            "Landroid/media/audiopolicy/AudioProductStrategy;",
            ">;"
        }
    .end annotation
.end field

.field private static final sLock:Ljava/lang/Object;


# instance fields
.field private final mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

.field private mId:I

.field private final mName:Ljava/lang/String;


# direct methods
.method static bridge synthetic -$$Nest$sfgetDEFAULT_ATTRIBUTES()Landroid/media/AudioAttributes;
    .registers 1

    sget-object v0, Landroid/media/audiopolicy/AudioProductStrategy;->DEFAULT_ATTRIBUTES:Landroid/media/AudioAttributes;

    return-object v0
.end method

.method static bridge synthetic -$$Nest$smattributesMatches(Landroid/media/AudioAttributes;Landroid/media/AudioAttributes;)Z
    .registers 2

    invoke-static {p0, p1}, Landroid/media/audiopolicy/AudioProductStrategy;->attributesMatches(Landroid/media/AudioAttributes;Landroid/media/AudioAttributes;)Z

    move-result p0

    return p0
.end method

.method static constructor <clinit>()V
    .registers 1

    .line 63
    new-instance v0, Ljava/lang/Object;

    invoke-direct {v0}, Ljava/lang/Object;-><init>()V

    sput-object v0, Landroid/media/audiopolicy/AudioProductStrategy;->sLock:Ljava/lang/Object;

    .line 370
    new-instance v0, Landroid/media/audiopolicy/AudioProductStrategy$1;

    invoke-direct {v0}, Landroid/media/audiopolicy/AudioProductStrategy$1;-><init>()V

    sput-object v0, Landroid/media/audiopolicy/AudioProductStrategy;->CREATOR:Landroid/os/Parcelable$Creator;

    .line 408
    new-instance v0, Landroid/media/AudioAttributes$Builder;

    invoke-direct {v0}, Landroid/media/AudioAttributes$Builder;-><init>()V

    .line 409
    invoke-virtual {v0}, Landroid/media/AudioAttributes$Builder;->build()Landroid/media/AudioAttributes;

    move-result-object v0

    sput-object v0, Landroid/media/audiopolicy/AudioProductStrategy;->DEFAULT_ATTRIBUTES:Landroid/media/AudioAttributes;

    .line 408
    return-void
.end method

.method private constructor <init>(Ljava/lang/String;I[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;)V
    .registers 5
    .param p1, "name"    # Ljava/lang/String;
    .param p2, "id"    # I
    .param p3, "aag"    # [Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    .line 222
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 223
    const-string v0, "name must not be null"

    invoke-static {p1, v0}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;

    .line 224
    const-string v0, "AudioAttributesGroups must not be null"

    invoke-static {p3, v0}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;

    .line 225
    iput-object p1, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mName:Ljava/lang/String;

    .line 226
    iput p2, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mId:I

    .line 227
    iput-object p3, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    .line 228
    return-void
.end method

.method synthetic constructor <init>(Ljava/lang/String;I[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;Landroid/media/audiopolicy/AudioProductStrategy-IA;)V
    .registers 5

    invoke-direct {p0, p1, p2, p3}, Landroid/media/audiopolicy/AudioProductStrategy;-><init>(Ljava/lang/String;I[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;)V

    return-void
.end method

.method private static attributesMatches(Landroid/media/AudioAttributes;Landroid/media/AudioAttributes;)Z
    .registers 7
    .param p0, "refAttr"    # Landroid/media/AudioAttributes;
    .param p1, "attr"    # Landroid/media/AudioAttributes;

    .line 428
    const-string v0, "reference AudioAttributes must not be null"

    invoke-static {p0, v0}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;

    .line 429
    const-string v0, "requester\'s AudioAttributes must not be null"

    invoke-static {p1, v0}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;

    .line 430
    invoke-virtual {p0}, Landroid/media/AudioAttributes;->getTags()Ljava/util/Set;

    move-result-object v0

    const-string v1, ";"

    invoke-static {v1, v0}, Landroid/text/TextUtils;->join(Ljava/lang/CharSequence;Ljava/lang/Iterable;)Ljava/lang/String;

    move-result-object v0

    .line 431
    .local v0, "refFormattedTags":Ljava/lang/String;
    invoke-virtual {p1}, Landroid/media/AudioAttributes;->getTags()Ljava/util/Set;

    move-result-object v2

    invoke-static {v1, v2}, Landroid/text/TextUtils;->join(Ljava/lang/CharSequence;Ljava/lang/Iterable;)Ljava/lang/String;

    move-result-object v1

    .line 432
    .local v1, "cliFormattedTags":Ljava/lang/String;
    sget-object v2, Landroid/media/audiopolicy/AudioProductStrategy;->DEFAULT_ATTRIBUTES:Landroid/media/AudioAttributes;

    invoke-virtual {p0, v2}, Landroid/media/AudioAttributes;->equals(Ljava/lang/Object;)Z

    move-result v2

    const/4 v3, 0x0

    if-eqz v2, :cond_26

    .line 433
    return v3

    .line 435
    :cond_26
    invoke-virtual {p0}, Landroid/media/AudioAttributes;->getSystemUsage()I

    move-result v2

    if-eqz v2, :cond_36

    .line 436
    invoke-virtual {p1}, Landroid/media/AudioAttributes;->getSystemUsage()I

    move-result v2

    invoke-virtual {p0}, Landroid/media/AudioAttributes;->getSystemUsage()I

    move-result v4

    if-ne v2, v4, :cond_6e

    .line 437
    :cond_36
    invoke-virtual {p0}, Landroid/media/AudioAttributes;->getContentType()I

    move-result v2

    if-eqz v2, :cond_46

    .line 438
    invoke-virtual {p1}, Landroid/media/AudioAttributes;->getContentType()I

    move-result v2

    invoke-virtual {p0}, Landroid/media/AudioAttributes;->getContentType()I

    move-result v4

    if-ne v2, v4, :cond_6e

    .line 439
    :cond_46
    invoke-virtual {p0}, Landroid/media/AudioAttributes;->getAllFlags()I

    move-result v2

    if-eqz v2, :cond_61

    .line 440
    invoke-virtual {p1}, Landroid/media/AudioAttributes;->getAllFlags()I

    move-result v2

    if-eqz v2, :cond_6e

    .line 441
    invoke-virtual {p1}, Landroid/media/AudioAttributes;->getAllFlags()I

    move-result v2

    invoke-virtual {p0}, Landroid/media/AudioAttributes;->getAllFlags()I

    move-result v4

    and-int/2addr v2, v4

    invoke-virtual {p0}, Landroid/media/AudioAttributes;->getAllFlags()I

    move-result v4

    if-ne v2, v4, :cond_6e

    .line 442
    :cond_61
    invoke-virtual {v0}, Ljava/lang/String;->length()I

    move-result v2

    if-eqz v2, :cond_6f

    invoke-virtual {v0, v1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-eqz v2, :cond_6e

    goto :goto_6f

    :cond_6e
    goto :goto_70

    :cond_6f
    :goto_6f
    const/4 v3, 0x1

    .line 435
    :goto_70
    return v3
.end method

.method public static createInvalidAudioProductStrategy(I)Landroid/media/audiopolicy/AudioProductStrategy;
    .registers 4
    .param p0, "id"    # I
    .annotation runtime Landroid/annotation/SystemApi;
    .end annotation

    .line 113
    new-instance v0, Landroid/media/audiopolicy/AudioProductStrategy;

    const/4 v1, 0x0

    new-array v1, v1, [Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    const-string v2, "dummy strategy"

    invoke-direct {v0, v2, p0, v1}, Landroid/media/audiopolicy/AudioProductStrategy;-><init>(Ljava/lang/String;I[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;)V

    return-object v0
.end method

.method public static getAudioAttributesForStrategyWithLegacyStreamType(I)Landroid/media/AudioAttributes;
    .registers 4
    .param p0, "streamType"    # I

    .line 126
    invoke-static {}, Landroid/media/audiopolicy/AudioProductStrategy;->getAudioProductStrategies()Ljava/util/List;

    move-result-object v0

    invoke-interface {v0}, Ljava/util/List;->iterator()Ljava/util/Iterator;

    move-result-object v0

    :goto_8
    invoke-interface {v0}, Ljava/util/Iterator;->hasNext()Z

    move-result v1

    if-eqz v1, :cond_1c

    invoke-interface {v0}, Ljava/util/Iterator;->next()Ljava/lang/Object;

    move-result-object v1

    check-cast v1, Landroid/media/audiopolicy/AudioProductStrategy;

    .line 127
    .local v1, "productStrategy":Landroid/media/audiopolicy/AudioProductStrategy;
    invoke-virtual {v1, p0}, Landroid/media/audiopolicy/AudioProductStrategy;->getAudioAttributesForLegacyStreamType(I)Landroid/media/AudioAttributes;

    move-result-object v2

    .line 128
    .local v2, "aa":Landroid/media/AudioAttributes;
    if-eqz v2, :cond_1b

    .line 129
    return-object v2

    .line 131
    .end local v1    # "productStrategy":Landroid/media/audiopolicy/AudioProductStrategy;
    .end local v2    # "aa":Landroid/media/AudioAttributes;
    :cond_1b
    goto :goto_8

    .line 132
    :cond_1c
    sget-object v0, Landroid/media/audiopolicy/AudioProductStrategy;->DEFAULT_ATTRIBUTES:Landroid/media/AudioAttributes;

    return-object v0
.end method

.method public static getAudioProductStrategies()Ljava/util/List;
    .registers 2
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "()",
            "Ljava/util/List<",
            "Landroid/media/audiopolicy/AudioProductStrategy;",
            ">;"
        }
    .end annotation

    .line 74
    sget-object v0, Landroid/media/audiopolicy/AudioProductStrategy;->sAudioProductStrategies:Ljava/util/List;

    if-nez v0, :cond_16

    .line 75
    sget-object v0, Landroid/media/audiopolicy/AudioProductStrategy;->sLock:Ljava/lang/Object;

    monitor-enter v0

    .line 76
    :try_start_7
    sget-object v1, Landroid/media/audiopolicy/AudioProductStrategy;->sAudioProductStrategies:Ljava/util/List;

    if-nez v1, :cond_11

    .line 77
    invoke-static {}, Landroid/media/audiopolicy/AudioProductStrategy;->initializeAudioProductStrategies()Ljava/util/List;

    move-result-object v1

    sput-object v1, Landroid/media/audiopolicy/AudioProductStrategy;->sAudioProductStrategies:Ljava/util/List;

    .line 79
    :cond_11
    monitor-exit v0

    goto :goto_16

    :catchall_13
    move-exception v1

    monitor-exit v0
    :try_end_15
    .catchall {:try_start_7 .. :try_end_15} :catchall_13

    throw v1

    .line 81
    :cond_16
    :goto_16
    sget-object v0, Landroid/media/audiopolicy/AudioProductStrategy;->sAudioProductStrategies:Ljava/util/List;

    return-object v0
.end method

.method public static getAudioProductStrategyWithId(I)Landroid/media/audiopolicy/AudioProductStrategy;
    .registers 5
    .param p0, "id"    # I

    .line 92
    sget-object v0, Landroid/media/audiopolicy/AudioProductStrategy;->sLock:Ljava/lang/Object;

    monitor-enter v0

    .line 93
    :try_start_3
    sget-object v1, Landroid/media/audiopolicy/AudioProductStrategy;->sAudioProductStrategies:Ljava/util/List;

    if-nez v1, :cond_d

    .line 94
    invoke-static {}, Landroid/media/audiopolicy/AudioProductStrategy;->initializeAudioProductStrategies()Ljava/util/List;

    move-result-object v1

    sput-object v1, Landroid/media/audiopolicy/AudioProductStrategy;->sAudioProductStrategies:Ljava/util/List;

    .line 96
    :cond_d
    sget-object v1, Landroid/media/audiopolicy/AudioProductStrategy;->sAudioProductStrategies:Ljava/util/List;

    invoke-interface {v1}, Ljava/util/List;->iterator()Ljava/util/Iterator;

    move-result-object v1

    :goto_13
    invoke-interface {v1}, Ljava/util/Iterator;->hasNext()Z

    move-result v2

    if-eqz v2, :cond_28

    invoke-interface {v1}, Ljava/util/Iterator;->next()Ljava/lang/Object;

    move-result-object v2

    check-cast v2, Landroid/media/audiopolicy/AudioProductStrategy;

    .line 97
    .local v2, "strategy":Landroid/media/audiopolicy/AudioProductStrategy;
    invoke-virtual {v2}, Landroid/media/audiopolicy/AudioProductStrategy;->getId()I

    move-result v3

    if-ne v3, p0, :cond_27

    .line 98
    monitor-exit v0

    return-object v2

    .line 100
    .end local v2    # "strategy":Landroid/media/audiopolicy/AudioProductStrategy;
    :cond_27
    goto :goto_13

    .line 101
    :cond_28
    monitor-exit v0

    .line 102
    const/4 v0, 0x0

    return-object v0

    .line 101
    :catchall_2b
    move-exception v1

    monitor-exit v0
    :try_end_2d
    .catchall {:try_start_3 .. :try_end_2d} :catchall_2b

    throw v1
.end method

.method public static getDefaultAttributes()Landroid/media/AudioAttributes;
    .registers 1

    .line 416
    sget-object v0, Landroid/media/audiopolicy/AudioProductStrategy;->DEFAULT_ATTRIBUTES:Landroid/media/AudioAttributes;

    return-object v0
.end method

.method public static getLegacyStreamTypeForStrategyWithAudioAttributes(Landroid/media/AudioAttributes;)I
    .registers 6
    .param p0, "audioAttributes"    # Landroid/media/AudioAttributes;

    .line 143
    const-string v0, "AudioAttributes must not be null"

    invoke-static {p0, v0}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;

    .line 145
    invoke-static {}, Landroid/media/audiopolicy/AudioProductStrategy;->getAudioProductStrategies()Ljava/util/List;

    move-result-object v0

    invoke-interface {v0}, Ljava/util/List;->iterator()Ljava/util/Iterator;

    move-result-object v0

    :goto_d
    invoke-interface {v0}, Ljava/util/Iterator;->hasNext()Z

    move-result v1

    const/4 v2, 0x3

    if-eqz v1, :cond_60

    invoke-interface {v0}, Ljava/util/Iterator;->next()Ljava/lang/Object;

    move-result-object v1

    check-cast v1, Landroid/media/audiopolicy/AudioProductStrategy;

    .line 146
    .local v1, "productStrategy":Landroid/media/audiopolicy/AudioProductStrategy;
    invoke-virtual {v1, p0}, Landroid/media/audiopolicy/AudioProductStrategy;->supportsAudioAttributes(Landroid/media/AudioAttributes;)Z

    move-result v3

    if-eqz v3, :cond_5f

    .line 147
    invoke-virtual {v1, p0}, Landroid/media/audiopolicy/AudioProductStrategy;->getLegacyStreamTypeForAudioAttributes(Landroid/media/AudioAttributes;)I

    move-result v3

    .line 149
    .local v3, "streamType":I
    const/4 v4, -0x1

    if-ne v3, v4, :cond_58

    .line 150
    new-instance v0, Ljava/lang/StringBuilder;

    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V

    const-string v4, "Attributes "

    invoke-virtual {v0, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v0

    invoke-virtual {p0}, Landroid/media/AudioAttributes;->toString()Ljava/lang/String;

    move-result-object v4

    invoke-virtual {v0, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v0

    const-string v4, " ported by strategy "

    invoke-virtual {v0, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v0

    .line 151
    invoke-virtual {v1}, Landroid/media/audiopolicy/AudioProductStrategy;->getId()I

    move-result v4

    invoke-virtual {v0, v4}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    move-result-object v0

    const-string v4, " has no stream type associated, DO NOT USE STREAM TO CONTROL THE VOLUME"

    invoke-virtual {v0, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v0

    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v0

    .line 150
    const-string v4, "AudioProductStrategy"

    invoke-static {v4, v0}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;)I

    .line 153
    return v2

    .line 155
    :cond_58
    invoke-static {}, Landroid/media/AudioSystem;->getNumStreamTypes()I

    move-result v2

    if-ge v3, v2, :cond_5f

    .line 156
    return v3

    .line 159
    .end local v1    # "productStrategy":Landroid/media/audiopolicy/AudioProductStrategy;
    .end local v3    # "streamType":I
    :cond_5f
    goto :goto_d

    .line 160
    :cond_60
    return v2
.end method

.method public static getVolumeGroupIdForAudioAttributes(Landroid/media/AudioAttributes;Z)I
    .registers 4
    .param p0, "attributes"    # Landroid/media/AudioAttributes;
    .param p1, "fallbackOnDefault"    # Z

    .line 176
    const-string v0, "attributes must not be null"

    invoke-static {p0, v0}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;

    .line 177
    invoke-static {p0}, Landroid/media/audiopolicy/AudioProductStrategy;->getVolumeGroupIdForAudioAttributesInt(Landroid/media/AudioAttributes;)I

    move-result v0

    .line 178
    .local v0, "volumeGroupId":I
    const/4 v1, -0x1

    if-eq v0, v1, :cond_d

    .line 179
    return v0

    .line 181
    :cond_d
    if-eqz p1, :cond_18

    .line 182
    invoke-static {}, Landroid/media/audiopolicy/AudioProductStrategy;->getDefaultAttributes()Landroid/media/AudioAttributes;

    move-result-object v1

    invoke-static {v1}, Landroid/media/audiopolicy/AudioProductStrategy;->getVolumeGroupIdForAudioAttributesInt(Landroid/media/AudioAttributes;)I

    move-result v1

    return v1

    .line 184
    :cond_18
    return v1
.end method

.method private static getVolumeGroupIdForAudioAttributesInt(Landroid/media/AudioAttributes;)I
    .registers 5
    .param p0, "attributes"    # Landroid/media/AudioAttributes;

    .line 344
    const-string v0, "attributes must not be null"

    invoke-static {p0, v0}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;

    .line 345
    invoke-static {}, Landroid/media/audiopolicy/AudioProductStrategy;->getAudioProductStrategies()Ljava/util/List;

    move-result-object v0

    invoke-interface {v0}, Ljava/util/List;->iterator()Ljava/util/Iterator;

    move-result-object v0

    :goto_d
    invoke-interface {v0}, Ljava/util/Iterator;->hasNext()Z

    move-result v1

    const/4 v2, -0x1

    if-eqz v1, :cond_22

    invoke-interface {v0}, Ljava/util/Iterator;->next()Ljava/lang/Object;

    move-result-object v1

    check-cast v1, Landroid/media/audiopolicy/AudioProductStrategy;

    .line 346
    .local v1, "productStrategy":Landroid/media/audiopolicy/AudioProductStrategy;
    invoke-virtual {v1, p0}, Landroid/media/audiopolicy/AudioProductStrategy;->getVolumeGroupIdForAudioAttributes(Landroid/media/AudioAttributes;)I

    move-result v3

    .line 347
    .local v3, "volumeGroupId":I
    if-eq v3, v2, :cond_21

    .line 348
    return v3

    .line 350
    .end local v1    # "productStrategy":Landroid/media/audiopolicy/AudioProductStrategy;
    .end local v3    # "volumeGroupId":I
    :cond_21
    goto :goto_d

    .line 351
    :cond_22
    return v2
.end method

.method private static initializeAudioProductStrategies()Ljava/util/List;
    .registers 1
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "()",
            "Ljava/util/List<",
            "Landroid/media/audiopolicy/AudioProductStrategy;",
            ">;"
        }
    .end annotation

    new-instance v0, Ljava/util/ArrayList;

    invoke-direct {v0}, Ljava/util/ArrayList;-><init>()V

    return-object v0
.end method

.method private static native native_list_audio_product_strategies(Ljava/util/ArrayList;)I
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "(",
            "Ljava/util/ArrayList<",
            "Landroid/media/audiopolicy/AudioProductStrategy;",
            ">;)I"
        }
    .end annotation
.end method


# virtual methods
.method public describeContents()I
    .registers 2

    .line 356
    const/4 v0, 0x0

    return v0
.end method

.method public equals(Ljava/lang/Object;)Z
    .registers 7
    .param p1, "o"    # Ljava/lang/Object;

    .line 201
    const/4 v0, 0x1

    if-ne p0, p1, :cond_4

    return v0

    .line 202
    :cond_4
    const/4 v1, 0x0

    if-eqz p1, :cond_32

    invoke-virtual {p0}, Ljava/lang/Object;->getClass()Ljava/lang/Class;

    move-result-object v2

    invoke-virtual {p1}, Ljava/lang/Object;->getClass()Ljava/lang/Class;

    move-result-object v3

    if-eq v2, v3, :cond_12

    goto :goto_32

    .line 204
    :cond_12
    move-object v2, p1

    check-cast v2, Landroid/media/audiopolicy/AudioProductStrategy;

    .line 206
    .local v2, "thatStrategy":Landroid/media/audiopolicy/AudioProductStrategy;
    iget v3, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mId:I

    iget v4, v2, Landroid/media/audiopolicy/AudioProductStrategy;->mId:I

    if-ne v3, v4, :cond_30

    iget-object v3, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mName:Ljava/lang/String;

    iget-object v4, v2, Landroid/media/audiopolicy/AudioProductStrategy;->mName:Ljava/lang/String;

    .line 207
    invoke-static {v3, v4}, Ljava/util/Objects;->equals(Ljava/lang/Object;Ljava/lang/Object;)Z

    move-result v3

    if-eqz v3, :cond_30

    iget-object v3, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    iget-object v4, v2, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    .line 208
    invoke-static {v3, v4}, Ljava/util/Arrays;->equals([Ljava/lang/Object;[Ljava/lang/Object;)Z

    move-result v3

    if-eqz v3, :cond_30

    goto :goto_31

    :cond_30
    move v0, v1

    .line 206
    :goto_31
    return v0

    .line 202
    .end local v2    # "thatStrategy":Landroid/media/audiopolicy/AudioProductStrategy;
    :cond_32
    :goto_32
    return v1
.end method

.method public getAudioAttributes()Landroid/media/AudioAttributes;
    .registers 3
    .annotation runtime Landroid/annotation/SystemApi;
    .end annotation

    .line 257
    iget-object v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    array-length v1, v0

    if-nez v1, :cond_8

    sget-object v0, Landroid/media/audiopolicy/AudioProductStrategy;->DEFAULT_ATTRIBUTES:Landroid/media/AudioAttributes;

    goto :goto_f

    .line 258
    :cond_8
    const/4 v1, 0x0

    aget-object v0, v0, v1

    invoke-virtual {v0}, Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;->getAudioAttributes()Landroid/media/AudioAttributes;

    move-result-object v0

    .line 257
    :goto_f
    return-object v0
.end method

.method public getAudioAttributesForLegacyStreamType(I)Landroid/media/AudioAttributes;
    .registers 7
    .param p1, "streamType"    # I

    .line 268
    iget-object v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    array-length v1, v0

    const/4 v2, 0x0

    :goto_4
    if-ge v2, v1, :cond_16

    aget-object v3, v0, v2

    .line 269
    .local v3, "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    invoke-virtual {v3, p1}, Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;->supportsStreamType(I)Z

    move-result v4

    if-eqz v4, :cond_13

    .line 270
    invoke-virtual {v3}, Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;->getAudioAttributes()Landroid/media/AudioAttributes;

    move-result-object v0

    return-object v0

    .line 268
    .end local v3    # "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    :cond_13
    add-int/lit8 v2, v2, 0x1

    goto :goto_4

    .line 273
    :cond_16
    const/4 v0, 0x0

    return-object v0
.end method

.method public getId()I
    .registers 2
    .annotation runtime Landroid/annotation/SystemApi;
    .end annotation

    .line 237
    iget v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mId:I

    return v0
.end method

.method public getLegacyStreamTypeForAudioAttributes(Landroid/media/AudioAttributes;)I
    .registers 7
    .param p1, "aa"    # Landroid/media/AudioAttributes;

    .line 284
    const-string v0, "AudioAttributes must not be null"

    invoke-static {p1, v0}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;

    .line 285
    iget-object v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    array-length v1, v0

    const/4 v2, 0x0

    :goto_9
    if-ge v2, v1, :cond_1b

    aget-object v3, v0, v2

    .line 286
    .local v3, "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    invoke-virtual {v3, p1}, Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;->supportsAttributes(Landroid/media/AudioAttributes;)Z

    move-result v4

    if-eqz v4, :cond_18

    .line 287
    invoke-virtual {v3}, Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;->getStreamType()I

    move-result v0

    return v0

    .line 285
    .end local v3    # "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    :cond_18
    add-int/lit8 v2, v2, 0x1

    goto :goto_9

    .line 290
    :cond_1b
    const/4 v0, -0x1

    return v0
.end method

.method public getName()Ljava/lang/String;
    .registers 2
    .annotation runtime Landroid/annotation/SystemApi;
    .end annotation

    .line 247
    iget-object v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mName:Ljava/lang/String;

    return-object v0
.end method

.method public getVolumeGroupIdForAudioAttributes(Landroid/media/AudioAttributes;)I
    .registers 7
    .param p1, "aa"    # Landroid/media/AudioAttributes;

    .line 334
    const-string v0, "AudioAttributes must not be null"

    invoke-static {p1, v0}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;

    .line 335
    iget-object v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    array-length v1, v0

    const/4 v2, 0x0

    :goto_9
    if-ge v2, v1, :cond_1b

    aget-object v3, v0, v2

    .line 336
    .local v3, "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    invoke-virtual {v3, p1}, Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;->supportsAttributes(Landroid/media/AudioAttributes;)Z

    move-result v4

    if-eqz v4, :cond_18

    .line 337
    invoke-virtual {v3}, Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;->getVolumeGroupId()I

    move-result v0

    return v0

    .line 335
    .end local v3    # "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    :cond_18
    add-int/lit8 v2, v2, 0x1

    goto :goto_9

    .line 340
    :cond_1b
    const/4 v0, -0x1

    return v0
.end method

.method public getVolumeGroupIdForLegacyStreamType(I)I
    .registers 7
    .param p1, "streamType"    # I

    .line 318
    iget-object v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    array-length v1, v0

    const/4 v2, 0x0

    :goto_4
    if-ge v2, v1, :cond_16

    aget-object v3, v0, v2

    .line 319
    .local v3, "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    invoke-virtual {v3, p1}, Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;->supportsStreamType(I)Z

    move-result v4

    if-eqz v4, :cond_13

    .line 320
    invoke-virtual {v3}, Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;->getVolumeGroupId()I

    move-result v0

    return v0

    .line 318
    .end local v3    # "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    :cond_13
    add-int/lit8 v2, v2, 0x1

    goto :goto_4

    .line 323
    :cond_16
    const/4 v0, -0x1

    return v0
.end method

.method public hashCode()I
    .registers 4

    .line 213
    iget v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mId:I

    invoke-static {v0}, Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;

    move-result-object v0

    iget-object v1, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mName:Ljava/lang/String;

    iget-object v2, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    invoke-static {v2}, Ljava/util/Arrays;->hashCode([Ljava/lang/Object;)I

    move-result v2

    invoke-static {v2}, Ljava/lang/Integer;->valueOf(I)Ljava/lang/Integer;

    move-result-object v2

    filled-new-array {v0, v1, v2}, [Ljava/lang/Object;

    move-result-object v0

    invoke-static {v0}, Ljava/util/Objects;->hash([Ljava/lang/Object;)I

    move-result v0

    return v0
.end method

.method public supportsAudioAttributes(Landroid/media/AudioAttributes;)Z
    .registers 8
    .param p1, "aa"    # Landroid/media/AudioAttributes;
    .annotation runtime Landroid/annotation/SystemApi;
    .end annotation

    .line 301
    const-string v0, "AudioAttributes must not be null"

    invoke-static {p1, v0}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;

    .line 302
    iget-object v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    array-length v1, v0

    const/4 v2, 0x0

    move v3, v2

    :goto_a
    if-ge v3, v1, :cond_19

    aget-object v4, v0, v3

    .line 303
    .local v4, "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    invoke-virtual {v4, p1}, Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;->supportsAttributes(Landroid/media/AudioAttributes;)Z

    move-result v5

    if-eqz v5, :cond_16

    .line 304
    const/4 v0, 0x1

    return v0

    .line 302
    .end local v4    # "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    :cond_16
    add-int/lit8 v3, v3, 0x1

    goto :goto_a

    .line 307
    :cond_19
    return v2
.end method

.method public toString()Ljava/lang/String;
    .registers 7

    .line 393
    new-instance v0, Ljava/lang/StringBuilder;

    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V

    .line 394
    .local v0, "s":Ljava/lang/StringBuilder;
    const-string v1, "\n Name: "

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 395
    iget-object v1, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mName:Ljava/lang/String;

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 396
    const-string v1, " Id: "

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 397
    iget v1, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mId:I

    invoke-static {v1}, Ljava/lang/Integer;->toString(I)Ljava/lang/String;

    move-result-object v1

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 398
    iget-object v1, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    array-length v2, v1

    const/4 v3, 0x0

    :goto_21
    if-ge v3, v2, :cond_2f

    aget-object v4, v1, v3

    .line 399
    .local v4, "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    invoke-virtual {v4}, Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;->toString()Ljava/lang/String;

    move-result-object v5

    invoke-virtual {v0, v5}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    .line 398
    .end local v4    # "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    add-int/lit8 v3, v3, 0x1

    goto :goto_21

    .line 401
    :cond_2f
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v1

    return-object v1
.end method

.method public writeToParcel(Landroid/os/Parcel;I)V
    .registers 7
    .param p1, "dest"    # Landroid/os/Parcel;
    .param p2, "flags"    # I

    .line 361
    iget-object v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mName:Ljava/lang/String;

    invoke-virtual {p1, v0}, Landroid/os/Parcel;->writeString(Ljava/lang/String;)V

    .line 362
    iget v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mId:I

    invoke-virtual {p1, v0}, Landroid/os/Parcel;->writeInt(I)V

    .line 363
    iget-object v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    array-length v0, v0

    invoke-virtual {p1, v0}, Landroid/os/Parcel;->writeInt(I)V

    .line 364
    iget-object v0, p0, Landroid/media/audiopolicy/AudioProductStrategy;->mAudioAttributesGroups:[Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;

    array-length v1, v0

    const/4 v2, 0x0

    :goto_14
    if-ge v2, v1, :cond_1e

    aget-object v3, v0, v2

    .line 365
    .local v3, "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    invoke-virtual {v3, p1, p2}, Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;->writeToParcel(Landroid/os/Parcel;I)V

    .line 364
    .end local v3    # "aag":Landroid/media/audiopolicy/AudioProductStrategy$AudioAttributesGroup;
    add-int/lit8 v2, v2, 0x1

    goto :goto_14

    .line 367
    :cond_1e
    return-void
.end method
