/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fault_handler.h"

#include <string.h>
#include <unistd.h>   // [W15-PROBE] write(2) for async-signal-safe emit
#include <sys/mman.h>
#include <sys/ucontext.h>

#include <atomic>

#include "art_method-inl.h"
#include "base/logging.h"  // For VLOG
#include "base/membarrier.h"
#include "base/safe_copy.h"
#include "base/stl_util.h"
#include "dex/dex_file.h"   // [W15-PROBE] no-alloc method-name accessors
#include "dex/dex_file_types.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"  // [W21-PROBE] boot-image ArtMethod scan
#include "image.h"                  // [W21-PROBE] ImageHeader / ImageSection
#include "interpreter/shadow_frame.h"   // [W15-PROBE] walk interpreted (app) call chain
#include "managed_stack.h"              // [W15-PROBE] ManagedStack segment links
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "mirror/class.h"
#include "mirror/object_reference.h"
#include "oat_file.h"
#include "oat_quick_method_header.h"
#include <dlfcn.h>
#include "sigchain.h"
#include "thread-current-inl.h"
#include "verify_object-inl.h"

namespace art {
// Static fault manger object accessed by signal handler.
FaultManager fault_manager;

// This needs to be NO_INLINE since some debuggers do not read the inline-info to set a breakpoint
// if it isn't.
extern "C" NO_INLINE __attribute__((visibility("default"))) void art_sigsegv_fault() {
  // Set a breakpoint here to be informed when a SIGSEGV is unhandled by ART.
  VLOG(signals)<< "Caught unknown SIGSEGV in ART fault handler - chaining to next handler.";
}

// Signal handler called on SIGSEGV.
static bool art_sigsegv_handler(int sig, siginfo_t* info, void* context) {
  return fault_manager.HandleSigsegvFault(sig, info, context);
}

// Signal handler called on SIGBUS.
static bool art_sigbus_handler(int sig, siginfo_t* info, void* context) {
  return fault_manager.HandleSigbusFault(sig, info, context);
}

FaultManager::FaultManager()
    : generated_code_ranges_lock_("FaultHandler generated code ranges lock",
                                  LockLevel::kGenericBottomLock),
      initialized_(false) {}

FaultManager::~FaultManager() {
}

static const char* SignalCodeName(int sig, int code) {
  if (sig == SIGSEGV) {
    switch (code) {
      case SEGV_MAPERR: return "SEGV_MAPERR";
      case SEGV_ACCERR: return "SEGV_ACCERR";
      case 8:           return "SEGV_MTEAERR";
      case 9:           return "SEGV_MTESERR";
      default:          return "SEGV_UNKNOWN";
    }
  } else if (sig == SIGBUS) {
    switch (code) {
      case BUS_ADRALN: return "BUS_ADRALN";
      case BUS_ADRERR: return "BUS_ADRERR";
      case BUS_OBJERR: return "BUS_OBJERR";
      default:         return "BUS_UNKNOWN";
    }
  } else {
    return "UNKNOWN";
  }
}

static std::ostream& PrintSignalInfo(std::ostream& os, siginfo_t* info) {
  os << "  si_signo: " << info->si_signo << " (" << strsignal(info->si_signo) << ")\n"
     << "  si_code: " << info->si_code
     << " (" << SignalCodeName(info->si_signo, info->si_code) << ")";
  if (info->si_signo == SIGSEGV || info->si_signo == SIGBUS) {
    os << "\n" << "  si_addr: " << info->si_addr;
  }
  return os;
}

static bool InstallSigbusHandler() {
  return gUseUserfaultfd &&
         Runtime::Current()->GetHeap()->MarkCompactCollector()->IsUsingSigbusFeature();
}

void FaultManager::Init(bool use_sig_chain) {
  CHECK(!initialized_);
  if (use_sig_chain) {
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGABRT);
    sigdelset(&mask, SIGBUS);
    sigdelset(&mask, SIGFPE);
    sigdelset(&mask, SIGILL);
    sigdelset(&mask, SIGSEGV);

    SigchainAction sa = {
        .sc_sigaction = art_sigsegv_handler,
        .sc_mask = mask,
        .sc_flags = 0UL,
    };

    AddSpecialSignalHandlerFn(SIGSEGV, &sa);
    if (InstallSigbusHandler()) {
      sa.sc_sigaction = art_sigbus_handler;
      AddSpecialSignalHandlerFn(SIGBUS, &sa);
    }

    // [W15-PV 2026-05-30] OHOS musl's sigchain (newer than our source) does a context
    // "pre-validation": it restores the original ucontext after a chained handler returns,
    // silently discarding ART's mc->arm_pc edit (the SIGSEGV->NPE redirect). Result: every
    // implicit null/bounds/stack check is FATAL instead of a catchable exception (W15 root).
    // The device musl exports set_sigchain_pre_validation_ignore(signo) to disable that
    // pre-validation per signal; ART (bionic-derived) never calls it. Resolve it at runtime
    // (the device libc provides it; our build sysroot's older musl does not) and disable
    // pre-validation for SIGSEGV/SIGBUS so the handler's PC redirect survives sigreturn.
    {
      void* pv = dlsym(RTLD_DEFAULT, "set_sigchain_pre_validation_ignore");
      if (pv != nullptr) {
        reinterpret_cast<void (*)(int, int)>(pv)(SIGSEGV, 1);
        reinterpret_cast<void (*)(int, int)>(pv)(SIGBUS, 1);
        LOG(INFO) << "[W15-PV] set_sigchain_pre_validation_ignore(SIGSEGV,SIGBUS) applied";
      } else {
        LOG(WARNING) << "[W15-PV] set_sigchain_pre_validation_ignore not found (NPE redirect may be dropped)";
      }
    }

    // Notify the kernel that we intend to use a specific `membarrier()` command.
    int result = art::membarrier(MembarrierCommand::kRegisterPrivateExpedited);
    if (result != 0) {
      LOG(WARNING) << "FaultHandler: MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED failed: "
                   << errno << " " << strerror(errno);
    }

    {
      MutexLock lock(Thread::Current(), generated_code_ranges_lock_);
      for (size_t i = 0; i != kNumLocalGeneratedCodeRanges; ++i) {
        GeneratedCodeRange* next = (i + 1u != kNumLocalGeneratedCodeRanges)
            ? &generated_code_ranges_storage_[i + 1u]
            : nullptr;
        generated_code_ranges_storage_[i].next.store(next, std::memory_order_relaxed);
        generated_code_ranges_storage_[i].start = nullptr;
        generated_code_ranges_storage_[i].size = 0u;
      }
      free_generated_code_ranges_ = generated_code_ranges_storage_;
    }

    initialized_ = true;
  } else if (InstallSigbusHandler()) {
    struct sigaction act;
    std::memset(&act, '\0', sizeof(act));
    act.sa_flags = SA_SIGINFO | SA_RESTART;
    act.sa_sigaction = [](int sig, siginfo_t* info, void* context) {
      if (!art_sigbus_handler(sig, info, context)) {
        std::ostringstream oss;
        PrintSignalInfo(oss, info);
        LOG(FATAL) << "Couldn't handle SIGBUS fault:"
                   << "\n"
                   << oss.str();
      }
    };
    if (sigaction(SIGBUS, &act, nullptr)) {
      LOG(FATAL) << "Fault handler for SIGBUS couldn't be setup: " << strerror(errno);
    }
  }
}

