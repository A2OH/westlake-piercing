/*
 * OhImeBridge.java
 *
 * Android IMM <-> OHOS inputMethod bridge (Java half).
 *
 * Companion to liboh_adapter_bridge.so's input_method_bridge.cpp. The native
 * side Attaches an OHOS OnTextChangedListener to InputMethodController and
 * shows/hides the soft keyboard; this class:
 *   - exposes show()/hide() that call the native entry points
 *     (nativeShowKeyboard/nativeHideKeyboard, registered by the bridge on THIS
 *     class via RegisterNatives), and
 *   - exposes nativeOn* statics the native listener calls back to, each of
 *     which performs one InputConnection edit on the Android UI thread against
 *     the currently-focused EditText (commitText / deleteSurroundingText /
 *     ENTER key). commitText mutates the live Editable so the text appears.
 *
 * This is a standalone (non-BCP-coupled) class so it compiles against the
 * public android.jar; ViewRootImpl / WindowManagerGlobal (hidden) are reached
 * via reflection. adapter.window.InputMethodManagerAdapter (the registered
 * "input_method" stub) just forwards showSoftInput/hideSoftInput/
 * startInputOrWindowGainedFocus here.
 */
package adapter.window;

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import java.lang.reflect.Field;
import java.util.ArrayList;

public final class OhImeBridge {

    private static final String TAG = "OH_IMEBridge";

    private OhImeBridge() {}

    // ---- native entry points (registered by liboh_adapter_bridge.so) ----
    private static native boolean nativeShowKeyboard();
    private static native boolean nativeHideKeyboard();

    /** Summon the OHOS soft keyboard. Called from the IMM adapter. */
    public static boolean show() {
        try {
            boolean r = nativeShowKeyboard();
            System.err.println("[" + TAG + "] show -> nativeShowKeyboard=" + r);
            return r;
        } catch (Throwable t) {
            System.err.println("[" + TAG + "] show failed: " + t);
            return false;
        }
    }

    /** Hide the OHOS soft keyboard. Called from the IMM adapter. */
    public static boolean hide() {
        try {
            boolean r = nativeHideKeyboard();
            System.err.println("[" + TAG + "] hide -> nativeHideKeyboard=" + r);
            return r;
        } catch (Throwable t) {
            System.err.println("[" + TAG + "] hide failed: " + t);
            return false;
        }
    }

    // ------------------------------------------------------------------
    // Text routing: OHOS keyboard events -> focused Android InputConnection
    // ------------------------------------------------------------------
    static final int OP_INSERT = 0;
    static final int OP_DEL_BEFORE = 1;
    static final int OP_DEL_AFTER = 2;
    static final int OP_ENTER = 3;

    private static volatile Handler sUiHandler;

    private static Handler uiHandler() {
        if (sUiHandler == null) {
            synchronized (OhImeBridge.class) {
                if (sUiHandler == null) {
                    sUiHandler = new Handler(Looper.getMainLooper());
                }
            }
        }
        return sUiHandler;
    }

    private static Field sRootsField;     // WindowManagerGlobal.mRoots
    private static Field sViewField;      // ViewRootImpl.mView

