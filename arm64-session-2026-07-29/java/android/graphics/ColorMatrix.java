package android.graphics;

import java.util.Arrays;

/**
 * WESTLAKE §675 (2026-08-18) — real android.graphics.ColorMatrix.
 *
 * The shipped adapter-mainline-stubs.jar carried a stub whose entire body was `<init>()V`
 * (verified with DumpCls: "CLASS Landroid/graphics/ColorMatrix; ... <init>()V codeUnits=4").
 * Toutiao's com.ss.android.image.AsyncImageView and com.ss.android.common.util.UiUtils both
 * call ColorMatrix.set(float[]) from their <clinit>, so both died with
 *   NoSuchMethodError: No InvokeType(2) method set([F)V in class Landroid/graphics/ColorMatrix;
 * and were left half-initialised by the tolerate-clinit-failure path. AsyncImageView is the view
 * class every Toutiao feed row uses; feed row layouts (NightModeAsyncImageView) were being
 * inflated while FeedCommonRecyclerView stayed empty.
 *
 * Pure Java in AOSP — no native code, so a faithful reimplementation is enough. Semantics and
 * coefficients follow frameworks/base/graphics/java/android/graphics/ColorMatrix.java.
 */
public class ColorMatrix {

    private final float[] mArray = new float[20];

    /** Create a new colormatrix initialized to identity (as if reset() had been called). */
    public ColorMatrix() {
        reset();
    }

    /** Create a new colormatrix initialized with the specified array of values. */
    public ColorMatrix(float[] src) {
        System.arraycopy(src, 0, mArray, 0, 20);
    }

    /** Create a new colormatrix initialized with the specified colormatrix. */
    public ColorMatrix(ColorMatrix src) {
        System.arraycopy(src.mArray, 0, mArray, 0, 20);
    }

    /** Return the array of floats representing this colormatrix. */
    public final float[] getArray() {
        return mArray;
    }

    /** Set this colormatrix to identity. */
    public void reset() {
        final float[] a = mArray;
        Arrays.fill(a, 0);
        a[0] = a[6] = a[12] = a[18] = 1;
    }

    /** Assign the src colormatrix into this matrix, copying its values. */
    public void set(ColorMatrix src) {
        System.arraycopy(src.mArray, 0, mArray, 0, 20);
    }

    /** Assign the array of floats into this matrix, copying all of its values. */
    public void set(float[] src) {
        System.arraycopy(src, 0, mArray, 0, 20);
    }

    /** Set this colormatrix to scale by the specified values. */
    public void setScale(float rScale, float gScale, float bScale, float aScale) {
        final float[] a = mArray;
        for (int i = 19; i > 0; --i) {
            a[i] = 0;
        }
        a[0] = rScale;
        a[6] = gScale;
        a[12] = bScale;
        a[18] = aScale;
    }

    /**
     * Set the rotation on a color axis by the specified values.
     * axis=0 rotates around red, 1 around green, 2 around blue.
     */
    public void setRotate(int axis, float degrees) {
        reset();
        double radians = degrees * Math.PI / 180d;
        float cosine = (float) Math.cos(radians);
        float sine = (float) Math.sin(radians);
        switch (axis) {
            case 0:
                mArray[6] = mArray[12] = cosine;
                mArray[7] = sine;
                mArray[11] = -sine;
                break;
            case 1:
                mArray[0] = mArray[12] = cosine;
                mArray[2] = -sine;
                mArray[10] = sine;
                break;
            case 2:
                mArray[0] = mArray[6] = cosine;
                mArray[1] = sine;
                mArray[5] = -sine;
                break;
            default:
                throw new RuntimeException();
        }
    }

    /** Set this colormatrix to the concatenation of the two specified colormatrices. */
    public void setConcat(ColorMatrix matA, ColorMatrix matB) {
        float[] tmp = new float[20];
        int index = 0;
        for (int j = 0; j < 20; j += 5) {
            for (int i = 0; i < 4; i++) {
                tmp[index++] = matA.mArray[j + 0] * matB.mArray[i + 0]
                        + matA.mArray[j + 1] * matB.mArray[i + 5]
                        + matA.mArray[j + 2] * matB.mArray[i + 10]
                        + matA.mArray[j + 3] * matB.mArray[i + 15];
            }
            tmp[index++] = matA.mArray[j + 0] * matB.mArray[4]
                    + matA.mArray[j + 1] * matB.mArray[9]
                    + matA.mArray[j + 2] * matB.mArray[14]
                    + matA.mArray[j + 3] * matB.mArray[19]
                    + matA.mArray[j + 4];
        }
        System.arraycopy(tmp, 0, mArray, 0, 20);
    }

    /** Concat this colormatrix with the specified prematrix. */
    public void preConcat(ColorMatrix prematrix) {
        setConcat(this, prematrix);
    }

    /** Concat this colormatrix with the specified postmatrix. */
    public void postConcat(ColorMatrix postmatrix) {
        setConcat(postmatrix, this);
    }

    /**
     * Set the matrix to affect the saturation of colors.
     * @param sat 0 maps to greyscale, 1 leaves colors unchanged, >1 increases saturation.
     */
    public void setSaturation(float sat) {
        reset();
        float[] m = mArray;

        final float invSat = 1 - sat;
        final float R = 0.213f * invSat;
        final float G = 0.715f * invSat;
        final float B = 0.072f * invSat;

        m[0] = R + sat;
        m[1] = G;
        m[2] = B;
        m[5] = R;
        m[6] = G + sat;
        m[7] = B;
        m[10] = R;
        m[11] = G;
        m[12] = B + sat;
    }

    /** Set the matrix to convert RGB to YUV. */
    public void setRGB2YUV() {
        reset();
        float[] m = mArray;
        // these coefficients match those in libjpeg
        m[0] = 0.299f;
        m[1] = 0.587f;
        m[2] = 0.114f;
        m[5] = -0.16874f;
        m[6] = -0.33126f;
        m[7] = 0.5f;
        m[10] = 0.5f;
        m[11] = -0.41869f;
        m[12] = -0.08131f;
    }

    /** Set the matrix to convert from YUV to RGB. */
    public void setYUV2RGB() {
        reset();
        float[] m = mArray;
        // these coefficients match those in libjpeg
        m[2] = 1.402f;
        m[5] = 1;
        m[6] = -0.34414f;
        m[7] = -0.71414f;
        m[10] = 1;
        m[11] = 1.772f;
        m[12] = 0;
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof ColorMatrix)) {
            return false;
        }
        final ColorMatrix other = (ColorMatrix) obj;
        return Arrays.equals(mArray, other.mArray);
    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(mArray);
    }

    @Override
    public String toString() {
        return "ColorMatrix" + Arrays.toString(mArray);
    }
}