void FaultManager::Release() {
  if (initialized_) {
    RemoveSpecialSignalHandlerFn(SIGSEGV, art_sigsegv_handler);
    if (InstallSigbusHandler()) {
      RemoveSpecialSignalHandlerFn(SIGBUS, art_sigbus_handler);
    }
    initialized_ = false;
  }
}

void FaultManager::Shutdown() {
  if (initialized_) {
    Release();

    // Free all handlers.
    STLDeleteElements(&generated_code_handlers_);
    STLDeleteElements(&other_handlers_);

    // Delete remaining code ranges if any (such as nterp code or oat code from
    // oat files that have not been unloaded, including boot image oat files).
    MutexLock lock(Thread::Current(), generated_code_ranges_lock_);
    GeneratedCodeRange* range = generated_code_ranges_.load(std::memory_order_acquire);
    generated_code_ranges_.store(nullptr, std::memory_order_release);
    while (range != nullptr) {
      GeneratedCodeRange* next_range = range->next.load(std::memory_order_relaxed);
      std::less<GeneratedCodeRange*> less;
      if (!less(range, generated_code_ranges_storage_) &&
          less(range, generated_code_ranges_storage_ + kNumLocalGeneratedCodeRanges)) {
        // Nothing to do - not adding `range` to the `free_generated_code_ranges_` anymore.
      } else {
        // Range is not in the `generated_code_ranges_storage_`.
        delete range;
      }
      range = next_range;
    }
  }
}

bool FaultManager::HandleFaultByOtherHandlers(int sig, siginfo_t* info, void* context) {
  if (other_handlers_.empty()) {
    return false;
  }

  Thread* self = Thread::Current();

  DCHECK(self != nullptr);
  DCHECK(Runtime::Current() != nullptr);
  DCHECK(Runtime::Current()->IsStarted());
  for (const auto& handler : other_handlers_) {
    if (handler->Action(sig, info, context)) {
      return true;
    }
  }
  return false;
}

bool FaultManager::HandleSigbusFault(int sig, siginfo_t* info, void* context ATTRIBUTE_UNUSED) {
  DCHECK_EQ(sig, SIGBUS);
  if (VLOG_IS_ON(signals)) {
    PrintSignalInfo(VLOG_STREAM(signals) << "Handling SIGBUS fault:\n", info);
  }

#ifdef TEST_NESTED_SIGNAL
  // Simulate a crash in a handler.
  raise(SIGBUS);
#endif
  return Runtime::Current()->GetHeap()->MarkCompactCollector()->SigbusHandler(info);
}

// [W15-PROBE] async-signal-safe unbuffered emit to fd 2 (child stderr). Avoids the
// stdio FILE* lock + buffering: a buffered fprintf is lost if a later nested fault
// kills the process before fflush (the likely reason the first probe was silent).
static void W15Write(const char* b, int n) {
  if (n <= 0) return;
  int off = 0;
  while (off < n) {
    ssize_t w = write(2, b + off, static_cast<size_t>(n - off));
    if (w <= 0) break;
    off += static_cast<int>(w);
  }
}

