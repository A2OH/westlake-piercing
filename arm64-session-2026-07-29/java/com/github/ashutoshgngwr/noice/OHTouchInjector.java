package com.github.ashutoshgngwr.noice;

import android.view.MotionEvent;
import android.view.View;

/**
 * §554 — dispatch injected touches on the UI thread.
 *
 * oh_input_bridge looks for exactly this class (BCP `adapter/window/OHTouchInjector` first, then
 * this per-app fallback). Without it the bridge calls View.dispatchTouchEvent DIRECTLY from its
 * injector thread, which is fine for ACTION_DOWN but breaks ACTION_UP on any editable view:
 *
 *   dispatchTouchViaViewRoot: DOWN handled=1 threw=0
 *   dispatchTouchViaViewRoot UP: EXCEPTION ViewRootImpl$CalledFromWrongThreadException:
 *       Only the original thread that created a view hierarchy can touch its views.
 *   dispatchTouchViaViewRoot: UP handled=0 threw=1
 *
 * The UP is what focuses the field and starts the cursor, so it mutates view state and trips
 * ViewRootImpl.checkThread(). Net effect: NO text field on the board could be focused or typed into.
 *
 * ⚠️THE FOUR STATIC COUNTERS BELOW ARE PART OF THE CONTRACT, not decoration. After dispatching, the
 * bridge does GetStaticFieldID for `invokeCount`, `runCount`, `lastHandled` (I) and `lastEx`
 * (String) and reads them for its OHTI log line. A missing field raises NoSuchFieldError that the
 * native side never clears, and the runtime then aborts:
 *     Pending exception java.lang.NoSuchFieldError: no "I" field "runCount" in class
 *     "Lcom/github/ashutoshgngwr/noice/OHTouchInjector;"
 * — which killed the child on the first tap when this class shipped without them.
 *
 * ⚠️Named inner class, never an anonymous one — d8 silently drops anonymous classes here.
 * ⚠️The native caller never recycles the MotionEvent (verified: no recycle() in oh_input_bridge),
 * so it is safe to hold it until the post runs. The bridge also sleeps ~100 ms after the UP so the
 * posted Runnable has time to complete on the UI thread.
 */
public final class OHTouchInjector {

    public static int invokeCount = 0;
    public static int runCount = 0;
    public static int lastHandled = -1;
    public static String lastEx = "none";

    public static void dispatchTouchOnMain(View v, MotionEvent ev) {
        invokeCount++;
        if (v == null || ev == null) {
            lastEx = "null arg";
            return;
        }
        Task t = new Task(v, ev);
        // post() returns false when the view has no handler yet (not attached). Falling back to a
        // direct call is still better than dropping the event — that is the old behaviour.
        if (!v.post(t)) {
            t.run();
        }
    }

    static final class Task implements Runnable {
        private final View view;
        private final MotionEvent event;

        Task(View view, MotionEvent event) {
            this.view = view;
            this.event = event;
        }

        @Override
        public void run() {
            runCount++;
            try {
                lastHandled = view.dispatchTouchEvent(event) ? 1 : 0;
                lastEx = "none";
            } catch (Throwable t) {
                lastHandled = -1;
                lastEx = String.valueOf(t);
            }
        }
    }
}
