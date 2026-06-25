.class Ladapter/window/OhImeBridge$1;
.super Ljava/lang/Object;
.source "OhImeBridge.java"

# interfaces
.implements Ljava/lang/Runnable;


# annotations
.annotation system Ldalvik/annotation/EnclosingMethod;
    value = Ladapter/window/OhImeBridge;->runOp(IILjava/lang/String;)V
.end annotation

.annotation system Ldalvik/annotation/InnerClass;
    accessFlags = 0x0
    name = null
.end annotation


# instance fields
.field final synthetic val$arg:I

.field final synthetic val$op:I

.field final synthetic val$text:Ljava/lang/String;


# direct methods
.method constructor <init>(ILjava/lang/String;I)V
    .registers 4
    .annotation system Ldalvik/annotation/Signature;
        value = {
            "()V"
        }
    .end annotation

    .line 144
    iput p1, p0, Ladapter/window/OhImeBridge$1;->val$op:I

    iput-object p2, p0, Ladapter/window/OhImeBridge$1;->val$text:Ljava/lang/String;

    iput p3, p0, Ladapter/window/OhImeBridge$1;->val$arg:I

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method


# virtual methods
.method public run()V
    .registers 14

    const-string v0, "[OH_IMEBridge] runOp("

    .line 147
    :try_start_2
    invoke-static {}, Ladapter/window/OhImeBridge;->focusedInputConnection()Landroid/view/inputmethod/InputConnection;

    move-result-object v1

    if-nez v1, :cond_9

    return-void

    .line 149
    :cond_9
    invoke-interface {v1}, Landroid/view/inputmethod/InputConnection;->beginBatchEdit()Z

    .line 150
    invoke-interface {v1}, Landroid/view/inputmethod/InputConnection;->finishComposingText()Z

    .line 151
    iget v2, p0, Ladapter/window/OhImeBridge$1;->val$op:I

    const/4 v3, 0x1

    if-nez v2, :cond_1a

    .line 152
    iget-object v2, p0, Ladapter/window/OhImeBridge$1;->val$text:Ljava/lang/String;

    invoke-interface {v1, v2, v3}, Landroid/view/inputmethod/InputConnection;->commitText(Ljava/lang/CharSequence;I)Z

    goto :goto_57

    :cond_1a
    const/4 v4, 0x0

    if-ne v2, v3, :cond_26

    .line 154
    iget v2, p0, Ladapter/window/OhImeBridge$1;->val$arg:I

    if-lez v2, :cond_22

    move v3, v2

    :cond_22
    invoke-interface {v1, v3, v4}, Landroid/view/inputmethod/InputConnection;->deleteSurroundingText(II)Z

    goto :goto_57

    :cond_26
    const/4 v5, 0x2

    if-ne v2, v5, :cond_32

    .line 156
    iget v2, p0, Ladapter/window/OhImeBridge$1;->val$arg:I

    if-lez v2, :cond_2e

    move v3, v2

    :cond_2e
    invoke-interface {v1, v4, v3}, Landroid/view/inputmethod/InputConnection;->deleteSurroundingText(II)Z

    goto :goto_57

    :cond_32
    const/4 v3, 0x3

    if-ne v2, v3, :cond_57

    .line 158
    invoke-static {}, Landroid/os/SystemClock;->uptimeMillis()J

    move-result-wide v2

    .line 159
    new-instance v12, Landroid/view/KeyEvent;

    const/4 v9, 0x0

    const/16 v10, 0x42

    const/4 v11, 0x0

    move-object v4, v12

    move-wide v5, v2

    move-wide v7, v2

    invoke-direct/range {v4 .. v11}, Landroid/view/KeyEvent;-><init>(JJIII)V

    invoke-interface {v1, v12}, Landroid/view/inputmethod/InputConnection;->sendKeyEvent(Landroid/view/KeyEvent;)Z

    .line 161
    new-instance v12, Landroid/view/KeyEvent;

    const/4 v9, 0x1

    const/16 v10, 0x42

    const/4 v11, 0x0

    move-object v4, v12

    move-wide v5, v2

    move-wide v7, v2

    invoke-direct/range {v4 .. v11}, Landroid/view/KeyEvent;-><init>(JJIII)V

    invoke-interface {v1, v12}, Landroid/view/inputmethod/InputConnection;->sendKeyEvent(Landroid/view/KeyEvent;)Z

    .line 164
    :cond_57
    :goto_57
    invoke-interface {v1}, Landroid/view/inputmethod/InputConnection;->endBatchEdit()Z

    .line 165
    sget-object v1, Ljava/lang/System;->err:Ljava/io/PrintStream;

    iget v2, p0, Ladapter/window/OhImeBridge$1;->val$op:I

    new-instance v3, Ljava/lang/StringBuilder;

    invoke-direct {v3, v0}, Ljava/lang/StringBuilder;-><init>(Ljava/lang/String;)V

    invoke-virtual {v3, v2}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    const-string v2, ") done"

    invoke-virtual {v3, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v2

    invoke-virtual {v1, v2}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    :try_end_72
    .catchall {:try_start_2 .. :try_end_72} :catchall_73

    goto :goto_93

    :catchall_73
    move-exception v1

    .line 167
    sget-object v2, Ljava/lang/System;->err:Ljava/io/PrintStream;

    iget p0, p0, Ladapter/window/OhImeBridge$1;->val$op:I

    invoke-static {v1}, Ljava/lang/String;->valueOf(Ljava/lang/Object;)Ljava/lang/String;

    move-result-object v1

    new-instance v3, Ljava/lang/StringBuilder;

    invoke-direct {v3, v0}, Ljava/lang/StringBuilder;-><init>(Ljava/lang/String;)V

    invoke-virtual {v3, p0}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    const-string p0, ") failed: "

    invoke-virtual {v3, p0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v3, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object p0

    invoke-virtual {v2, p0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    :goto_93
    return-void
.end method