bool FaultManager::HandleSigsegvFault(int sig, siginfo_t* info, void* context) {
  if (VLOG_IS_ON(signals)) {
    PrintSignalInfo(VLOG_STREAM(signals) << "Handling SIGSEGV fault:\n", info);
  }

  // === [W15-PROBE] entry marker — UNGUARDED so it survives even if IsInGeneratedCode
  // is false or the later method scan faults. Reveals pc/sp + whether the fault is
  // classified as generated code (inGen==0 alone would explain fatal-not-NPE).
  {
    Thread* w15t = Thread::Current();
    char hb[256];
    int hn = snprintf(hb, sizeof(hb),
        "[W15-PROBE] HandleSigsegvFault pc=0x%zx sp=0x%zx inGen=%d state=%d\n",
        static_cast<size_t>(GetFaultPc(info, context)),
        static_cast<size_t>(GetFaultSp(context)),
        IsInGeneratedCode(info, context) ? 1 : 0,
        (w15t != nullptr) ? static_cast<int>(w15t->GetState()) : -1);
    W15Write(hb, hn);
  }

  // === [W15-PROBE] Decode fault-register objects. The crash instruction decoded as
  // r5=[r1+8]; r6=r1(obj); r1=r5; LDR r0,[r1] -> so r6 holds the base object whose
  // field@offset8 is null. Print the class descriptor of every register that points at
  // a readable heap object; r6's class names the object type with the missing field.
#if defined(__arm__)
  {
    ucontext_t* w15uc = reinterpret_cast<ucontext_t*>(context);
    mcontext_t* w15mc = reinterpret_cast<mcontext_t*>(&w15uc->uc_mcontext);
    const uintptr_t w15regs[11] = {
        w15mc->arm_r0, w15mc->arm_r1, w15mc->arm_r2, w15mc->arm_r3,
        w15mc->arm_r4, w15mc->arm_r5, w15mc->arm_r6, w15mc->arm_r7,
        w15mc->arm_r8, w15mc->arm_r9, w15mc->arm_r10 };
    char rb[320];
    int rn = snprintf(rb, sizeof(rb),
        "[W15-PROBE] regs r0=%zx r1=%zx r4=%zx r5=%zx r6=%zx r7=%zx r9=%zx r10=%zx\n",
        static_cast<size_t>(w15regs[0]), static_cast<size_t>(w15regs[1]),
        static_cast<size_t>(w15regs[4]), static_cast<size_t>(w15regs[5]),
        static_cast<size_t>(w15regs[6]), static_cast<size_t>(w15regs[7]),
        static_cast<size_t>(w15regs[9]), static_cast<size_t>(w15regs[10]));
    W15Write(rb, rn);
    for (int r = 0; r < 11; ++r) {
      uintptr_t v = w15regs[r];
      if (v == 0u || !IsAligned<4u>(v)) continue;
      uint32_t kref = 0;  // obj->klass_ is a HeapReference<Class> at offset 0 (32-bit)
      if (SafeCopy(&kref, reinterpret_cast<const void*>(v), sizeof(kref)) !=
          static_cast<ssize_t>(sizeof(kref))) {
        continue;
      }
      if (kref < 0x10000000u) continue;  // not a plausible class pointer
      uint32_t probe = 0;  // confirm the class object is mapped
      if (SafeCopy(&probe, reinterpret_cast<const void*>(static_cast<uintptr_t>(kref)),
                   sizeof(probe)) != static_cast<ssize_t>(sizeof(probe))) {
        continue;
      }
      mirror::Object* o = reinterpret_cast<mirror::Object*>(v);
      mirror::Class* c = o->GetClass<kVerifyNone, kWithoutReadBarrier>();
      if (c == nullptr) continue;
      std::string w15st;
      const char* d = c->GetDescriptor(&w15st);
      char cb[320];
      int cn = snprintf(cb, sizeof(cb), "[W15-PROBE] r%d=%zx class=%s\n",
                        r, static_cast<size_t>(v), (d != nullptr) ? d : "?");
      W15Write(cb, cn);
      // Log the raw value at offset 8 (the field the crash deref'd) — null confirms the
      // missing base/field on this object without needing the heavy ArtField headers.
      uint32_t fv8 = 0;
      if (SafeCopy(&fv8, reinterpret_cast<const void*>(v + 8u), sizeof(fv8)) ==
          static_cast<ssize_t>(sizeof(fv8))) {
        char fb[160];
        int fnn = snprintf(fb, sizeof(fb), "[W15-PROBE]   r%d field@8 val=%zx\n",
                           r, static_cast<size_t>(fv8));
        W15Write(fb, fnn);
      }
    }
    // r0 is the ArtMethod* in ART's quick ABI — name the crashing method itself.
    {
      uintptr_t v0 = w15regs[0];
      if (v0 != 0u && IsAligned<4u>(v0)) {
        ArtMethod* m = reinterpret_cast<ArtMethod*>(v0);
        const DexFile* df = m->GetDexFile();
        if (df != nullptr) {
          uint32_t midx = m->GetDexMethodIndex();
          if (midx < df->NumMethodIds()) {
            const dex::MethodId& mid = df->GetMethodId(midx);
            const char* mc = df->GetMethodDeclaringClassDescriptor(mid);
            const char* mn = df->GetMethodName(mid);
            char mb[320];
            int mnn = snprintf(mb, sizeof(mb), "[W15-PROBE] r0-as-method = %s->%s\n",
                               (mc != nullptr) ? mc : "?", (mn != nullptr) ? mn : "?");
            W15Write(mb, mnn);
          }
        }
      }
    }
  }
#endif

#if defined(__arm__)
  // === [W21-PROBE] Boot-image entrypoint-corruption scan. The crash is a boot->boot
  // direct call (ArrayMap.get -> indexOf) landing at a WRONG code address inside another
  // boot method (handleBindApplication+0xd7c). The static image entrypoint is correct, so
  // some runtime write overwrote a boot ArtMethod's entry_point_from_quick_compiled_code_.
  // Here, at the fault, scan all boot-image ArtMethods and report every one whose quick
  // entrypoint lands INSIDE the faulting method's code region (mid-method = corrupt), with
  // its name + dex method index. Pattern across hits pinpoints the corruption source.
  // Guarded: only run when the fault PC is inside a boot-image oat's executable code.
  {
    ucontext_t* w21uc = reinterpret_cast<ucontext_t*>(context);
    mcontext_t* w21mc = reinterpret_cast<mcontext_t*>(&w21uc->uc_mcontext);
    uintptr_t fault_pc = w21mc->arm_pc;
    uintptr_t lr = w21mc->arm_lr;
    Runtime* w21rt = Runtime::Current();
    if (w21rt != nullptr && w21rt->GetHeap() != nullptr) {
      const std::vector<gc::space::ImageSpace*>& w21spaces =
          w21rt->GetHeap()->GetBootImageSpaces();
      char hb[220];
      int hn = snprintf(hb, sizeof(hb),
          "[W21-PROBE] fault_pc=0x%zx lr=0x%zx bootspaces=%zu\n",
          static_cast<size_t>(fault_pc), static_cast<size_t>(lr), w21spaces.size());
      W15Write(hb, hn);
      // Find the boot image space whose OAT contains fault_pc.
      for (gc::space::ImageSpace* sp : w21spaces) {
        if (sp == nullptr) continue;
        const ImageHeader& ih = sp->GetImageHeader();
        uintptr_t oat_begin = reinterpret_cast<uintptr_t>(ih.GetOatFileBegin());
        uintptr_t oat_end = reinterpret_cast<uintptr_t>(ih.GetOatFileEnd());
        if (!(fault_pc >= oat_begin && fault_pc < oat_end)) continue;
        uintptr_t img_begin = reinterpret_cast<uintptr_t>(sp->Begin());
        // ArrayMap.get's 'add r0,pc' for the indexOf call is at oat file offset 0x1e3b310
        // (const movw#0xc984/movt#0xfc86). The runtime addr of an oat byte at file off F is
        // oat_begin + F. Reconstruct r0 = (pc_of_add + 4) + const, then read [r0] (klass) and
        // [r0+0x14] (entry_point_from_quick_compiled_code_). This tells us EXACTLY what the
        // blx jumped through, without walking the (LengthPrefixed) methods section.
        char ob[220];
        int on = snprintf(ob, sizeof(ob),
            "[W21-PROBE] match space img_begin=0x%zx oat_begin=0x%zx oat_end=0x%zx\n",
            static_cast<size_t>(img_begin), static_cast<size_t>(oat_begin),
            static_cast<size_t>(oat_end));
        W15Write(ob, on);
        // file offset of the 'add r0,pc' (4th call) inside ArrayMap.get == 0x1e3b310
        const uintptr_t kAddFileOff = 0x1e3b310;
        const uint32_t kConst = 0xfc86c984u;  // movt:movw combined
        uintptr_t add_pc = oat_begin + kAddFileOff;  // Thumb instr addr
        uintptr_t r0 = (add_pc + 4 + kConst) & 0xffffffffu;
        char rb[220];
        int rn = snprintf(rb, sizeof(rb),
            "[W21-PROBE] reconstructed indexOf-ArtMethod r0=0x%zx (add_pc=0x%zx)\n",
            static_cast<size_t>(r0), static_cast<size_t>(add_pc));
        W15Write(rb, rn);
        // Read klass (offset 0) and entrypoint (offset 0x14) via SafeCopy.
        uint32_t klassref = 0, epval = 0, accflags = 0, dmi = 0;
        bool ok_k = SafeCopy(&klassref, reinterpret_cast<const void*>(r0), 4) == 4;
        bool ok_a = SafeCopy(&accflags, reinterpret_cast<const void*>(r0 + 4), 4) == 4;
        bool ok_d = SafeCopy(&dmi, reinterpret_cast<const void*>(r0 + 8), 4) == 4;
        bool ok_e = SafeCopy(&epval, reinterpret_cast<const void*>(r0 + 0x14), 4) == 4;
        char db[300];
        int dn = snprintf(db, sizeof(db),
            "[W21-PROBE] r0 fields: klass=0x%x(ok%d) accflags=0x%x(ok%d) dmi=0x%x(ok%d) EP@0x14=0x%x(ok%d)\n",
            klassref, ok_k, accflags, ok_a, dmi, ok_d, epval, ok_e);
        W15Write(db, dn);
        // Decode: is EP inside oat code? what file-offset does it map to?
        if (ok_e) {
          uintptr_t epv = epval & ~1u;
          char fb[260];
          if (epv >= oat_begin && epv < oat_end) {
            int fn = snprintf(fb, sizeof(fb),
                "[W21-PROBE] EP maps to oat file-offset 0x%zx (indexOf SHOULD be 0x1e3b420; "
                "handleBindApp+0xd7c is 0x%zx)\n",
                static_cast<size_t>(epv - oat_begin),
                static_cast<size_t>((fault_pc & ~1u) - oat_begin));
            W15Write(fb, fn);
          } else {
            int fn = snprintf(fb, sizeof(fb),
                "[W21-PROBE] EP 0x%x is OUTSIDE this oat [0x%zx,0x%zx)\n",
                epval, static_cast<size_t>(oat_begin), static_cast<size_t>(oat_end));
            W15Write(fb, fn);
          }
        }
        break;
      }
    }
  }
#endif

#ifdef TEST_NESTED_SIGNAL
  // Simulate a crash in a handler.
  raise(SIGSEGV);
#endif

  if (IsInGeneratedCode(info, context)) {
    VLOG(signals) << "in generated code, looking for handler";
    for (const auto& handler : generated_code_handlers_) {
      VLOG(signals) << "invoking Action on handler " << handler;
      if (handler->Action(sig, info, context)) {
        // We have handled a signal so it's time to return from the
        // signal handler to the appropriate place.
        return true;
      }
    }
  }

  // === [W15-PROBE] Name the faulting method on an UNHANDLED SIGSEGV. The current-frame
  // ArtMethod* at sp[0] may be clobbered (observed sp[0]==0 -> NullPointerHandler rejects
  // -> fatal), so scan up-stack for the first valid ArtMethods. SafeCopy reads stack +
  // candidate fields without nested faults; write(2) emits unbuffered; no-alloc dex
  // const-char* accessors (NOT PrettyMethod, which mallocs and could deadlock) name it.
  {
    uintptr_t fsp = GetFaultSp(context);
    if (fsp != 0u) {
      int found = 0;
      // Widened to 8192 slots (32KB): the crashing method may have a large frame and
      // faulted in its prologue before storing its own ArtMethod at sp[0], so the
      // nearest recoverable (caller) ArtMethod can be far up-stack.
      for (int i = 0; i < 8192 && found < 8; ++i) {
        uintptr_t v = 0;
        if (SafeCopy(&v, reinterpret_cast<const void*>(fsp + i * sizeof(uintptr_t)),
                     sizeof(v)) != static_cast<ssize_t>(sizeof(v))) {
          break;  // ran off the mapped stack
        }
        // ArtMethods for boot/framework code live in the boot image near
        // ART_BASE_ADDRESS (0x70000000; observed r0=0x710a7ec8, oat base 0x72c76000).
        // Range-filter to reject stack garbage that merely looks aligned, so the deeper
        // dex-chain deref below cannot fault (the prior scan faulted on a false positive).
        if (v < 0x70000000u || v >= 0x73000000u || !IsAligned<sizeof(void*)>(v)) continue;
        // declaring_class_ is ArtMethod's first field; read+validate via SafeCopy.
        uint32_t dc = 0;
        if (SafeCopy(&dc, reinterpret_cast<const void*>(v), sizeof(dc)) !=
            static_cast<ssize_t>(sizeof(dc))) {
          continue;
        }
        if (dc < 0x70000000u || dc >= 0x73000000u) continue;  // declaring class in boot image
        uint32_t cw = 0;  // confirm the class object itself is mapped before trusting it
        if (SafeCopy(&cw, reinterpret_cast<const void*>(static_cast<uintptr_t>(dc)),
                     sizeof(cw)) != static_cast<ssize_t>(sizeof(cw))) {
          continue;
        }
        ArtMethod* m = reinterpret_cast<ArtMethod*>(v);
        const char* cls = "?";
        const char* nm = "?";
        const DexFile* df = m->GetDexFile();
        if (df != nullptr) {
          uint32_t midx = m->GetDexMethodIndex();
          if (midx < df->NumMethodIds()) {
            const dex::MethodId& mid = df->GetMethodId(midx);
            cls = df->GetMethodDeclaringClassDescriptor(mid);
            nm = df->GetMethodName(mid);
          }
        }
        char b[768];
        int n = snprintf(b, sizeof(b), "[W15-PROBE] sp[+%d]=%p %s->%s\n",
                         i, reinterpret_cast<void*>(v), (cls != nullptr) ? cls : "?",
                         (nm != nullptr) ? nm : "?");
        W15Write(b, n);
        found++;
      }
      if (found == 0) {
        const char* msg = "[W15-PROBE] no valid ArtMethod in sp[0..8191]\n";
        W15Write(msg, static_cast<int>(strlen(msg)));
      }
    }
  }

  // === [W15-PROBE] Dump the MANAGED (interpreted) call chain via the ShadowFrame links.
  // App code runs in the switch interpreter (nterp/JIT gated off for app classloaders),
  // so its managed frames are ShadowFrames threaded off the heap, NOT native-stack frames.
  // If this chain is populated while the native-stack scan above found nothing, it proves
  // the managed call stack is off the native stack (why ART can't synthesize a catchable
  // NPE for a compiled-framework null-deref reached from interpreted code) AND names the
  // app methods that led into the crash.
  {
    Thread* w15s = Thread::Current();
    int depth = 0;
    if (w15s != nullptr) {
      for (const ManagedStack* ms = w15s->GetManagedStack();
           ms != nullptr && depth < 48; ms = ms->GetLink()) {
        for (ShadowFrame* sf = ms->GetTopShadowFrame();
             sf != nullptr && depth < 48; sf = sf->GetLink()) {
          if (!IsAligned<sizeof(void*)>(reinterpret_cast<uintptr_t>(sf))) break;
          ArtMethod* m = sf->GetMethod();
          const char* cls = "?";
          const char* nm = "?";
          if (m != nullptr) {
            const DexFile* df = m->GetDexFile();
            if (df != nullptr) {
              uint32_t midx = m->GetDexMethodIndex();
              if (midx < df->NumMethodIds()) {
                const dex::MethodId& mid = df->GetMethodId(midx);
                cls = df->GetMethodDeclaringClassDescriptor(mid);
                nm = df->GetMethodName(mid);
              }
            }
          }
          char b[768];
          int n = snprintf(b, sizeof(b), "[W15-PROBE] shadow[%d] %s->%s\n",
                           depth, (cls != nullptr) ? cls : "?", (nm != nullptr) ? nm : "?");
          W15Write(b, n);
          depth++;
        }
      }
    }
    if (depth == 0) {
      const char* msg = "[W15-PROBE] NO shadow frames (managed stack empty)\n";
      W15Write(msg, static_cast<int>(strlen(msg)));
    }
  }

  // We hit a signal we didn't handle.  This might be something for which
  // we can give more information about so call all registered handlers to
  // see if it is.
  if (HandleFaultByOtherHandlers(sig, info, context)) {
    return true;
  }

  // Set a breakpoint in this function to catch unhandled signals.
  art_sigsegv_fault();
  return false;
}