    /** Reflectively locate the InputConnection of the focused editable View. */
    static InputConnection focusedInputConnection() {
        try {
            Class<?> wmgCls = Class.forName("android.view.WindowManagerGlobal");
            Object wmg = wmgCls.getMethod("getInstance").invoke(null);
            if (sRootsField == null) {
                sRootsField = wmgCls.getDeclaredField("mRoots");
                sRootsField.setAccessible(true);
            }
            Object rootsObj = sRootsField.get(wmg);
            if (!(rootsObj instanceof ArrayList)) return null;
            ArrayList<?> roots = (ArrayList<?>) rootsObj;
            Class<?> vriCls = Class.forName("android.view.ViewRootImpl");
            if (sViewField == null) {
                sViewField = vriCls.getDeclaredField("mView");
                sViewField.setAccessible(true);
            }
            View focusView = null;
            View fallback = null;
            for (int i = 0; i < roots.size(); i++) {
                Object vri = roots.get(i);
                if (vri == null) continue;
                Object vObj = sViewField.get(vri);
                if (!(vObj instanceof View)) continue;
                View v = (View) vObj;
                View ff = v.findFocus();
                // 2026-06-26 INPUT-ROUTING DIAG: log every root's focus state to
                // see why typed text doesn't reach a 2nd-Activity's search field.
                System.err.println("[" + TAG + "] IC-scan root#" + i
                        + " rootView=" + v.getClass().getName()
                        + " hasWindowFocus=" + v.hasWindowFocus()
                        + " findFocus=" + (ff == null ? "null" : ff.getClass().getName())
                        + " isTextEditor=" + (ff != null && ff.onCheckIsTextEditor()));
                if (ff != null && ff.onCheckIsTextEditor()) {
                    if (v.hasWindowFocus()) { focusView = ff; break; }
                    if (fallback == null) fallback = ff;
                }
            }
            if (focusView == null) focusView = fallback;
            if (focusView == null) {
                System.err.println("[" + TAG + "] focusedInputConnection: no editable focus");
                return null;
            }
            EditorInfo ei = new EditorInfo();
            InputConnection ic = focusView.onCreateInputConnection(ei);
            // 2026-06-26 INPUT-ROUTING DIAG: which view + IC actually won.
            System.err.println("[" + TAG + "] IC-chosen view=" + focusView.getClass().getName()
                    + " ic=" + (ic == null ? "null" : ic.getClass().getName()));
            if (ic == null) {
                System.err.println("[" + TAG + "] focusedInputConnection: view returned null IC ("
                        + focusView.getClass().getName() + ")");
            }
            return ic;
        } catch (Throwable t) {
            System.err.println("[" + TAG + "] focusedInputConnection: " + t);
            return null;
        }
    }

    private static void runOp(final int op, final int arg, final String text) {
        uiHandler().post(new Runnable() {
            @Override public void run() {
                try {
                    InputConnection ic = focusedInputConnection();
                    if (ic == null) return;
                    ic.beginBatchEdit();
                    ic.finishComposingText();
                    if (op == OP_INSERT) {
                        ic.commitText(text, 1);
                    } else if (op == OP_DEL_BEFORE) {
                        ic.deleteSurroundingText(arg > 0 ? arg : 1, 0);
                    } else if (op == OP_DEL_AFTER) {
                        ic.deleteSurroundingText(0, arg > 0 ? arg : 1);
                    } else if (op == OP_ENTER) {
                        long now = SystemClock.uptimeMillis();
                        ic.sendKeyEvent(new KeyEvent(now, now, KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_ENTER, 0));
                        ic.sendKeyEvent(new KeyEvent(now, now, KeyEvent.ACTION_UP,
                                KeyEvent.KEYCODE_ENTER, 0));
                    }
                    ic.endBatchEdit();
                    System.err.println("[" + TAG + "] runOp(" + op + ") done");
                } catch (Throwable t) {
                    System.err.println("[" + TAG + "] runOp(" + op + ") failed: " + t);
                }
            }
        });
    }

    /** Called from native: commit text into the focused field. */
    public static void nativeOnInsertText(String text) { runOp(OP_INSERT, 0, text); }

    /** Called from native: backspace (delete `n` chars before cursor). */
    public static void nativeOnDeleteBefore(int n) { runOp(OP_DEL_BEFORE, n, null); }

    /** Called from native: forward-delete (delete `n` chars after cursor). */
    public static void nativeOnDeleteAfter(int n) { runOp(OP_DEL_AFTER, n, null); }

    /** Called from native: enter / IME action key. */
    public static void nativeOnEnterAction(int enterKeyType) { runOp(OP_ENTER, enterKeyType, null); }
}
