.class public Lio/material/catalog/transition/ContainerTransformConfigurationHelper;
.super Ljava/lang/Object;
.source "ContainerTransformConfigurationHelper.java"


# annotations
.annotation system Ldalvik/annotation/MemberClasses;
    value = {
        Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomOvershootInterpolator;,
        Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomAnticipateOvershootInterpolator;,
        Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomCubicBezier;
    }
.end annotation


# static fields
.field private static transient synthetic $jacocoData:[Z = null

.field private static final CUBIC_CONTROL_FORMAT:Ljava/lang/String; = "%.3f"

.field private static final DURATION_FORMAT:Ljava/lang/String; = "%.0f"

.field private static final FADE_MODE_MAP:Landroid/util/SparseIntArray;

.field private static final NO_DURATION:J = -0x1L


# instance fields
.field private arcMotionEnabled:Z

.field private drawDebugEnabled:Z

.field private enterDuration:J

.field private fadeModeButtonId:I

.field private interpolator:Landroid/view/animation/Interpolator;

.field private returnDuration:J


# direct methods
.method private static synthetic $jacocoInit()[Z
    .registers 4

    sget-object v0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoData:[Z

    if-nez v0, :cond_13

    const-wide v0, 0x4d4abb6b149b29a2L    # 2.1993898384020314E64

    const/16 v2, 0xd3

    const-string v3, "io/material/catalog/transition/ContainerTransformConfigurationHelper"

    invoke-static {v0, v1, v3, v2}, Lorg/jacoco/agent/rt/internal_3570298/Offline;->getProbes(JLjava/lang/String;I)[Z

    move-result-object v0

    sput-object v0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoData:[Z

    :cond_13
    return-object v0
.end method

.method static constructor <clinit>()V
    .registers 5

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 71
    new-instance v1, Landroid/util/SparseIntArray;

    invoke-direct {v1}, Landroid/util/SparseIntArray;-><init>()V

    sput-object v1, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->FADE_MODE_MAP:Landroid/util/SparseIntArray;

    const/16 v2, 0xce

    const/4 v3, 0x1

    aput-boolean v3, v0, v2

    .line 74
    const v2, 0x7f0901fa

    const/4 v4, 0x0

    invoke-virtual {v1, v2, v4}, Landroid/util/SparseIntArray;->append(II)V

    const/16 v2, 0xcf

    aput-boolean v3, v0, v2

    .line 75
    const v2, 0x7f0901fc

    invoke-virtual {v1, v2, v3}, Landroid/util/SparseIntArray;->append(II)V

    const/16 v2, 0xd0

    aput-boolean v3, v0, v2

    .line 76
    const v2, 0x7f0901f8

    const/4 v4, 0x2

    invoke-virtual {v1, v2, v4}, Landroid/util/SparseIntArray;->append(II)V

    const/16 v2, 0xd1

    aput-boolean v3, v0, v2

    .line 77
    const v2, 0x7f0901fd

    const/4 v4, 0x3

    invoke-virtual {v1, v2, v4}, Landroid/util/SparseIntArray;->append(II)V

    .line 78
    const/16 v1, 0xd2

    aput-boolean v3, v0, v1

    return-void
.end method

.method public constructor <init>()V
    .registers 4

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 80
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    const/4 v1, 0x0

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    .line 81
    invoke-direct {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setUpDefaultValues()V

    .line 82
    aput-boolean v2, v0, v2

    return-void
.end method

.method private static areValidCubicBezierControls(Landroid/view/View;Ljava/lang/Float;Ljava/lang/Float;Ljava/lang/Float;Ljava/lang/Float;)Z
    .registers 9
    .param p0, "view"    # Landroid/view/View;
    .param p1, "x1"    # Ljava/lang/Float;
    .param p2, "y1"    # Ljava/lang/Float;
    .param p3, "x2"    # Ljava/lang/Float;
    .param p4, "y2"    # Ljava/lang/Float;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 391
    const/4 v1, 0x1

    .local v1, "isValid":Z
    const/16 v2, 0x87

    const/4 v3, 0x1

    aput-boolean v3, v0, v2

    .line 392
    invoke-static {p1}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->isValidCubicBezierControlValue(Ljava/lang/Float;)Z

    move-result v2

    if-eqz v2, :cond_15

    const/16 v2, 0x88

    aput-boolean v3, v0, v2

    goto :goto_2a

    .line 393
    :cond_15
    const/4 v1, 0x0

    const/16 v2, 0x89

    aput-boolean v3, v0, v2

    .line 394
    const v2, 0x7f09046e

    invoke-virtual {p0, v2}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v2

    check-cast v2, Lcom/google/android/material/textfield/TextInputLayout;

    invoke-static {v2}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setTextInputLayoutError(Lcom/google/android/material/textfield/TextInputLayout;)V

    const/16 v2, 0x8a

    aput-boolean v3, v0, v2

    .line 396
    :goto_2a
    invoke-static {p2}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->isValidCubicBezierControlValue(Ljava/lang/Float;)Z

    move-result v2

    if-eqz v2, :cond_35

    const/16 v2, 0x8b

    aput-boolean v3, v0, v2

    goto :goto_4a

    .line 397
    :cond_35
    const/4 v1, 0x0

    const/16 v2, 0x8c

    aput-boolean v3, v0, v2

    .line 398
    const v2, 0x7f090474

    invoke-virtual {p0, v2}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v2

    check-cast v2, Lcom/google/android/material/textfield/TextInputLayout;

    invoke-static {v2}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setTextInputLayoutError(Lcom/google/android/material/textfield/TextInputLayout;)V

    const/16 v2, 0x8d

    aput-boolean v3, v0, v2

    .line 400
    :goto_4a
    invoke-static {p3}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->isValidCubicBezierControlValue(Ljava/lang/Float;)Z

    move-result v2

    if-eqz v2, :cond_55

    const/16 v2, 0x8e

    aput-boolean v3, v0, v2

    goto :goto_6a

    .line 401
    :cond_55
    const/4 v1, 0x0

    const/16 v2, 0x8f

    aput-boolean v3, v0, v2

    .line 402
    const v2, 0x7f090470

    invoke-virtual {p0, v2}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v2

    check-cast v2, Lcom/google/android/material/textfield/TextInputLayout;

    invoke-static {v2}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setTextInputLayoutError(Lcom/google/android/material/textfield/TextInputLayout;)V

    const/16 v2, 0x90

    aput-boolean v3, v0, v2

    .line 404
    :goto_6a
    invoke-static {p4}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->isValidCubicBezierControlValue(Ljava/lang/Float;)Z

    move-result v2

    if-eqz v2, :cond_75

    const/16 v2, 0x91

    aput-boolean v3, v0, v2

    goto :goto_8a

    .line 405
    :cond_75
    const/4 v1, 0x0

    const/16 v2, 0x92

    aput-boolean v3, v0, v2

    .line 406
    const v2, 0x7f090476

    invoke-virtual {p0, v2}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v2

    check-cast v2, Lcom/google/android/material/textfield/TextInputLayout;

    invoke-static {v2}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setTextInputLayoutError(Lcom/google/android/material/textfield/TextInputLayout;)V

    const/16 v2, 0x93

    aput-boolean v3, v0, v2

    .line 409
    :goto_8a
    const/16 v2, 0x94

    aput-boolean v3, v0, v2

    return v1
.end method

.method private createConfigurationBottomSheetView(Landroid/content/Context;Lcom/google/android/material/bottomsheet/BottomSheetDialog;)Landroid/view/View;
    .registers 8
    .param p1, "context"    # Landroid/content/Context;
    .param p2, "dialog"    # Lcom/google/android/material/bottomsheet/BottomSheetDialog;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 176
    const/16 v1, 0x28

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    .line 177
    invoke-static {p1}, Landroid/view/LayoutInflater;->from(Landroid/content/Context;)Landroid/view/LayoutInflater;

    move-result-object v1

    const v3, 0x7f0c00c2

    const/4 v4, 0x0

    invoke-virtual {v1, v3, v4}, Landroid/view/LayoutInflater;->inflate(ILandroid/view/ViewGroup;)Landroid/view/View;

    move-result-object v1

    .local v1, "layout":Landroid/view/View;
    const/16 v3, 0x29

    aput-boolean v2, v0, v3

    .line 178
    invoke-direct {p0, v1}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setUpBottomSheetPathMotionButtonGroup(Landroid/view/View;)V

    const/16 v3, 0x2a

    aput-boolean v2, v0, v3

    .line 179
    invoke-direct {p0, v1}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setUpBottomSheetEnterDurationSlider(Landroid/view/View;)V

    const/16 v3, 0x2b

    aput-boolean v2, v0, v3

    .line 180
    invoke-direct {p0, v1}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setUpBottomSheetReturnDurationSlider(Landroid/view/View;)V

    const/16 v3, 0x2c

    aput-boolean v2, v0, v3

    .line 181
    invoke-direct {p0, v1}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setUpBottomSheetInterpolation(Landroid/view/View;)V

    const/16 v3, 0x2d

    aput-boolean v2, v0, v3

    .line 182
    invoke-direct {p0, v1}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setUpBottomSheetFadeModeButtonGroup(Landroid/view/View;)V

    const/16 v3, 0x2e

    aput-boolean v2, v0, v3

    .line 183
    invoke-direct {p0, v1}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setUpBottomSheetDebugging(Landroid/view/View;)V

    const/16 v3, 0x2f

    aput-boolean v2, v0, v3

    .line 184
    invoke-direct {p0, v1, p2}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setUpBottomSheetConfirmationButtons(Landroid/view/View;Lcom/google/android/material/bottomsheet/BottomSheetDialog;)V

    .line 185
    const/16 v3, 0x30

    aput-boolean v2, v0, v3

    return-object v1
.end method

.method private static getTextFloat(Landroid/widget/EditText;)Ljava/lang/Float;
    .registers 7
    .param p0, "editText"    # Landroid/widget/EditText;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 351
    const/4 v1, 0x0

    const/4 v2, 0x1

    if-nez p0, :cond_d

    .line 352
    const/16 v3, 0x79

    aput-boolean v2, v0, v3

    return-object v1

    .line 355
    :cond_d
    invoke-virtual {p0}, Landroid/widget/EditText;->getText()Landroid/text/Editable;

    move-result-object v3

    invoke-virtual {v3}, Ljava/lang/Object;->toString()Ljava/lang/String;

    move-result-object v3

    const/16 v4, 0x7a

    :try_start_17
    aput-boolean v2, v0, v4
    :try_end_19
    .catch Ljava/lang/Exception; {:try_start_17 .. :try_end_19} :catch_24

    .line 357
    .local v3, "text":Ljava/lang/String;
    :try_start_19
    invoke-static {v3}, Ljava/lang/Float;->valueOf(Ljava/lang/String;)Ljava/lang/Float;

    move-result-object v1
    :try_end_1d
    .catch Ljava/lang/Exception; {:try_start_19 .. :try_end_1d} :catch_22

    const/16 v4, 0x7b

    aput-boolean v2, v0, v4

    return-object v1

    .line 358
    :catch_22
    move-exception v4

    goto :goto_25

    .end local v3    # "text":Ljava/lang/String;
    :catch_24
    move-exception v4

    .line 359
    .restart local v3    # "text":Ljava/lang/String;
    .local v4, "e":Ljava/lang/Exception;
    :goto_25
    const/16 v5, 0x7c

    aput-boolean v2, v0, v5

    return-object v1
.end method

.method private static isValidCubicBezierControlValue(Ljava/lang/Float;)Z
    .registers 5
    .param p0, "value"    # Ljava/lang/Float;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 386
    const/4 v1, 0x1

    if-nez p0, :cond_c

    const/16 v2, 0x81

    aput-boolean v1, v0, v2

    goto :goto_28

    :cond_c
    invoke-virtual {p0}, Ljava/lang/Float;->floatValue()F

    move-result v2

    const/4 v3, 0x0

    cmpl-float v2, v2, v3

    if-gez v2, :cond_1a

    const/16 v2, 0x82

    aput-boolean v1, v0, v2

    goto :goto_28

    :cond_1a
    invoke-virtual {p0}, Ljava/lang/Float;->floatValue()F

    move-result v2

    const/high16 v3, 0x3f800000    # 1.0f

    cmpg-float v2, v2, v3

    if-lez v2, :cond_2e

    const/16 v2, 0x83

    aput-boolean v1, v0, v2

    :goto_28
    const/4 v2, 0x0

    const/16 v3, 0x85

    aput-boolean v1, v0, v3

    goto :goto_33

    :cond_2e
    const/16 v2, 0x84

    aput-boolean v1, v0, v2

    const/4 v2, 0x1

    :goto_33
    const/16 v3, 0x86

    aput-boolean v1, v0, v3

    return v2
.end method

.method static synthetic lambda$setUpBottomSheetDurationSlider$4(Lcom/google/android/material/slider/Slider$OnChangeListener;Landroid/widget/TextView;Lcom/google/android/material/slider/Slider;FZ)V
    .registers 10
    .param p0, "listener"    # Lcom/google/android/material/slider/Slider$OnChangeListener;
    .param p1, "durationValue"    # Landroid/widget/TextView;
    .param p2, "slider"    # Lcom/google/android/material/slider/Slider;
    .param p3, "value"    # F
    .param p4, "fromUser"    # Z

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 254
    invoke-interface {p0, p2, p3, p4}, Lcom/google/android/material/slider/Slider$OnChangeListener;->onValueChange(Lcom/google/android/material/slider/Slider;FZ)V

    const/16 v1, 0xc4

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    .line 255
    new-array v1, v2, [Ljava/lang/Object;

    invoke-static {p3}, Ljava/lang/Float;->valueOf(F)Ljava/lang/Float;

    move-result-object v3

    const/4 v4, 0x0

    aput-object v3, v1, v4

    const-string v3, "%.0f"

    invoke-static {v3, v1}, Ljava/lang/String;->format(Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/String;

    move-result-object v1

    invoke-virtual {p1, v1}, Landroid/widget/TextView;->setText(Ljava/lang/CharSequence;)V

    .line 256
    const/16 v1, 0xc5

    aput-boolean v2, v0, v1

    return-void
.end method

.method static synthetic lambda$setUpBottomSheetInterpolation$5(Lcom/google/android/material/textfield/TextInputLayout;Lcom/google/android/material/textfield/TextInputLayout;Landroid/view/ViewGroup;Landroid/widget/RadioGroup;I)V
    .registers 8
    .param p0, "overshootTensionTextInputLayout"    # Lcom/google/android/material/textfield/TextInputLayout;
    .param p1, "anticipateOvershootTensionTextInputLayout"    # Lcom/google/android/material/textfield/TextInputLayout;
    .param p2, "customContainer"    # Landroid/view/ViewGroup;
    .param p3, "group"    # Landroid/widget/RadioGroup;
    .param p4, "checkedId"    # I

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 324
    invoke-static {p4, p0, p1, p2}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->updateCustomTextFieldsVisibility(ILcom/google/android/material/textfield/TextInputLayout;Lcom/google/android/material/textfield/TextInputLayout;Landroid/view/ViewGroup;)V

    const/16 v1, 0xc3

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    return-void
.end method

.method private static setTextFloat(Landroid/widget/EditText;F)V
    .registers 7
    .param p0, "editText"    # Landroid/widget/EditText;
    .param p1, "value"    # F

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 346
    const/4 v1, 0x1

    new-array v2, v1, [Ljava/lang/Object;

    invoke-static {p1}, Ljava/lang/Float;->valueOf(F)Ljava/lang/Float;

    move-result-object v3

    const/4 v4, 0x0

    aput-object v3, v2, v4

    const-string v3, "%.3f"

    invoke-static {v3, v2}, Ljava/lang/String;->format(Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/String;

    move-result-object v2

    invoke-virtual {p0, v2}, Landroid/widget/EditText;->setText(Ljava/lang/CharSequence;)V

    .line 347
    const/16 v2, 0x78

    aput-boolean v1, v0, v2

    return-void
.end method

.method private static setTextInputClearOnTextChanged(Lcom/google/android/material/textfield/TextInputLayout;)V
    .registers 6
    .param p0, "layout"    # Lcom/google/android/material/textfield/TextInputLayout;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 368
    const/16 v1, 0x7e

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    .line 369
    invoke-virtual {p0}, Lcom/google/android/material/textfield/TextInputLayout;->getEditText()Landroid/widget/EditText;

    move-result-object v1

    new-instance v3, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$1;

    invoke-direct {v3, p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$1;-><init>(Lcom/google/android/material/textfield/TextInputLayout;)V

    const/16 v4, 0x7f

    aput-boolean v2, v0, v4

    .line 370
    invoke-virtual {v1, v3}, Landroid/widget/EditText;->addTextChangedListener(Landroid/text/TextWatcher;)V

    .line 383
    const/16 v1, 0x80

    aput-boolean v2, v0, v1

    return-void
.end method

.method private static setTextInputLayoutError(Lcom/google/android/material/textfield/TextInputLayout;)V
    .registers 4
    .param p0, "layout"    # Lcom/google/android/material/textfield/TextInputLayout;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 364
    const-string v1, " "

    invoke-virtual {p0, v1}, Lcom/google/android/material/textfield/TextInputLayout;->setError(Ljava/lang/CharSequence;)V

    .line 365
    const/16 v1, 0x7d

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    return-void
.end method

.method private setUpBottomSheetConfirmationButtons(Landroid/view/View;Lcom/google/android/material/bottomsheet/BottomSheetDialog;)V
    .registers 8
    .param p1, "view"    # Landroid/view/View;
    .param p2, "dialog"    # Lcom/google/android/material/bottomsheet/BottomSheetDialog;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 424
    const v1, 0x7f090078

    invoke-virtual {p1, v1}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v1

    new-instance v2, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda0;

    invoke-direct {v2, p0, p1, p2}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda0;-><init>(Lio/material/catalog/transition/ContainerTransformConfigurationHelper;Landroid/view/View;Lcom/google/android/material/bottomsheet/BottomSheetDialog;)V

    const/16 v3, 0x9a

    const/4 v4, 0x1

    aput-boolean v4, v0, v3

    .line 425
    invoke-virtual {v1, v2}, Landroid/view/View;->setOnClickListener(Landroid/view/View$OnClickListener;)V

    const/16 v1, 0x9b

    aput-boolean v4, v0, v1

    .line 470
    const v1, 0x7f090170

    invoke-virtual {p1, v1}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v1

    new-instance v2, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda1;

    invoke-direct {v2, p0, p2}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda1;-><init>(Lio/material/catalog/transition/ContainerTransformConfigurationHelper;Lcom/google/android/material/bottomsheet/BottomSheetDialog;)V

    const/16 v3, 0x9c

    aput-boolean v4, v0, v3

    .line 471
    invoke-virtual {v1, v2}, Landroid/view/View;->setOnClickListener(Landroid/view/View$OnClickListener;)V

    .line 476
    const/16 v1, 0x9d

    aput-boolean v4, v0, v1

    return-void
.end method

.method private setUpBottomSheetDebugging(Landroid/view/View;)V
    .registers 6
    .param p1, "view"    # Landroid/view/View;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 414
    const v1, 0x7f0901cd

    invoke-virtual {p1, v1}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v1

    check-cast v1, Landroid/widget/CheckBox;

    .line 415
    .local v1, "debugCheckbox":Landroid/widget/CheckBox;
    const/4 v2, 0x1

    if-nez v1, :cond_15

    const/16 v3, 0x95

    aput-boolean v2, v0, v3

    goto :goto_2e

    :cond_15
    const/16 v3, 0x96

    aput-boolean v2, v0, v3

    .line 416
    iget-boolean v3, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->drawDebugEnabled:Z

    invoke-virtual {v1, v3}, Landroid/widget/CheckBox;->setChecked(Z)V

    const/16 v3, 0x97

    aput-boolean v2, v0, v3

    .line 417
    new-instance v3, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda2;

    invoke-direct {v3, p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda2;-><init>(Lio/material/catalog/transition/ContainerTransformConfigurationHelper;)V

    invoke-virtual {v1, v3}, Landroid/widget/CheckBox;->setOnCheckedChangeListener(Landroid/widget/CompoundButton$OnCheckedChangeListener;)V

    const/16 v3, 0x98

    aput-boolean v2, v0, v3

    .line 420
    :goto_2e
    const/16 v3, 0x99

    aput-boolean v2, v0, v3

    return-void
.end method

.method private setUpBottomSheetDurationSlider(Landroid/view/View;IIFLcom/google/android/material/slider/Slider$OnChangeListener;)V
    .registers 13
    .param p1, "view"    # Landroid/view/View;
    .param p2, "sliderResId"    # I
    .param p3, "labelResId"    # I
    .param p4, "duration"    # F
    .param p5, "listener"    # Lcom/google/android/material/slider/Slider$OnChangeListener;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 245
    invoke-virtual {p1, p2}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v1

    check-cast v1, Lcom/google/android/material/slider/Slider;

    .local v1, "durationSlider":Lcom/google/android/material/slider/Slider;
    const/16 v2, 0x3f

    const/4 v3, 0x1

    aput-boolean v3, v0, v2

    .line 246
    invoke-virtual {p1, p3}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v2

    check-cast v2, Landroid/widget/TextView;

    .line 247
    .local v2, "durationValue":Landroid/widget/TextView;
    if-nez v1, :cond_1c

    const/16 v4, 0x40

    aput-boolean v3, v0, v4

    goto :goto_65

    :cond_1c
    if-nez v2, :cond_23

    const/16 v4, 0x41

    aput-boolean v3, v0, v4

    goto :goto_65

    :cond_23
    const/16 v4, 0x42

    aput-boolean v3, v0, v4

    .line 249
    const/high16 v4, -0x40800000    # -1.0f

    cmpl-float v4, p4, v4

    if-eqz v4, :cond_33

    const/16 v4, 0x43

    aput-boolean v3, v0, v4

    move v4, p4

    goto :goto_38

    :cond_33
    const/4 v4, 0x0

    const/16 v5, 0x44

    aput-boolean v3, v0, v5

    :goto_38
    invoke-virtual {v1, v4}, Lcom/google/android/material/slider/Slider;->setValue(F)V

    const/16 v4, 0x45

    aput-boolean v3, v0, v4

    .line 250
    new-array v4, v3, [Ljava/lang/Object;

    const/4 v5, 0x0

    invoke-virtual {v1}, Lcom/google/android/material/slider/Slider;->getValue()F

    move-result v6

    invoke-static {v6}, Ljava/lang/Float;->valueOf(F)Ljava/lang/Float;

    move-result-object v6

    aput-object v6, v4, v5

    const-string v5, "%.0f"

    invoke-static {v5, v4}, Ljava/lang/String;->format(Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/String;

    move-result-object v4

    invoke-virtual {v2, v4}, Landroid/widget/TextView;->setText(Ljava/lang/CharSequence;)V

    const/16 v4, 0x46

    aput-boolean v3, v0, v4

    .line 252
    new-instance v4, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda6;

    invoke-direct {v4, p5, v2}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda6;-><init>(Lcom/google/android/material/slider/Slider$OnChangeListener;Landroid/widget/TextView;)V

    invoke-virtual {v1, v4}, Lcom/google/android/material/slider/Slider;->addOnChangeListener(Lcom/google/android/material/slider/BaseOnChangeListener;)V

    const/16 v4, 0x47

    aput-boolean v3, v0, v4

    .line 258
    :goto_65
    const/16 v4, 0x48

    aput-boolean v3, v0, v4

    return-void
.end method

.method private setUpBottomSheetEnterDurationSlider(Landroid/view/View;)V
    .registers 11
    .param p1, "view"    # Landroid/view/View;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 220
    iget-wide v1, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->enterDuration:J

    long-to-float v7, v1

    new-instance v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda7;

    invoke-direct {v8, p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda7;-><init>(Lio/material/catalog/transition/ContainerTransformConfigurationHelper;)V

    const v5, 0x7f0901ef

    const v6, 0x7f0901f0

    move-object v3, p0

    move-object v4, p1

    invoke-direct/range {v3 .. v8}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setUpBottomSheetDurationSlider(Landroid/view/View;IIFLcom/google/android/material/slider/Slider$OnChangeListener;)V

    .line 226
    const/16 v1, 0x3d

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    return-void
.end method

.method private setUpBottomSheetFadeModeButtonGroup(Landroid/view/View;)V
    .registers 6
    .param p1, "view"    # Landroid/view/View;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 205
    const v1, 0x7f0901fb

    invoke-virtual {p1, v1}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v1

    check-cast v1, Lcom/google/android/material/button/MaterialButtonToggleGroup;

    .line 206
    .local v1, "toggleGroup":Lcom/google/android/material/button/MaterialButtonToggleGroup;
    const/4 v2, 0x1

    if-nez v1, :cond_15

    const/16 v3, 0x38

    aput-boolean v2, v0, v3

    goto :goto_2e

    :cond_15
    const/16 v3, 0x39

    aput-boolean v2, v0, v3

    .line 208
    iget v3, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->fadeModeButtonId:I

    invoke-virtual {v1, v3}, Lcom/google/android/material/button/MaterialButtonToggleGroup;->check(I)V

    const/16 v3, 0x3a

    aput-boolean v2, v0, v3

    .line 209
    new-instance v3, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda4;

    invoke-direct {v3, p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda4;-><init>(Lio/material/catalog/transition/ContainerTransformConfigurationHelper;)V

    invoke-virtual {v1, v3}, Lcom/google/android/material/button/MaterialButtonToggleGroup;->addOnButtonCheckedListener(Lcom/google/android/material/button/MaterialButtonToggleGroup$OnButtonCheckedListener;)V

    const/16 v3, 0x3b

    aput-boolean v2, v0, v3

    .line 216
    :goto_2e
    const/16 v3, 0x3c

    aput-boolean v2, v0, v3

    return-void
.end method

.method private setUpBottomSheetInterpolation(Landroid/view/View;)V
    .registers 13
    .param p1, "view"    # Landroid/view/View;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 262
    const v1, 0x7f09025b

    invoke-virtual {p1, v1}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v1

    check-cast v1, Landroid/widget/RadioGroup;

    .local v1, "interpolationGroup":Landroid/widget/RadioGroup;
    const/16 v2, 0x49

    const/4 v3, 0x1

    aput-boolean v3, v0, v2

    .line 263
    const v2, 0x7f09019f

    invoke-virtual {p1, v2}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v2

    check-cast v2, Landroid/view/ViewGroup;

    .line 264
    .local v2, "customContainer":Landroid/view/ViewGroup;
    const/16 v4, 0x4a

    aput-boolean v3, v0, v4

    .line 265
    const v4, 0x7f090315

    invoke-virtual {p1, v4}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v4

    check-cast v4, Lcom/google/android/material/textfield/TextInputLayout;

    .local v4, "overshootTensionTextInputLayout":Lcom/google/android/material/textfield/TextInputLayout;
    const/16 v5, 0x4b

    aput-boolean v3, v0, v5

    .line 266
    const v5, 0x7f090314

    invoke-virtual {p1, v5}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v5

    check-cast v5, Landroid/widget/EditText;

    .line 267
    .local v5, "overshootTensionEditText":Landroid/widget/EditText;
    const/16 v6, 0x4c

    aput-boolean v3, v0, v6

    .line 268
    const v6, 0x7f090074

    invoke-virtual {p1, v6}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v6

    check-cast v6, Lcom/google/android/material/textfield/TextInputLayout;

    .line 269
    .local v6, "anticipateOvershootTensionTextInputLayout":Lcom/google/android/material/textfield/TextInputLayout;
    const/16 v7, 0x4d

    aput-boolean v3, v0, v7

    .line 270
    const v7, 0x7f090073

    invoke-virtual {p1, v7}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v7

    check-cast v7, Landroid/widget/EditText;

    .line 272
    .local v7, "anticipateOvershootTensionEditText":Landroid/widget/EditText;
    if-nez v1, :cond_57

    const/16 v8, 0x4e

    aput-boolean v3, v0, v8

    goto/16 :goto_1ce

    :cond_57
    if-nez v2, :cond_5f

    const/16 v8, 0x4f

    aput-boolean v3, v0, v8

    goto/16 :goto_1ce

    :cond_5f
    const/16 v8, 0x50

    aput-boolean v3, v0, v8

    .line 273
    const v8, 0x7f09046e

    invoke-virtual {p1, v8}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v8

    check-cast v8, Lcom/google/android/material/textfield/TextInputLayout;

    invoke-static {v8}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setTextInputClearOnTextChanged(Lcom/google/android/material/textfield/TextInputLayout;)V

    const/16 v8, 0x51

    aput-boolean v3, v0, v8

    .line 274
    const v8, 0x7f090470

    invoke-virtual {p1, v8}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v8

    check-cast v8, Lcom/google/android/material/textfield/TextInputLayout;

    invoke-static {v8}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setTextInputClearOnTextChanged(Lcom/google/android/material/textfield/TextInputLayout;)V

    const/16 v8, 0x52

    aput-boolean v3, v0, v8

    .line 275
    const v8, 0x7f090474

    invoke-virtual {p1, v8}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v8

    check-cast v8, Lcom/google/android/material/textfield/TextInputLayout;

    invoke-static {v8}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setTextInputClearOnTextChanged(Lcom/google/android/material/textfield/TextInputLayout;)V

    const/16 v8, 0x53

    aput-boolean v3, v0, v8

    .line 276
    const v8, 0x7f090476

    invoke-virtual {p1, v8}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v8

    check-cast v8, Lcom/google/android/material/textfield/TextInputLayout;

    invoke-static {v8}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setTextInputClearOnTextChanged(Lcom/google/android/material/textfield/TextInputLayout;)V

    const/16 v8, 0x54

    aput-boolean v3, v0, v8

    .line 278
    const/high16 v8, 0x40000000    # 2.0f

    invoke-static {v8}, Ljava/lang/String;->valueOf(F)Ljava/lang/String;

    move-result-object v9

    invoke-virtual {v5, v9}, Landroid/widget/EditText;->setText(Ljava/lang/CharSequence;)V

    .line 279
    const/16 v9, 0x55

    aput-boolean v3, v0, v9

    .line 280
    invoke-static {v8}, Ljava/lang/String;->valueOf(F)Ljava/lang/String;

    move-result-object v8

    const/16 v9, 0x56

    aput-boolean v3, v0, v9

    .line 279
    invoke-virtual {v7, v8}, Landroid/widget/EditText;->setText(Ljava/lang/CharSequence;)V

    .line 283
    iget-object v8, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->interpolator:Landroid/view/animation/Interpolator;

    instance-of v9, v8, Landroidx/interpolator/view/animation/FastOutSlowInInterpolator;

    if-eqz v9, :cond_d1

    const/16 v8, 0x57

    aput-boolean v3, v0, v8

    .line 284
    const v8, 0x7f090342

    invoke-virtual {v1, v8}, Landroid/widget/RadioGroup;->check(I)V

    const/16 v8, 0x58

    aput-boolean v3, v0, v8

    goto/16 :goto_1af

    .line 285
    :cond_d1
    instance-of v9, v8, Landroid/view/animation/OvershootInterpolator;

    if-eqz v9, :cond_100

    const/16 v8, 0x59

    aput-boolean v3, v0, v8

    .line 286
    const v8, 0x7f090343

    invoke-virtual {v1, v8}, Landroid/widget/RadioGroup;->check(I)V

    .line 287
    iget-object v8, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->interpolator:Landroid/view/animation/Interpolator;

    instance-of v9, v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomOvershootInterpolator;

    if-nez v9, :cond_eb

    const/16 v8, 0x5a

    aput-boolean v3, v0, v8

    goto/16 :goto_1af

    .line 288
    :cond_eb
    check-cast v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomOvershootInterpolator;

    .local v8, "customOvershootInterpolator":Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomOvershootInterpolator;
    const/16 v9, 0x5b

    aput-boolean v3, v0, v9

    .line 290
    iget v9, v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomOvershootInterpolator;->tension:F

    invoke-static {v9}, Ljava/lang/String;->valueOf(F)Ljava/lang/String;

    move-result-object v9

    invoke-virtual {v5, v9}, Landroid/widget/EditText;->setText(Ljava/lang/CharSequence;)V

    .line 291
    .end local v8    # "customOvershootInterpolator":Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomOvershootInterpolator;
    const/16 v8, 0x5c

    aput-boolean v3, v0, v8

    goto/16 :goto_1af

    .line 292
    :cond_100
    instance-of v9, v8, Landroid/view/animation/AnticipateOvershootInterpolator;

    if-eqz v9, :cond_133

    const/16 v8, 0x5d

    aput-boolean v3, v0, v8

    .line 293
    const v8, 0x7f09033a

    invoke-virtual {v1, v8}, Landroid/widget/RadioGroup;->check(I)V

    .line 294
    iget-object v8, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->interpolator:Landroid/view/animation/Interpolator;

    instance-of v9, v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomAnticipateOvershootInterpolator;

    if-nez v9, :cond_11a

    const/16 v8, 0x5e

    aput-boolean v3, v0, v8

    goto/16 :goto_1af

    .line 295
    :cond_11a
    check-cast v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomAnticipateOvershootInterpolator;

    .line 297
    .local v8, "customAnticipateOvershootInterpolator":Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomAnticipateOvershootInterpolator;
    iget v9, v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomAnticipateOvershootInterpolator;->tension:F

    const/16 v10, 0x5f

    aput-boolean v3, v0, v10

    .line 298
    invoke-static {v9}, Ljava/lang/String;->valueOf(F)Ljava/lang/String;

    move-result-object v9

    const/16 v10, 0x60

    aput-boolean v3, v0, v10

    .line 297
    invoke-virtual {v7, v9}, Landroid/widget/EditText;->setText(Ljava/lang/CharSequence;)V

    .line 299
    .end local v8    # "customAnticipateOvershootInterpolator":Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomAnticipateOvershootInterpolator;
    const/16 v8, 0x61

    aput-boolean v3, v0, v8

    goto/16 :goto_1af

    .line 300
    :cond_133
    instance-of v9, v8, Landroid/view/animation/BounceInterpolator;

    if-eqz v9, :cond_146

    const/16 v8, 0x62

    aput-boolean v3, v0, v8

    .line 301
    const v8, 0x7f09033b

    invoke-virtual {v1, v8}, Landroid/widget/RadioGroup;->check(I)V

    const/16 v8, 0x63

    aput-boolean v3, v0, v8

    goto :goto_1af

    .line 302
    :cond_146
    instance-of v8, v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomCubicBezier;

    if-eqz v8, :cond_1a5

    const/16 v8, 0x64

    aput-boolean v3, v0, v8

    .line 303
    const v8, 0x7f090340

    invoke-virtual {v1, v8}, Landroid/widget/RadioGroup;->check(I)V

    .line 304
    iget-object v8, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->interpolator:Landroid/view/animation/Interpolator;

    check-cast v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomCubicBezier;

    .local v8, "currentInterp":Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomCubicBezier;
    const/16 v9, 0x65

    aput-boolean v3, v0, v9

    .line 305
    const v9, 0x7f09046d

    invoke-virtual {p1, v9}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v9

    check-cast v9, Landroid/widget/EditText;

    iget v10, v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomCubicBezier;->controlX1:F

    invoke-static {v9, v10}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setTextFloat(Landroid/widget/EditText;F)V

    const/16 v9, 0x66

    aput-boolean v3, v0, v9

    .line 306
    const v9, 0x7f090473

    invoke-virtual {p1, v9}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v9

    check-cast v9, Landroid/widget/EditText;

    iget v10, v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomCubicBezier;->controlY1:F

    invoke-static {v9, v10}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setTextFloat(Landroid/widget/EditText;F)V

    const/16 v9, 0x67

    aput-boolean v3, v0, v9

    .line 307
    const v9, 0x7f09046f

    invoke-virtual {p1, v9}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v9

    check-cast v9, Landroid/widget/EditText;

    iget v10, v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomCubicBezier;->controlX2:F

    invoke-static {v9, v10}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setTextFloat(Landroid/widget/EditText;F)V

    const/16 v9, 0x68

    aput-boolean v3, v0, v9

    .line 308
    const v9, 0x7f090475

    invoke-virtual {p1, v9}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v9

    check-cast v9, Landroid/widget/EditText;

    iget v10, v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomCubicBezier;->controlY2:F

    invoke-static {v9, v10}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setTextFloat(Landroid/widget/EditText;F)V

    .line 309
    .end local v8    # "currentInterp":Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomCubicBezier;
    const/16 v8, 0x69

    aput-boolean v3, v0, v8

    goto :goto_1af

    .line 310
    :cond_1a5
    const v8, 0x7f090341

    invoke-virtual {v1, v8}, Landroid/widget/RadioGroup;->check(I)V

    const/16 v8, 0x6a

    aput-boolean v3, v0, v8

    .line 314
    :goto_1af
    const/16 v8, 0x6b

    aput-boolean v3, v0, v8

    .line 315
    invoke-virtual {v1}, Landroid/widget/RadioGroup;->getCheckedRadioButtonId()I

    move-result v8

    const/16 v9, 0x6c

    aput-boolean v3, v0, v9

    .line 314
    invoke-static {v8, v4, v6, v2}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->updateCustomTextFieldsVisibility(ILcom/google/android/material/textfield/TextInputLayout;Lcom/google/android/material/textfield/TextInputLayout;Landroid/view/ViewGroup;)V

    const/16 v8, 0x6d

    aput-boolean v3, v0, v8

    .line 322
    new-instance v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda3;

    invoke-direct {v8, v4, v6, v2}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda3;-><init>(Lcom/google/android/material/textfield/TextInputLayout;Lcom/google/android/material/textfield/TextInputLayout;Landroid/view/ViewGroup;)V

    invoke-virtual {v1, v8}, Landroid/widget/RadioGroup;->setOnCheckedChangeListener(Landroid/widget/RadioGroup$OnCheckedChangeListener;)V

    const/16 v8, 0x6e

    aput-boolean v3, v0, v8

    .line 330
    :goto_1ce
    const/16 v8, 0x6f

    aput-boolean v3, v0, v8

    return-void
.end method

.method private setUpBottomSheetPathMotionButtonGroup(Landroid/view/View;)V
    .registers 7
    .param p1, "view"    # Landroid/view/View;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 190
    const v1, 0x7f090326

    invoke-virtual {p1, v1}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v1

    check-cast v1, Lcom/google/android/material/button/MaterialButtonToggleGroup;

    .line 191
    .local v1, "toggleGroup":Lcom/google/android/material/button/MaterialButtonToggleGroup;
    const/4 v2, 0x1

    if-nez v1, :cond_15

    const/16 v3, 0x31

    aput-boolean v2, v0, v3

    goto :goto_3f

    :cond_15
    const/16 v3, 0x32

    aput-boolean v2, v0, v3

    .line 193
    iget-boolean v3, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->arcMotionEnabled:Z

    if-eqz v3, :cond_25

    const v3, 0x7f09007a

    const/16 v4, 0x33

    aput-boolean v2, v0, v4

    goto :goto_2c

    :cond_25
    const v3, 0x7f09027a

    const/16 v4, 0x34

    aput-boolean v2, v0, v4

    :goto_2c
    invoke-virtual {v1, v3}, Lcom/google/android/material/button/MaterialButtonToggleGroup;->check(I)V

    const/16 v3, 0x35

    aput-boolean v2, v0, v3

    .line 194
    new-instance v3, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda5;

    invoke-direct {v3, p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda5;-><init>(Lio/material/catalog/transition/ContainerTransformConfigurationHelper;)V

    invoke-virtual {v1, v3}, Lcom/google/android/material/button/MaterialButtonToggleGroup;->addOnButtonCheckedListener(Lcom/google/android/material/button/MaterialButtonToggleGroup$OnButtonCheckedListener;)V

    const/16 v3, 0x36

    aput-boolean v2, v0, v3

    .line 201
    :goto_3f
    const/16 v3, 0x37

    aput-boolean v2, v0, v3

    return-void
.end method

.method private setUpBottomSheetReturnDurationSlider(Landroid/view/View;)V
    .registers 11
    .param p1, "view"    # Landroid/view/View;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 230
    iget-wide v1, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->returnDuration:J

    long-to-float v7, v1

    new-instance v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda8;

    invoke-direct {v8, p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$$ExternalSyntheticLambda8;-><init>(Lio/material/catalog/transition/ContainerTransformConfigurationHelper;)V

    const v5, 0x7f090350

    const v6, 0x7f090351

    move-object v3, p0

    move-object v4, p1

    invoke-direct/range {v3 .. v8}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setUpBottomSheetDurationSlider(Landroid/view/View;IIFLcom/google/android/material/slider/Slider$OnChangeListener;)V

    .line 236
    const/16 v1, 0x3e

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    return-void
.end method

.method private setUpDefaultValues()V
    .registers 5

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 166
    const/4 v1, 0x0

    iput-boolean v1, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->arcMotionEnabled:Z

    .line 167
    const-wide/16 v2, -0x1

    iput-wide v2, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->enterDuration:J

    .line 168
    iput-wide v2, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->returnDuration:J

    .line 169
    const/4 v2, 0x0

    iput-object v2, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->interpolator:Landroid/view/animation/Interpolator;

    .line 170
    const v2, 0x7f0901fa

    iput v2, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->fadeModeButtonId:I

    .line 171
    iput-boolean v1, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->drawDebugEnabled:Z

    .line 172
    const/16 v1, 0x27

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    return-void
.end method

.method private static updateCustomTextFieldsVisibility(ILcom/google/android/material/textfield/TextInputLayout;Lcom/google/android/material/textfield/TextInputLayout;Landroid/view/ViewGroup;)V
    .registers 9
    .param p0, "checkedId"    # I
    .param p1, "overshootTensionTextInputLayout"    # Lcom/google/android/material/textfield/TextInputLayout;
    .param p2, "anticipateOvershootTensionTextInputLayout"    # Lcom/google/android/material/textfield/TextInputLayout;
    .param p3, "customContainer"    # Landroid/view/ViewGroup;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 337
    nop

    .line 338
    const/4 v1, 0x0

    const/16 v2, 0x8

    const/4 v3, 0x1

    const v4, 0x7f090343

    if-ne p0, v4, :cond_14

    const/16 v4, 0x70

    aput-boolean v3, v0, v4

    const/4 v4, 0x0

    goto :goto_1a

    :cond_14
    const/16 v4, 0x71

    aput-boolean v3, v0, v4

    const/16 v4, 0x8

    .line 337
    :goto_1a
    invoke-virtual {p1, v4}, Lcom/google/android/material/textfield/TextInputLayout;->setVisibility(I)V

    .line 339
    nop

    .line 340
    const v4, 0x7f09033a

    if-ne p0, v4, :cond_29

    const/16 v4, 0x72

    aput-boolean v3, v0, v4

    const/4 v4, 0x0

    goto :goto_2f

    :cond_29
    const/16 v4, 0x73

    aput-boolean v3, v0, v4

    const/16 v4, 0x8

    .line 339
    :goto_2f
    invoke-virtual {p2, v4}, Lcom/google/android/material/textfield/TextInputLayout;->setVisibility(I)V

    const/16 v4, 0x74

    aput-boolean v3, v0, v4

    .line 341
    const v4, 0x7f090340

    if-ne p0, v4, :cond_40

    const/16 v2, 0x75

    aput-boolean v3, v0, v2

    goto :goto_46

    :cond_40
    const/16 v1, 0x76

    aput-boolean v3, v0, v1

    const/16 v1, 0x8

    :goto_46
    invoke-virtual {p3, v1}, Landroid/view/ViewGroup;->setVisibility(I)V

    .line 342
    const/16 v1, 0x77

    aput-boolean v3, v0, v1

    return-void
.end method


# virtual methods
.method configure(Lcom/google/android/material/transition/MaterialContainerTransform;Z)V
    .registers 10
    .param p1, "transform"    # Lcom/google/android/material/transition/MaterialContainerTransform;
    .param p2, "entering"    # Z

    const/high16 v0, 0x3f800000    # 1.0f

    invoke-static {v0}, Landroid/animation/ValueAnimator;->setDurationScale(F)V

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 98
    const/4 v1, 0x1

    if-eqz p2, :cond_14

    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getEnterDuration()J

    move-result-wide v2

    const/4 v4, 0x7

    aput-boolean v1, v0, v4

    goto :goto_1c

    :cond_14
    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getReturnDuration()J

    move-result-wide v2

    const/16 v4, 0x8

    aput-boolean v1, v0, v4

    .line 99
    .local v2, "duration":J
    :goto_1c
    const-wide/16 v4, -0x1

    cmp-long v6, v2, v4

    if-nez v6, :cond_27

    const/16 v4, 0x9

    aput-boolean v1, v0, v4

    goto :goto_32

    :cond_27
    const/16 v4, 0xa

    aput-boolean v1, v0, v4

    .line 100
    invoke-virtual {p1, v2, v3}, Lcom/google/android/material/transition/MaterialContainerTransform;->setDuration(J)Landroidx/transition/Transition;

    const/16 v4, 0xb

    aput-boolean v1, v0, v4

    .line 102
    :goto_32
    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getInterpolator()Landroid/view/animation/Interpolator;

    move-result-object v4

    if-nez v4, :cond_3d

    const/16 v4, 0xc

    aput-boolean v1, v0, v4

    goto :goto_4c

    :cond_3d
    const/16 v4, 0xd

    aput-boolean v1, v0, v4

    .line 103
    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getInterpolator()Landroid/view/animation/Interpolator;

    move-result-object v4

    invoke-virtual {p1, v4}, Lcom/google/android/material/transition/MaterialContainerTransform;->setInterpolator(Landroid/animation/TimeInterpolator;)Landroidx/transition/Transition;

    const/16 v4, 0xe

    aput-boolean v1, v0, v4

    .line 105
    :goto_4c
    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->isArcMotionEnabled()Z

    move-result v4

    if-nez v4, :cond_57

    const/16 v4, 0xf

    aput-boolean v1, v0, v4

    goto :goto_67

    :cond_57
    const/16 v4, 0x10

    aput-boolean v1, v0, v4

    .line 106
    new-instance v4, Lcom/google/android/material/transition/MaterialArcMotion;

    invoke-direct {v4}, Lcom/google/android/material/transition/MaterialArcMotion;-><init>()V

    invoke-virtual {p1, v4}, Lcom/google/android/material/transition/MaterialContainerTransform;->setPathMotion(Landroidx/transition/PathMotion;)V

    const/16 v4, 0x11

    aput-boolean v1, v0, v4

    .line 108
    :goto_67
    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getFadeMode()I

    move-result v4

    invoke-virtual {p1, v4}, Lcom/google/android/material/transition/MaterialContainerTransform;->setFadeMode(I)V

    const/16 v4, 0x12

    aput-boolean v1, v0, v4

    .line 109
    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->isDrawDebugEnabled()Z

    move-result v4

    invoke-virtual {p1, v4}, Lcom/google/android/material/transition/MaterialContainerTransform;->setDrawDebugEnabled(Z)V

    .line 110
    const/16 v4, 0x13

    aput-boolean v1, v0, v4

    return-void
.end method

.method configure(Lcom/google/android/material/transition/platform/MaterialContainerTransform;Z)V
    .registers 10
    .param p1, "transform"    # Lcom/google/android/material/transition/platform/MaterialContainerTransform;
    .param p2, "entering"    # Z

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 117
    const/4 v1, 0x1

    if-eqz p2, :cond_10

    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getEnterDuration()J

    move-result-wide v2

    const/16 v4, 0x14

    aput-boolean v1, v0, v4

    goto :goto_18

    :cond_10
    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getReturnDuration()J

    move-result-wide v2

    const/16 v4, 0x15

    aput-boolean v1, v0, v4

    .line 118
    .local v2, "duration":J
    :goto_18
    const-wide/16 v4, -0x1

    cmp-long v6, v2, v4

    if-nez v6, :cond_23

    const/16 v4, 0x16

    aput-boolean v1, v0, v4

    goto :goto_2e

    :cond_23
    const/16 v4, 0x17

    aput-boolean v1, v0, v4

    .line 119
    invoke-virtual {p1, v2, v3}, Lcom/google/android/material/transition/platform/MaterialContainerTransform;->setDuration(J)Landroid/transition/Transition;

    const/16 v4, 0x18

    aput-boolean v1, v0, v4

    .line 121
    :goto_2e
    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getInterpolator()Landroid/view/animation/Interpolator;

    move-result-object v4

    if-nez v4, :cond_39

    const/16 v4, 0x19

    aput-boolean v1, v0, v4

    goto :goto_48

    :cond_39
    const/16 v4, 0x1a

    aput-boolean v1, v0, v4

    .line 122
    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getInterpolator()Landroid/view/animation/Interpolator;

    move-result-object v4

    invoke-virtual {p1, v4}, Lcom/google/android/material/transition/platform/MaterialContainerTransform;->setInterpolator(Landroid/animation/TimeInterpolator;)Landroid/transition/Transition;

    const/16 v4, 0x1b

    aput-boolean v1, v0, v4

    .line 124
    :goto_48
    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->isArcMotionEnabled()Z

    move-result v4

    if-nez v4, :cond_53

    const/16 v4, 0x1c

    aput-boolean v1, v0, v4

    goto :goto_63

    :cond_53
    const/16 v4, 0x1d

    aput-boolean v1, v0, v4

    .line 125
    new-instance v4, Lcom/google/android/material/transition/platform/MaterialArcMotion;

    invoke-direct {v4}, Lcom/google/android/material/transition/platform/MaterialArcMotion;-><init>()V

    invoke-virtual {p1, v4}, Lcom/google/android/material/transition/platform/MaterialContainerTransform;->setPathMotion(Landroid/transition/PathMotion;)V

    const/16 v4, 0x1e

    aput-boolean v1, v0, v4

    .line 128
    :goto_63
    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getFadeMode()I

    move-result v4

    invoke-virtual {p1, v4}, Lcom/google/android/material/transition/platform/MaterialContainerTransform;->setFadeMode(I)V

    const/16 v4, 0x1f

    aput-boolean v1, v0, v4

    .line 129
    invoke-virtual {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->isDrawDebugEnabled()Z

    move-result v4

    invoke-virtual {p1, v4}, Lcom/google/android/material/transition/platform/MaterialContainerTransform;->setDrawDebugEnabled(Z)V

    .line 130
    const/16 v4, 0x20

    aput-boolean v1, v0, v4

    return-void
.end method

.method getEnterDuration()J
    .registers 6

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 142
    iget-wide v1, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->enterDuration:J

    const/16 v3, 0x22

    const/4 v4, 0x1

    aput-boolean v4, v0, v3

    return-wide v1
.end method

.method getFadeMode()I
    .registers 5

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 157
    sget-object v1, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->FADE_MODE_MAP:Landroid/util/SparseIntArray;

    iget v2, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->fadeModeButtonId:I

    invoke-virtual {v1, v2}, Landroid/util/SparseIntArray;->get(I)I

    move-result v1

    const/16 v2, 0x25

    const/4 v3, 0x1

    aput-boolean v3, v0, v2

    return v1
.end method

.method getInterpolator()Landroid/view/animation/Interpolator;
    .registers 5

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 152
    iget-object v1, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->interpolator:Landroid/view/animation/Interpolator;

    const/16 v2, 0x24

    const/4 v3, 0x1

    aput-boolean v3, v0, v2

    return-object v1
.end method

.method getReturnDuration()J
    .registers 6

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 147
    iget-wide v1, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->returnDuration:J

    const/16 v3, 0x23

    const/4 v4, 0x1

    aput-boolean v4, v0, v3

    return-wide v1
.end method

.method isArcMotionEnabled()Z
    .registers 5

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 137
    iget-boolean v1, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->arcMotionEnabled:Z

    const/16 v2, 0x21

    const/4 v3, 0x1

    aput-boolean v3, v0, v2

    return v1
.end method

.method isDrawDebugEnabled()Z
    .registers 5

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 162
    iget-boolean v1, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->drawDebugEnabled:Z

    const/16 v2, 0x26

    const/4 v3, 0x1

    aput-boolean v3, v0, v2

    return v1
.end method

.method synthetic lambda$setUpBottomSheetConfirmationButtons$7$io-material-catalog-transition-ContainerTransformConfigurationHelper(Landroid/view/View;Lcom/google/android/material/bottomsheet/BottomSheetDialog;Landroid/view/View;)V
    .registers 19
    .param p1, "view"    # Landroid/view/View;
    .param p2, "dialog"    # Lcom/google/android/material/bottomsheet/BottomSheetDialog;
    .param p3, "v"    # Landroid/view/View;

    move-object v0, p0

    move-object/from16 v1, p1

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v2

    .line 428
    const v3, 0x7f09025b

    invoke-virtual {v1, v3}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v3

    check-cast v3, Landroid/widget/RadioGroup;

    .local v3, "interpolationGroup":Landroid/widget/RadioGroup;
    const/16 v4, 0xa0

    const/4 v5, 0x1

    aput-boolean v5, v2, v4

    .line 429
    invoke-virtual {v3}, Landroid/widget/RadioGroup;->getCheckedRadioButtonId()I

    move-result v4

    .line 430
    .local v4, "checkedRadioButtonId":I
    const v6, 0x7f090340

    if-ne v4, v6, :cond_9d

    const/16 v6, 0xa1

    aput-boolean v5, v2, v6

    .line 431
    const v6, 0x7f09046d

    invoke-virtual {v1, v6}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v6

    check-cast v6, Landroid/widget/EditText;

    invoke-static {v6}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getTextFloat(Landroid/widget/EditText;)Ljava/lang/Float;

    move-result-object v6

    .local v6, "x1":Ljava/lang/Float;
    const/16 v7, 0xa2

    aput-boolean v5, v2, v7

    .line 432
    const v7, 0x7f090473

    invoke-virtual {v1, v7}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v7

    check-cast v7, Landroid/widget/EditText;

    invoke-static {v7}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getTextFloat(Landroid/widget/EditText;)Ljava/lang/Float;

    move-result-object v7

    .local v7, "y1":Ljava/lang/Float;
    const/16 v8, 0xa3

    aput-boolean v5, v2, v8

    .line 433
    const v8, 0x7f09046f

    invoke-virtual {v1, v8}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v8

    check-cast v8, Landroid/widget/EditText;

    invoke-static {v8}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getTextFloat(Landroid/widget/EditText;)Ljava/lang/Float;

    move-result-object v8

    .local v8, "x2":Ljava/lang/Float;
    const/16 v9, 0xa4

    aput-boolean v5, v2, v9

    .line 434
    const v9, 0x7f090475

    invoke-virtual {v1, v9}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v9

    check-cast v9, Landroid/widget/EditText;

    invoke-static {v9}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getTextFloat(Landroid/widget/EditText;)Ljava/lang/Float;

    move-result-object v9

    .local v9, "y2":Ljava/lang/Float;
    const/16 v10, 0xa5

    aput-boolean v5, v2, v10

    .line 436
    invoke-static {v1, v6, v7, v8, v9}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->areValidCubicBezierControls(Landroid/view/View;Ljava/lang/Float;Ljava/lang/Float;Ljava/lang/Float;Ljava/lang/Float;)Z

    move-result v10

    if-nez v10, :cond_71

    const/16 v10, 0xa6

    aput-boolean v5, v2, v10

    goto :goto_97

    :cond_71
    const/16 v10, 0xa7

    aput-boolean v5, v2, v10

    .line 437
    new-instance v10, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomCubicBezier;

    invoke-virtual {v6}, Ljava/lang/Float;->floatValue()F

    move-result v11

    invoke-virtual {v7}, Ljava/lang/Float;->floatValue()F

    move-result v12

    invoke-virtual {v8}, Ljava/lang/Float;->floatValue()F

    move-result v13

    invoke-virtual {v9}, Ljava/lang/Float;->floatValue()F

    move-result v14

    invoke-direct {v10, v11, v12, v13, v14}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomCubicBezier;-><init>(FFFF)V

    iput-object v10, v0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->interpolator:Landroid/view/animation/Interpolator;

    const/16 v10, 0xa8

    aput-boolean v5, v2, v10

    .line 438
    invoke-virtual/range {p2 .. p2}, Lcom/google/android/material/bottomsheet/BottomSheetDialog;->dismiss()V

    const/16 v10, 0xa9

    aput-boolean v5, v2, v10

    .line 440
    .end local v6    # "x1":Ljava/lang/Float;
    .end local v7    # "y1":Ljava/lang/Float;
    .end local v8    # "x2":Ljava/lang/Float;
    .end local v9    # "y2":Ljava/lang/Float;
    :goto_97
    const/16 v6, 0xaa

    aput-boolean v5, v2, v6

    goto/16 :goto_170

    :cond_9d
    const v6, 0x7f090343

    if-ne v4, v6, :cond_e4

    .line 441
    const v6, 0x7f090314

    const/16 v7, 0xab

    aput-boolean v5, v2, v7

    .line 442
    invoke-virtual {v1, v6}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v6

    check-cast v6, Landroid/widget/EditText;

    .local v6, "overshootTensionEditText":Landroid/widget/EditText;
    const/16 v7, 0xac

    aput-boolean v5, v2, v7

    .line 443
    invoke-static {v6}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getTextFloat(Landroid/widget/EditText;)Ljava/lang/Float;

    move-result-object v7

    .line 444
    .local v7, "tension":Ljava/lang/Float;
    nop

    .line 445
    if-eqz v7, :cond_cc

    const/16 v8, 0xad

    aput-boolean v5, v2, v8

    .line 446
    new-instance v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomOvershootInterpolator;

    invoke-virtual {v7}, Ljava/lang/Float;->floatValue()F

    move-result v9

    invoke-direct {v8, v9}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomOvershootInterpolator;-><init>(F)V

    const/16 v9, 0xae

    aput-boolean v5, v2, v9

    goto :goto_d5

    .line 447
    :cond_cc
    new-instance v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomOvershootInterpolator;

    invoke-direct {v8}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomOvershootInterpolator;-><init>()V

    const/16 v9, 0xaf

    aput-boolean v5, v2, v9

    :goto_d5
    iput-object v8, v0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->interpolator:Landroid/view/animation/Interpolator;

    const/16 v8, 0xb0

    aput-boolean v5, v2, v8

    .line 448
    invoke-virtual/range {p2 .. p2}, Lcom/google/android/material/bottomsheet/BottomSheetDialog;->dismiss()V

    .line 449
    .end local v6    # "overshootTensionEditText":Landroid/widget/EditText;
    .end local v7    # "tension":Ljava/lang/Float;
    const/16 v6, 0xb1

    aput-boolean v5, v2, v6

    goto/16 :goto_170

    :cond_e4
    const v6, 0x7f09033a

    if-ne v4, v6, :cond_12a

    .line 450
    const v6, 0x7f090073

    const/16 v7, 0xb2

    aput-boolean v5, v2, v7

    .line 451
    invoke-virtual {v1, v6}, Landroid/view/View;->findViewById(I)Landroid/view/View;

    move-result-object v6

    check-cast v6, Landroid/widget/EditText;

    .restart local v6    # "overshootTensionEditText":Landroid/widget/EditText;
    const/16 v7, 0xb3

    aput-boolean v5, v2, v7

    .line 452
    invoke-static {v6}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->getTextFloat(Landroid/widget/EditText;)Ljava/lang/Float;

    move-result-object v7

    .line 453
    .restart local v7    # "tension":Ljava/lang/Float;
    nop

    .line 454
    if-eqz v7, :cond_113

    const/16 v8, 0xb4

    aput-boolean v5, v2, v8

    .line 455
    new-instance v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomAnticipateOvershootInterpolator;

    invoke-virtual {v7}, Ljava/lang/Float;->floatValue()F

    move-result v9

    invoke-direct {v8, v9}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomAnticipateOvershootInterpolator;-><init>(F)V

    const/16 v9, 0xb5

    aput-boolean v5, v2, v9

    goto :goto_11c

    .line 456
    :cond_113
    new-instance v8, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomAnticipateOvershootInterpolator;

    invoke-direct {v8}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper$CustomAnticipateOvershootInterpolator;-><init>()V

    const/16 v9, 0xb6

    aput-boolean v5, v2, v9

    :goto_11c
    iput-object v8, v0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->interpolator:Landroid/view/animation/Interpolator;

    const/16 v8, 0xb7

    aput-boolean v5, v2, v8

    .line 457
    invoke-virtual/range {p2 .. p2}, Lcom/google/android/material/bottomsheet/BottomSheetDialog;->dismiss()V

    .line 458
    .end local v6    # "overshootTensionEditText":Landroid/widget/EditText;
    .end local v7    # "tension":Ljava/lang/Float;
    const/16 v6, 0xb8

    aput-boolean v5, v2, v6

    goto :goto_170

    :cond_12a
    const v6, 0x7f09033b

    if-ne v4, v6, :cond_146

    const/16 v6, 0xb9

    aput-boolean v5, v2, v6

    .line 459
    new-instance v6, Landroid/view/animation/BounceInterpolator;

    invoke-direct {v6}, Landroid/view/animation/BounceInterpolator;-><init>()V

    iput-object v6, v0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->interpolator:Landroid/view/animation/Interpolator;

    const/16 v6, 0xba

    aput-boolean v5, v2, v6

    .line 460
    invoke-virtual/range {p2 .. p2}, Lcom/google/android/material/bottomsheet/BottomSheetDialog;->dismiss()V

    const/16 v6, 0xbb

    aput-boolean v5, v2, v6

    goto :goto_170

    .line 461
    :cond_146
    const v6, 0x7f090342

    if-ne v4, v6, :cond_162

    const/16 v6, 0xbc

    aput-boolean v5, v2, v6

    .line 462
    new-instance v6, Landroidx/interpolator/view/animation/FastOutSlowInInterpolator;

    invoke-direct {v6}, Landroidx/interpolator/view/animation/FastOutSlowInInterpolator;-><init>()V

    iput-object v6, v0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->interpolator:Landroid/view/animation/Interpolator;

    const/16 v6, 0xbd

    aput-boolean v5, v2, v6

    .line 463
    invoke-virtual/range {p2 .. p2}, Lcom/google/android/material/bottomsheet/BottomSheetDialog;->dismiss()V

    const/16 v6, 0xbe

    aput-boolean v5, v2, v6

    goto :goto_170

    .line 465
    :cond_162
    const/4 v6, 0x0

    iput-object v6, v0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->interpolator:Landroid/view/animation/Interpolator;

    const/16 v6, 0xbf

    aput-boolean v5, v2, v6

    .line 466
    invoke-virtual/range {p2 .. p2}, Lcom/google/android/material/bottomsheet/BottomSheetDialog;->dismiss()V

    const/16 v6, 0xc0

    aput-boolean v5, v2, v6

    .line 468
    :goto_170
    const/16 v6, 0xc1

    aput-boolean v5, v2, v6

    return-void
.end method

.method synthetic lambda$setUpBottomSheetConfirmationButtons$8$io-material-catalog-transition-ContainerTransformConfigurationHelper(Lcom/google/android/material/bottomsheet/BottomSheetDialog;Landroid/view/View;)V
    .registers 6
    .param p1, "dialog"    # Lcom/google/android/material/bottomsheet/BottomSheetDialog;
    .param p2, "v"    # Landroid/view/View;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 473
    invoke-direct {p0}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->setUpDefaultValues()V

    const/16 v1, 0x9e

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    .line 474
    invoke-virtual {p1}, Lcom/google/android/material/bottomsheet/BottomSheetDialog;->dismiss()V

    .line 475
    const/16 v1, 0x9f

    aput-boolean v2, v0, v1

    return-void
.end method

.method synthetic lambda$setUpBottomSheetDebugging$6$io-material-catalog-transition-ContainerTransformConfigurationHelper(Landroid/widget/CompoundButton;Z)V
    .registers 6
    .param p1, "buttonView"    # Landroid/widget/CompoundButton;
    .param p2, "isChecked"    # Z

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 418
    iput-boolean p2, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->drawDebugEnabled:Z

    const/16 v1, 0xc2

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    return-void
.end method

.method synthetic lambda$setUpBottomSheetEnterDurationSlider$2$io-material-catalog-transition-ContainerTransformConfigurationHelper(Lcom/google/android/material/slider/Slider;FZ)V
    .registers 7
    .param p1, "slider"    # Lcom/google/android/material/slider/Slider;
    .param p2, "value"    # F
    .param p3, "fromUser"    # Z

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 225
    float-to-long v1, p2

    iput-wide v1, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->enterDuration:J

    const/16 v1, 0xc7

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    return-void
.end method

.method synthetic lambda$setUpBottomSheetFadeModeButtonGroup$1$io-material-catalog-transition-ContainerTransformConfigurationHelper(Lcom/google/android/material/button/MaterialButtonToggleGroup;IZ)V
    .registers 7
    .param p1, "group"    # Lcom/google/android/material/button/MaterialButtonToggleGroup;
    .param p2, "checkedId"    # I
    .param p3, "isChecked"    # Z

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 211
    const/4 v1, 0x1

    if-nez p3, :cond_c

    const/16 v2, 0xc8

    aput-boolean v1, v0, v2

    goto :goto_12

    .line 212
    :cond_c
    iput p2, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->fadeModeButtonId:I

    const/16 v2, 0xc9

    aput-boolean v1, v0, v2

    .line 214
    :goto_12
    const/16 v2, 0xca

    aput-boolean v1, v0, v2

    return-void
.end method

.method synthetic lambda$setUpBottomSheetPathMotionButtonGroup$0$io-material-catalog-transition-ContainerTransformConfigurationHelper(Lcom/google/android/material/button/MaterialButtonToggleGroup;IZ)V
    .registers 7
    .param p1, "group"    # Lcom/google/android/material/button/MaterialButtonToggleGroup;
    .param p2, "checkedId"    # I
    .param p3, "isChecked"    # Z

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 196
    const/4 v1, 0x1

    const v2, 0x7f09007a

    if-eq p2, v2, :cond_f

    const/16 v2, 0xcb

    aput-boolean v1, v0, v2

    goto :goto_15

    .line 197
    :cond_f
    iput-boolean p3, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->arcMotionEnabled:Z

    const/16 v2, 0xcc

    aput-boolean v1, v0, v2

    .line 199
    :goto_15
    const/16 v2, 0xcd

    aput-boolean v1, v0, v2

    return-void
.end method

.method synthetic lambda$setUpBottomSheetReturnDurationSlider$3$io-material-catalog-transition-ContainerTransformConfigurationHelper(Lcom/google/android/material/slider/Slider;FZ)V
    .registers 7
    .param p1, "slider"    # Lcom/google/android/material/slider/Slider;
    .param p2, "value"    # F
    .param p3, "fromUser"    # Z

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 235
    float-to-long v1, p2

    iput-wide v1, p0, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->returnDuration:J

    const/16 v1, 0xc6

    const/4 v2, 0x1

    aput-boolean v2, v0, v1

    return-void
.end method

.method showConfigurationChooser(Landroid/content/Context;Landroid/content/DialogInterface$OnDismissListener;)V
    .registers 8
    .param p1, "context"    # Landroid/content/Context;
    .param p2, "onDismissListener"    # Landroid/content/DialogInterface$OnDismissListener;

    invoke-static {}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->$jacocoInit()[Z

    move-result-object v0

    .line 89
    new-instance v1, Lcom/google/android/material/bottomsheet/BottomSheetDialog;

    invoke-direct {v1, p1}, Lcom/google/android/material/bottomsheet/BottomSheetDialog;-><init>(Landroid/content/Context;)V

    .line 90
    .local v1, "bottomSheetDialog":Lcom/google/android/material/bottomsheet/BottomSheetDialog;
    const/4 v2, 0x2

    const/4 v3, 0x1

    aput-boolean v3, v0, v2

    .line 91
    invoke-direct {p0, p1, v1}, Lio/material/catalog/transition/ContainerTransformConfigurationHelper;->createConfigurationBottomSheetView(Landroid/content/Context;Lcom/google/android/material/bottomsheet/BottomSheetDialog;)Landroid/view/View;

    move-result-object v2

    const/4 v4, 0x3

    aput-boolean v3, v0, v4

    .line 90
    invoke-virtual {v1, v2}, Lcom/google/android/material/bottomsheet/BottomSheetDialog;->setContentView(Landroid/view/View;)V

    const/4 v2, 0x4

    aput-boolean v3, v0, v2

    .line 92
    invoke-virtual {v1, p2}, Lcom/google/android/material/bottomsheet/BottomSheetDialog;->setOnDismissListener(Landroid/content/DialogInterface$OnDismissListener;)V

    const/4 v2, 0x5

    aput-boolean v3, v0, v2

    .line 93
    invoke-virtual {v1}, Lcom/google/android/material/bottomsheet/BottomSheetDialog;->show()V

    .line 94
    const/4 v2, 0x6

    aput-boolean v3, v0, v2

    return-void
.end method