void FaultManager::AddHandler(FaultHandler* handler, bool generated_code) {
  DCHECK(initialized_);
  if (generated_code) {
    generated_code_handlers_.push_back(handler);
  } else {
    other_handlers_.push_back(handler);
  }
}

void FaultManager::RemoveHandler(FaultHandler* handler) {
  auto it = std::find(generated_code_handlers_.begin(), generated_code_handlers_.end(), handler);
  if (it != generated_code_handlers_.end()) {
    generated_code_handlers_.erase(it);
    return;
  }
  auto it2 = std::find(other_handlers_.begin(), other_handlers_.end(), handler);
  if (it2 != other_handlers_.end()) {
    other_handlers_.erase(it2);
    return;
  }
  LOG(FATAL) << "Attempted to remove non existent handler " << handler;
}

inline FaultManager::GeneratedCodeRange* FaultManager::CreateGeneratedCodeRange(
    const void* start, size_t size) {
  GeneratedCodeRange* range = free_generated_code_ranges_;
  if (range != nullptr) {
    std::less<GeneratedCodeRange*> less;
    DCHECK(!less(range, generated_code_ranges_storage_));
    DCHECK(less(range, generated_code_ranges_storage_ + kNumLocalGeneratedCodeRanges));
    range->start = start;
    range->size = size;
    free_generated_code_ranges_ = range->next.load(std::memory_order_relaxed);
    range->next.store(nullptr, std::memory_order_relaxed);
    return range;
  } else {
    return new GeneratedCodeRange{nullptr, start, size};
  }
}

