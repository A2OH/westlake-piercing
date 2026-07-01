# diagnostics

Small standalone tools used to root-cause and validate the adapter audio path.

- **deathcatch.c** — arm32 LD_PRELOAD. Interposes exit/_exit/abort/pthread_exit
  and the exit_group syscall + chains a fatal-signal handler, logging a native
  backtrace to fd 2. This proved the noice "clean death" (no tombstone) is an
  uncaught Java NPE -> `AppSpawnXInit` handler -> `System.exit(1)` -> `JVM_Halt`
  -> `Runtime::CallExitHook` -> `_exit(1)`, NOT a native crash / signal-recovery
  failure. Add to the appspawn-x `LD_PRELOAD`.
- **toneplayer.c** — arm32. dlopens `/system/lib/ndk/libohaudio.so` and plays a
  440 Hz sine via `OH_AudioRenderer` (the target of the AudioTrack shim). Verifies
  the OHOS audio *output* backend works end-to-end (Create/GenerateRenderer/Start
  all rc=0, callback pulls PCM).
- **netcheck.c** — arm32. Connects to 127.0.0.1:8080 and sends an HTTP CONNECT to
  a public host; validates the device->host proxy tunnel (see proxy.py).
- **proxy.py** — host-side HTTP/HTTPS CONNECT forward proxy. Pair with
  `hdc rport tcp:8080 tcp:8080` to give an offline board a route to the internet;
  the proxy resolves hostnames host-side so the device needs no DNS. HTTPS is
  tunneled via CONNECT (end-to-end TLS, no MITM/cert issues).

Cross-compile (arm32, OHOS musl):
  clang --target=arm-linux-ohos -march=armv7-a --sysroot=$OHOS_MUSL_SYSROOT \
        -B$OHOS_MUSL_SYSROOT/lib/arm-linux-ohos -O2 -o <out> <file>.c
