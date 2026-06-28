.class public Landroid/app/job/JobSchedulerStub;
.super Landroid/app/job/JobScheduler;
.source "JobSchedulerStub.java"


# direct methods
.method public constructor <init>()V
    .locals 0

    invoke-direct {p0}, Landroid/app/job/JobScheduler;-><init>()V

    return-void
.end method


# virtual methods
.method public cancel(I)V
    .locals 0

    return-void
.end method

.method public cancelAll()V
    .locals 0

    return-void
.end method

.method public enqueue(Landroid/app/job/JobInfo;Landroid/app/job/JobWorkItem;)I
    .locals 0

    const/4 p0, 0x1

    return p0
.end method

.method public forNamespace(Ljava/lang/String;)Landroid/app/job/JobScheduler;
    .locals 0

    return-object p0
.end method

.method public getAllJobSnapshots()Ljava/util/List;
    .locals 0

    new-instance p0, Ljava/util/ArrayList;

    invoke-direct {p0}, Ljava/util/ArrayList;-><init>()V

    return-object p0
.end method

.method public getAllPendingJobs()Ljava/util/List;
    .locals 0

    new-instance p0, Ljava/util/ArrayList;

    invoke-direct {p0}, Ljava/util/ArrayList;-><init>()V

    return-object p0
.end method

.method public getPendingJob(I)Landroid/app/job/JobInfo;
    .locals 0

    const/4 p0, 0x0

    return-object p0
.end method

.method public getStartedJobs()Ljava/util/List;
    .locals 0

    new-instance p0, Ljava/util/ArrayList;

    invoke-direct {p0}, Ljava/util/ArrayList;-><init>()V

    return-object p0
.end method

.method public notePendingUserRequestedAppStop(Ljava/lang/String;ILjava/lang/String;)V
    .locals 0

    return-void
.end method

.method public registerUserVisibleJobObserver(Landroid/app/job/IUserVisibleJobObserver;)V
    .locals 0

    return-void
.end method

.method public schedule(Landroid/app/job/JobInfo;)I
    .locals 0

    const/4 p0, 0x1

    return p0
.end method

.method public scheduleAsPackage(Landroid/app/job/JobInfo;Ljava/lang/String;ILjava/lang/String;)I
    .locals 0

    const/4 p0, 0x1

    return p0
.end method

.method public unregisterUserVisibleJobObserver(Landroid/app/job/IUserVisibleJobObserver;)V
    .locals 0

    return-void
.end method