inline void FaultManager::FreeGeneratedCodeRange(GeneratedCodeRange* range) {
  std::less<GeneratedCodeRange*> less;
  if (!less(range, generated_code_ranges_storage_) &&
      less(range, generated_code_ranges_storage_ + kNumLocalGeneratedCodeRanges)) {
    MutexLock lock(Thread::Current(), generated_code_ranges_lock_);
    range->start = nullptr;
    range->size = 0u;
    range->next.store(free_generated_code_ranges_, std::memory_order_relaxed);
    free_generated_code_ranges_ = range;
  } else {
    // Range is not in the `generated_code_ranges_storage_`.
    delete range;
  }
}

void FaultManager::AddGeneratedCodeRange(const void* start, size_t size) {
  GeneratedCodeRange* new_range = nullptr;
  {
    MutexLock lock(Thread::Current(), generated_code_ranges_lock_);
    new_range = CreateGeneratedCodeRange(start, size);
    GeneratedCodeRange* old_head = generated_code_ranges_.load(std::memory_order_relaxed);
    new_range->next.store(old_head, std::memory_order_relaxed);
    generated_code_ranges_.store(new_range, std::memory_order_release);
  }

  // The above release operation on `generated_code_ranges_` with an acquire operation
  // on the same atomic object in `IsInGeneratedCode()` ensures the correct memory
  // visibility for the contents of `*new_range` for any thread that loads the value
  // written above (or a value written by a release sequence headed by that write).
  //
  // However, we also need to ensure that any thread that encounters a segmentation
  // fault in the provided range shall actually see the written value. For JIT code
  // cache and nterp, the registration happens while the process is single-threaded
  // but the synchronization is more complicated for code in oat files.
  //
  // Threads that load classes register dex files under the `Locks::dex_lock_` and
  // the first one to register a dex file with a given oat file shall add the oat
  // code range; the memory visibility for these threads is guaranteed by the lock.
  // However a thread that did not try to load a class with oat code can execute the
  // code if a direct or indirect reference to such class escapes from one of the
  // threads that loaded it. Use `membarrier()` for memory visibility in this case.
  art::membarrier(MembarrierCommand::kPrivateExpedited);
}

