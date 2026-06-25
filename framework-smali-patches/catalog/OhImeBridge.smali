.class public final Ladapter/window/OhImeBridge;
.super Ljava/lang/Object;
.source "OhImeBridge.java"


# static fields
.field static final OP_DEL_AFTER:I = 0x2

.field static final OP_DEL_BEFORE:I = 0x1

.field static final OP_ENTER:I = 0x3

.field static final OP_INSERT:I = 0x0

.field private static final TAG:Ljava/lang/String; = "OH_IMEBridge"

.field private static sRootsField:Ljava/lang/reflect/Field;

.field private static volatile sUiHandler:Landroid/os/Handler;

.field private static sViewField:Ljava/lang/reflect/Field;


# direct methods
.method private constructor <init>()V
    .registers 1

    .line 40
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method static focusedInputConnection()Landroid/view/inputmethod/InputConnection;
    .registers 7

    const/4 v0, 0x0

    :try_start_1
    const-string v1, "android.view.WindowManagerGlobal"

    .line 97
    invoke-static {v1}, Ljava/lang/Class;->forName(Ljava/lang/String;)Ljava/lang/Class;

    move-result-object v1

    const-string v2, "getInstance"

    const/4 v3, 0x0

    new-array v4, v3, [Ljava/lang/Class;

    .line 98
    invoke-virtual {v1, v2, v4}, Ljava/lang/Class;->getMethod(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;

    move-result-object v2

    new-array v4, v3, [Ljava/lang/Object;

    invoke-virtual {v2, v0, v4}, Ljava/lang/reflect/Method;->invoke(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v2

    .line 99
    sget-object v4, Ladapter/window/OhImeBridge;->sRootsField:Ljava/lang/reflect/Field;

    const/4 v5, 0x1

    if-nez v4, :cond_26

    const-string v4, "mRoots"

    .line 100
    invoke-virtual {v1, v4}, Ljava/lang/Class;->getDeclaredField(Ljava/lang/String;)Ljava/lang/reflect/Field;

    move-result-object v1

    sput-object v1, Ladapter/window/OhImeBridge;->sRootsField:Ljava/lang/reflect/Field;

    .line 101
    invoke-virtual {v1, v5}, Ljava/lang/reflect/Field;->setAccessible(Z)V

    .line 103
    :cond_26
    sget-object v1, Ladapter/window/OhImeBridge;->sRootsField:Ljava/lang/reflect/Field;

    invoke-virtual {v1, v2}, Ljava/lang/reflect/Field;->get(Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v1

    .line 104
    instance-of v2, v1, Ljava/util/ArrayList;

    if-nez v2, :cond_31

    return-object v0

    .line 105
    :cond_31
    check-cast v1, Ljava/util/ArrayList;

    const-string v2, "android.view.ViewRootImpl"

    .line 106
    invoke-static {v2}, Ljava/lang/Class;->forName(Ljava/lang/String;)Ljava/lang/Class;

    move-result-object v2

    .line 107
    sget-object v4, Ladapter/window/OhImeBridge;->sViewField:Ljava/lang/reflect/Field;

    if-nez v4, :cond_48

    const-string v4, "mView"

    .line 108
    invoke-virtual {v2, v4}, Ljava/lang/Class;->getDeclaredField(Ljava/lang/String;)Ljava/lang/reflect/Field;

    move-result-object v2

    sput-object v2, Ladapter/window/OhImeBridge;->sViewField:Ljava/lang/reflect/Field;

    .line 109
    invoke-virtual {v2, v5}, Ljava/lang/reflect/Field;->setAccessible(Z)V

    :cond_48
    move-object v2, v0

    .line 113
    :goto_49
    invoke-virtual {v1}, Ljava/util/ArrayList;->size()I

    move-result v4

    if-ge v3, v4, :cond_7c

    .line 114
    invoke-virtual {v1, v3}, Ljava/util/ArrayList;->get(I)Ljava/lang/Object;

    move-result-object v4

    if-nez v4, :cond_56

    goto :goto_79

    .line 116
    :cond_56
    sget-object v5, Ladapter/window/OhImeBridge;->sViewField:Ljava/lang/reflect/Field;

    invoke-virtual {v5, v4}, Ljava/lang/reflect/Field;->get(Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v4

    .line 117
    instance-of v5, v4, Landroid/view/View;

    if-nez v5, :cond_61

    goto :goto_79

    .line 118
    :cond_61
    check-cast v4, Landroid/view/View;

    .line 119
    invoke-virtual {v4}, Landroid/view/View;->findFocus()Landroid/view/View;

    move-result-object v5

    if-eqz v5, :cond_79

    .line 120
    invoke-virtual {v5}, Landroid/view/View;->onCheckIsTextEditor()Z

    move-result v6

    if-eqz v6, :cond_79

    .line 121
    invoke-virtual {v4}, Landroid/view/View;->hasWindowFocus()Z

    move-result v4

    if-eqz v4, :cond_76

    goto :goto_7d

    :cond_76
    if-nez v2, :cond_79

    move-object v2, v5

    :cond_79
    :goto_79
    add-int/lit8 v3, v3, 0x1

    goto :goto_49

    :cond_7c
    move-object v5, v0

    :goto_7d
    if-nez v5, :cond_80

    goto :goto_81

    :cond_80
    move-object v2, v5

    :goto_81
    if-nez v2, :cond_8b

    .line 127
    sget-object v1, Ljava/lang/System;->err:Ljava/io/PrintStream;

    const-string v2, "[OH_IMEBridge] focusedInputConnection: no editable focus"

    invoke-virtual {v1, v2}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    return-object v0

    .line 130
    :cond_8b
    new-instance v1, Landroid/view/inputmethod/EditorInfo;

    invoke-direct {v1}, Landroid/view/inputmethod/EditorInfo;-><init>()V

    .line 131
    invoke-virtual {v2, v1}, Landroid/view/View;->onCreateInputConnection(Landroid/view/inputmethod/EditorInfo;)Landroid/view/inputmethod/InputConnection;

    move-result-object v1

    if-nez v1, :cond_b9

    .line 133
    sget-object v3, Ljava/lang/System;->err:Ljava/io/PrintStream;

    .line 134
    invoke-virtual {v2}, Ljava/lang/Object;->getClass()Ljava/lang/Class;

    move-result-object v2

    invoke-virtual {v2}, Ljava/lang/Class;->getName()Ljava/lang/String;

    move-result-object v2

    new-instance v4, Ljava/lang/StringBuilder;

    invoke-direct {v4}, Ljava/lang/StringBuilder;-><init>()V

    const-string v5, "[OH_IMEBridge] focusedInputConnection: view returned null IC ("

    invoke-virtual {v4, v5}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v4, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    const-string v2, ")"

    invoke-virtual {v4, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v4}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v2

    .line 133
    invoke-virtual {v3, v2}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    :try_end_b9
    .catchall {:try_start_1 .. :try_end_b9} :catchall_ba

    :cond_b9
    return-object v1

    :catchall_ba
    move-exception v1

    .line 138
    sget-object v2, Ljava/lang/System;->err:Ljava/io/PrintStream;

    invoke-static {v1}, Ljava/lang/String;->valueOf(Ljava/lang/Object;)Ljava/lang/String;

    move-result-object v1

    new-instance v3, Ljava/lang/StringBuilder;

    const-string v4, "[OH_IMEBridge] focusedInputConnection: "

    invoke-direct {v3, v4}, Ljava/lang/StringBuilder;-><init>(Ljava/lang/String;)V

    invoke-virtual {v3, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v1

    invoke-virtual {v2, v1}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    return-object v0
.end method

.method public static hide()Z
    .registers 4

    const-string v0, "[OH_IMEBridge] hide -> nativeHideKeyboard="

    .line 61
    :try_start_2
    invoke-static {}, Ladapter/window/OhImeBridge;->nativeHideKeyboard()Z

    move-result v1

    .line 62
    sget-object v2, Ljava/lang/System;->err:Ljava/io/PrintStream;

    new-instance v3, Ljava/lang/StringBuilder;

    invoke-direct {v3, v0}, Ljava/lang/StringBuilder;-><init>(Ljava/lang/String;)V

    invoke-virtual {v3, v1}, Ljava/lang/StringBuilder;->append(Z)Ljava/lang/StringBuilder;

    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v0

    invoke-virtual {v2, v0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    :try_end_17
    .catchall {:try_start_2 .. :try_end_17} :catchall_18

    return v1

    :catchall_18
    move-exception v0

    .line 65
    sget-object v1, Ljava/lang/System;->err:Ljava/io/PrintStream;

    invoke-static {v0}, Ljava/lang/String;->valueOf(Ljava/lang/Object;)Ljava/lang/String;

    move-result-object v0

    new-instance v2, Ljava/lang/StringBuilder;

    const-string v3, "[OH_IMEBridge] hide failed: "

    invoke-direct {v2, v3}, Ljava/lang/StringBuilder;-><init>(Ljava/lang/String;)V

    invoke-virtual {v2, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v2}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v0

    invoke-virtual {v1, v0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    const/4 v0, 0x0

    return v0
.end method

.method private static native nativeHideKeyboard()Z
.end method

.method public static nativeOnDeleteAfter(I)V
    .registers 3

    const/4 v0, 0x2

    const/4 v1, 0x0

    .line 180
    invoke-static {v0, p0, v1}, Ladapter/window/OhImeBridge;->runOp(IILjava/lang/String;)V

    return-void
.end method

.method public static nativeOnDeleteBefore(I)V
    .registers 3

    const/4 v0, 0x1

    const/4 v1, 0x0

    .line 177
    invoke-static {v0, p0, v1}, Ladapter/window/OhImeBridge;->runOp(IILjava/lang/String;)V

    return-void
.end method

.method public static nativeOnEnterAction(I)V
    .registers 3

    const/4 v0, 0x3

    const/4 v1, 0x0

    .line 183
    invoke-static {v0, p0, v1}, Ladapter/window/OhImeBridge;->runOp(IILjava/lang/String;)V

    return-void
.end method

.method public static nativeOnInsertText(Ljava/lang/String;)V
    .registers 2

    const/4 v0, 0x0

    .line 174
    invoke-static {v0, v0, p0}, Ladapter/window/OhImeBridge;->runOp(IILjava/lang/String;)V

    return-void
.end method

.method private static native nativeShowKeyboard()Z
.end method

.method private static runOp(IILjava/lang/String;)V
    .registers 5

    .line 144
    invoke-static {}, Ladapter/window/OhImeBridge;->uiHandler()Landroid/os/Handler;

    move-result-object v0

    new-instance v1, Ladapter/window/OhImeBridge$1;

    invoke-direct {v1, p0, p2, p1}, Ladapter/window/OhImeBridge$1;-><init>(ILjava/lang/String;I)V

    invoke-virtual {v0, v1}, Landroid/os/Handler;->post(Ljava/lang/Runnable;)Z

    return-void
.end method

.method public static show()Z
    .registers 4

    const-string v0, "[OH_IMEBridge] show -> nativeShowKeyboard="

    .line 49
    :try_start_2
    invoke-static {}, Ladapter/window/OhImeBridge;->nativeShowKeyboard()Z

    move-result v1

    .line 50
    sget-object v2, Ljava/lang/System;->err:Ljava/io/PrintStream;

    new-instance v3, Ljava/lang/StringBuilder;

    invoke-direct {v3, v0}, Ljava/lang/StringBuilder;-><init>(Ljava/lang/String;)V

    invoke-virtual {v3, v1}, Ljava/lang/StringBuilder;->append(Z)Ljava/lang/StringBuilder;

    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v0

    invoke-virtual {v2, v0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    :try_end_17
    .catchall {:try_start_2 .. :try_end_17} :catchall_18

    return v1

    :catchall_18
    move-exception v0

    .line 53
    sget-object v1, Ljava/lang/System;->err:Ljava/io/PrintStream;

    invoke-static {v0}, Ljava/lang/String;->valueOf(Ljava/lang/Object;)Ljava/lang/String;

    move-result-object v0

    new-instance v2, Ljava/lang/StringBuilder;

    const-string v3, "[OH_IMEBridge] show failed: "

    invoke-direct {v2, v3}, Ljava/lang/StringBuilder;-><init>(Ljava/lang/String;)V

    invoke-virtual {v2, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v2}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v0

    invoke-virtual {v1, v0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    const/4 v0, 0x0

    return v0
.end method

.method private static uiHandler()Landroid/os/Handler;
    .registers 3

    .line 81
    sget-object v0, Ladapter/window/OhImeBridge;->sUiHandler:Landroid/os/Handler;

    if-nez v0, :cond_1b

    .line 82
    const-class v0, Ladapter/window/OhImeBridge;

    monitor-enter v0

    .line 83
    :try_start_7
    sget-object v1, Ladapter/window/OhImeBridge;->sUiHandler:Landroid/os/Handler;

    if-nez v1, :cond_16

    .line 84
    new-instance v1, Landroid/os/Handler;

    invoke-static {}, Landroid/os/Looper;->getMainLooper()Landroid/os/Looper;

    move-result-object v2

    invoke-direct {v1, v2}, Landroid/os/Handler;-><init>(Landroid/os/Looper;)V

    sput-object v1, Ladapter/window/OhImeBridge;->sUiHandler:Landroid/os/Handler;

    .line 86
    :cond_16
    monitor-exit v0

    goto :goto_1b

    :catchall_18
    move-exception v1

    monitor-exit v0
    :try_end_1a
    .catchall {:try_start_7 .. :try_end_1a} :catchall_18

    throw v1

    .line 88
    :cond_1b
    :goto_1b
    sget-object v0, Ladapter/window/OhImeBridge;->sUiHandler:Landroid/os/Handler;

    return-object v0
.end method
