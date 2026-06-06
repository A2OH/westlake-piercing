.class Landroid/app/SystemServiceRegistry$7;
.super Landroid/app/SystemServiceRegistry$CachedServiceFetcher;
.source "SystemServiceRegistry.java"


# annotations
.annotation system Ldalvik/annotation/EnclosingClass;
    value = Landroid/app/SystemServiceRegistry;
.end annotation

.annotation system Ldalvik/annotation/InnerClass;
    accessFlags = 0x0
    name = null
.end annotation

.annotation system Ldalvik/annotation/Signature;
    value = {
        "Landroid/app/SystemServiceRegistry$CachedServiceFetcher<",
        "Landroid/app/AlarmManager;",
        ">;"
    }
.end annotation


# direct methods
.method constructor <init>()V
    .registers 1

    .line 337
    invoke-direct {p0}, Landroid/app/SystemServiceRegistry$CachedServiceFetcher;-><init>()V

    return-void
.end method


# virtual methods
.method public createService(Landroid/app/ContextImpl;)Landroid/app/AlarmManager;
    .registers 5
    .param p1, "ctx"    # Landroid/app/ContextImpl;
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Landroid/os/ServiceManager$ServiceNotFoundException;
        }
    .end annotation

    .line 340
    const-string v0, "alarm"

    :try_start_smf
    invoke-static {v0}, Landroid/os/ServiceManager;->getServiceOrThrow(Ljava/lang/String;)Landroid/os/IBinder;

    move-result-object v0
    :try_end_smf
    .catch Landroid/os/ServiceManager$ServiceNotFoundException; {:try_start_smf .. :try_end_smf} :catch_smf

    :after_smf
    .line 341
    invoke-static {v0}, Landroid/app/IAlarmManager$Stub;->asInterface(Landroid/os/IBinder;)Landroid/app/IAlarmManager;

    move-result-object v1

    .line 342
    .local v1, "service":Landroid/app/IAlarmManager;
    new-instance v2, Landroid/app/AlarmManager;

    invoke-direct {v2, v1, p1}, Landroid/app/AlarmManager;-><init>(Landroid/app/IAlarmManager;Landroid/content/Context;)V

    return-object v2

    :catch_smf
    move-exception v1

    const/4 v0, 0x0

    goto :after_smf
.end method

.method public bridge synthetic createService(Landroid/app/ContextImpl;)Ljava/lang/Object;
    .registers 2
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Landroid/os/ServiceManager$ServiceNotFoundException;
        }
    .end annotation

    .line 337
    invoke-virtual {p0, p1}, Landroid/app/SystemServiceRegistry$7;->createService(Landroid/app/ContextImpl;)Landroid/app/AlarmManager;

    move-result-object p1

    return-object p1
.end method