void FaultManager::RemoveGeneratedCodeRange(const void* start, size_t size) {
  Thread* self = Thread::Current();
  GeneratedCodeRange* range = nullptr;
  {
    MutexLock lock(self, generated_code_ranges_lock_);
    std::atomic<GeneratedCodeRange*>* before = &generated_code_ranges_;
    range = before->load(std::memory_order_relaxed);
    while (range != nullptr && range->start != start) {
      before = &range->next;
      range = before->load(std::memory_order_relaxed);
    }
    if (range != nullptr) {
      GeneratedCodeRange* next = range->next.load(std::memory_order_relaxed);
      if (before == &generated_code_ranges_) {
        // Relaxed store directly to `generated_code_ranges_` would not satisfy
        // conditions for a release sequence, so we need to use store-release.
        before->store(next, std::memory_order_release);
      } else {
        // In the middle of the list, we can use a relaxed store as we're not
        // publishing any newly written memory to potential reader threads.
        // Whether they see the removed node or not is unimportant as we should
        // not execute that code anymore. We're keeping the `next` link of the
        // removed node, so that concurrent walk can use it to reach remaining
        // retained nodes, if any.
        before->store(next, std::memory_order_relaxed);
      }
    }
  }
  CHECK(range != nullptr);
  DCHECK_EQ(range->start, start);
  CHECK_EQ(range->size, size);

  Runtime* runtime = Runtime::Current();
  CHECK(runtime != nullptr);
  if (runtime->IsStarted() && runtime->GetThreadList() != nullptr) {
    // Run a checkpoint before deleting the range to ensure that no thread holds a
    // pointer to the removed range while walking the list in `IsInGeneratedCode()`.
    // That walk is guarded by checking that the thread is `Runnable`, so any walk
    // started before the removal shall be done when running the checkpoint and the
    // checkpoint also ensures the correct memory visibility of `next` links,
    // so the thread shall not see the pointer during future walks.

    // This function is currently called in different mutex and thread states.
    // Semi-space GC performs the cleanup during its `MarkingPhase()` while holding
    // the mutator exclusively, so we do not need a checkpoint. All other GCs perform
    // the cleanup in their `ReclaimPhase()` while holding the mutator lock as shared
    // and it's safe to release and re-acquire the mutator lock. Despite holding the
    // mutator lock as shared, the thread is not always marked as `Runnable`.
    // TODO: Clean up state transitions in different GC implementations. b/259440389
    if (Locks::mutator_lock_->IsExclusiveHeld(self)) {
      // We do not need a checkpoint because no other thread is Runnable.
    } else {
      DCHECK(Locks::mutator_lock_->IsSharedHeld(self));
      // Use explicit state transitions or unlock/lock.
      bool runnable = (self->GetState() == ThreadState::kRunnable);
      if (runnable) {
        self->TransitionFromRunnableToSuspended(ThreadState::kNative);
      } else {
        Locks::mutator_lock_->SharedUnlock(self);
      }
      DCHECK(!Locks::mutator_lock_->IsSharedHeld(self));
      runtime->GetThreadList()->RunEmptyCheckpoint();
      if (runnable) {
        self->TransitionFromSuspendedToRunnable();
      } else {
        Locks::mutator_lock_->SharedLock(self);
      }
    }
  }
  FreeGeneratedCodeRange(range);
}

