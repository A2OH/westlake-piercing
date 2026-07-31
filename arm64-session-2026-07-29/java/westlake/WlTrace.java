package westlake;

/**
 * WESTLAKE §480 — execution tracing for the app dex.
 *
 * One zero-arg static per trace site, so the injected call needs NO free register (see
 * tools/InjectTrace.java). Output goes through adapter.compat.WlProbe's native sink, because
 * System.err from app code is routed into the log framework and Throwable stacks are unavailable in
 * this runtime.
 */
public final class WlTrace {
    private WlTrace() {}

    private static void hit(String s) {
        try {
            adapter.compat.WlProbe.log("[WESTLAKE-480] " + s);
        } catch (Throwable ignored) { }
    }

    /** Log a VALUE. Injected where the object is already in a known register (see InjectTrace). */
    public static void obj(Object o) { hit("VALUE = " + o); }

    public static void s0() { hit("SoundPlayerManager.g ENTER"); }
    public static void s1() { hit("SoundPlayerManager.g -> calling SoundPlayer.play()"); }
    public static void s2() { hit("SoundPlayerManager.i ENTER (post-play update)"); }
    public static void s3() { hit("focus.b() ENTER (requestFocus)"); }
    public static void s4() { hit("focus.onAudioFocusChange ENTER"); }
    public static void s5() { hit("LocalSoundPlayer.e() PLAY called"); }
    public static void s6() { hit("SoundPlayerManager focus-callback b() [resume]"); }
    public static void s7() { hit("onStartCommand ENTER"); }
    public static void s8() { hit("playSound branch: equals matched, about to call l()"); }
    public static void s9() { hit("about to call SoundPlayerManager.g(soundId)"); }
    public static void s10() { hit("LocalSoundPlayer.o() ENTER (enqueue segments)"); }
    public static void s11() { hit("o(): segment list NON-EMPTY — loop body reached"); }
    public static void s12() { hit("e(): took ALREADY-PLAYING branch (just fades volume)"); }
    public static void s13() { hit("e(): took NOT-PLAYING branch (would load/enqueue)"); }
    public static void s14() { hit("LocalSoundPlayer.a(MediaPlayer.State) — media state callback"); }
    public static void s15() { hit("LocalSoundPlayer.b() "); }
    public static void s16() { hit("LocalSoundPlayer.d(boolean)"); }
    public static void s17() { hit("LocalSoundPlayer.m(boolean)"); }
    public static void s18() { hit("SoundPlayer.k(State) — state transition"); }
    public static void s31() { hit("y2/v.b ENTER — DAO impl reached"); }
    public static void s32() { hit("y2/v.b: Callable.call() returned — SQL ran"); }

    public static void s29() { hit("cache lambda: about to call Room suspend DAO query"); }
    public static void s30() { hit("cache lambda: Room DAO query RESUMED (row in hand)"); }

    public static void s26() { hit("SoundRepository.get: loadFromCache lambda RAN"); }
    public static void s27() { hit("SoundRepository.get: loadFromNetwork lambda RAN"); }
    public static void s28() { hit("fetchNetworkBoundResource g.a() ENTER (flow builder)"); }

    public static void s24() { hit("SoundMetadataSource.load() ENTER — repository fetch starts"); }
    public static void s25() { hit("loadSoundMetadata: RESUMED past the load (segments in hand)"); }

    public static void s22() { hit("loadSoundMetadataJob invokeSuspend v() — coroutine body RAN"); }
    public static void s23() { hit("loadSoundMetadataJob k() — coroutine invoke()"); }

    public static void s20() { hit("LocalSoundPlayer.n() = loadSoundMetadata ENTER (suspend)"); }
    public static void s21() { hit("LocalSoundPlayer.o() enqueue reached"); }

    public static void s19() { hit("LocalSoundPlayer.<init> constructed"); }

}