// This function is called within the signal handler. It checks that the thread
// is `Runnable`, the `mutator_lock_` is held (shared) and the fault PC is in one
// of the registered generated code ranges. No annotalysis is done.
bool FaultManager::IsInGeneratedCode(siginfo_t* siginfo, void* context) {
  // We can only be running Java code in the current thread if it
  // is in Runnable state.
  VLOG(signals) << "Checking for generated code";
  Thread* thread = Thread::Current();
  if (thread == nullptr) {
    VLOG(signals) << "no current thread";
    return false;
  }

  ThreadState state = thread->GetState();
  if (state != ThreadState::kRunnable) {
    VLOG(signals) << "not runnable";
    return false;
  }

  // Current thread is runnable.
  // Make sure it has the mutator lock.
  if (!Locks::mutator_lock_->IsSharedHeld(thread)) {
    VLOG(signals) << "no lock";
    return false;
  }

  uintptr_t fault_pc = GetFaultPc(siginfo, context);
  if (fault_pc == 0u) {
    VLOG(signals) << "no fault PC";
    return false;
  }

  // Walk over the list of registered code ranges.
  GeneratedCodeRange* range = generated_code_ranges_.load(std::memory_order_acquire);
  while (range != nullptr) {
    if (fault_pc - reinterpret_cast<uintptr_t>(range->start) < range->size) {
      return true;
    }
    // We may or may not see ranges that were concurrently removed, depending
    // on when the relaxed writes of the `next` links become visible. However,
    // even if we're currently at a node that is being removed, we shall visit
    // all remaining ranges that are not being removed as the removed nodes
    // retain the `next` link at the time of removal (which may lead to other
    // removed nodes before reaching remaining retained nodes, if any). Correct
    // memory visibility of `start` and `size` fields of the visited ranges is
    // ensured by the release and acquire operations on `generated_code_ranges_`.
    range = range->next.load(std::memory_order_relaxed);
  }
  return false;
}

FaultHandler::FaultHandler(FaultManager* manager) : manager_(manager) {
}

//
// Null pointer fault handler
//
NullPointerHandler::NullPointerHandler(FaultManager* manager) : FaultHandler(manager) {
  manager_->AddHandler(this, true);
}

bool NullPointerHandler::IsValidMethod(ArtMethod* method) {
  // At this point we know that the thread is `Runnable` and the PC is in one of
  // the registered code ranges. The `method` was read from the top of the stack
  // and should really point to an actual `ArtMethod`, unless we're crashing during
  // prologue or epilogue, or somehow managed to jump to the compiled code by some
  // unexpected path, other than method invoke or exception delivery. We do a few
  // quick checks without guarding from another fault.
  VLOG(signals) << "potential method: " << method;

  static_assert(IsAligned<sizeof(void*)>(ArtMethod::Size(kRuntimePointerSize)));
  if (method == nullptr || !IsAligned<sizeof(void*)>(method)) {
    VLOG(signals) << ((method == nullptr) ? "null method" : "unaligned method");
    return false;
  }

  // Check that the presumed method actually points to a class. Read barriers
  // are not needed (and would be undesirable in a signal handler) when reading
  // a chain of constant references to get to a non-movable `Class.class` object.

  // Note: Allowing nested faults. Checking that the method is in one of the
  // `LinearAlloc` spaces, or that objects we look at are in the `Heap` would be
  // slow and require locking a mutex, which is undesirable in a signal handler.
  // (Though we could register valid ranges similarly to the generated code ranges.)

  mirror::Object* klass =
      method->GetDeclaringClassAddressWithoutBarrier()->AsMirrorPtr();
  if (klass == nullptr || !IsAligned<kObjectAlignment>(klass)) {
    VLOG(signals) << ((klass == nullptr) ? "null class" : "unaligned class");
    return false;
  }

  mirror::Class* class_class = klass->GetClass<kVerifyNone, kWithoutReadBarrier>();
  if (class_class == nullptr || !IsAligned<kObjectAlignment>(class_class)) {
    VLOG(signals) << ((klass == nullptr) ? "null class_class" : "unaligned class_class");
    return false;
  }

  if (class_class != class_class->GetClass<kVerifyNone, kWithoutReadBarrier>()) {
    VLOG(signals) << "invalid class_class";
    return false;
  }

  return true;
}

bool NullPointerHandler::IsValidReturnPc(ArtMethod** sp, uintptr_t return_pc) {
  // Check if we can associate a dex PC with the return PC, whether from Nterp,
  // or with an existing stack map entry for a compiled method.
  // Note: Allowing nested faults if `IsValidMethod()` returned a false positive.
  // Note: The `ArtMethod::GetOatQuickMethodHeader()` can acquire locks (at least
  // `Locks::jit_lock_`) and if the thread already held such a lock, the signal
  // handler would deadlock. However, if a thread is holding one of the locks
  // below the mutator lock, the PC should be somewhere in ART code and should
  // not match any registered generated code range, so such as a deadlock is
  // unlikely. If it happens anyway, the worst case is that an internal ART crash
  // would be reported as ANR.
  ArtMethod* method = *sp;
  const OatQuickMethodHeader* method_header = method->GetOatQuickMethodHeader(return_pc);
  if (method_header == nullptr) {
    VLOG(signals) << "No method header.";
    return false;
  }
  VLOG(signals) << "looking for dex pc for return pc 0x" << std::hex << return_pc
                << " pc offset: 0x" << std::hex
                << (return_pc - reinterpret_cast<uintptr_t>(method_header->GetEntryPoint()));
  uint32_t dexpc = method_header->ToDexPc(reinterpret_cast<ArtMethod**>(sp), return_pc, false);
  VLOG(signals) << "dexpc: " << dexpc;
  return dexpc != dex::kDexNoIndex;
}

//
// Suspension fault handler
//
SuspensionHandler::SuspensionHandler(FaultManager* manager) : FaultHandler(manager) {
  manager_->AddHandler(this, true);
}

//
// Stack overflow fault handler
//
StackOverflowHandler::StackOverflowHandler(FaultManager* manager) : FaultHandler(manager) {
  manager_->AddHandler(this, true);
}

//
// Stack trace handler, used to help get a stack trace from SIGSEGV inside of compiled code.
//
JavaStackTraceHandler::JavaStackTraceHandler(FaultManager* manager) : FaultHandler(manager) {
  manager_->AddHandler(this, false);
}

bool JavaStackTraceHandler::Action(int sig ATTRIBUTE_UNUSED, siginfo_t* siginfo, void* context) {
  // Make sure that we are in the generated code, but we may not have a dex pc.
  bool in_generated_code = manager_->IsInGeneratedCode(siginfo, context);
  if (in_generated_code) {
    LOG(ERROR) << "Dumping java stack trace for crash in generated code";
    Thread* self = Thread::Current();

    uintptr_t sp = FaultManager::GetFaultSp(context);
    CHECK_NE(sp, 0u);  // Otherwise we should not have reached this handler.
    // Inside of generated code, sp[0] is the method, so sp is the frame.
    self->SetTopOfStack(reinterpret_cast<ArtMethod**>(sp));
    self->DumpJavaStack(LOG_STREAM(ERROR));
  }

  return false;  // Return false since we want to propagate the fault to the main signal handler.
}

}   // namespace art
