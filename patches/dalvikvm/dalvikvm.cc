/*
 * copyright (C) 2011 The Android Open Source Project
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

#if !defined(__MUSL__)
#include <execinfo.h>
#endif
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#if !defined(__MUSL__)
#include <ucontext.h>
#endif
#include <algorithm>
#include <memory>

#include "base/fast_exit.h"
#include "jni.h"
#include "nativehelper/JniInvocation.h"
#include "nativehelper/ScopedLocalRef.h"
#include "nativehelper/toStringArray.h"

// ART internal headers for forcing class initialization status
#include "runtime.h"
#include "class_linker.h"
#include "mirror/class-inl.h"
#include "handle_scope-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "object_lock.h"
#include "entrypoints/runtime_asm_entrypoints.h"

// Throwable.printStackTrace() no-op — prevents infinite loop when JNI
// ExceptionDescribe calls printStackTrace which triggers more exceptions.
extern "C" JNIEXPORT void JNICALL
Java_java_lang_Throwable_printStackTrace_noop(JNIEnv*, jobject) {
  // Intentionally empty — prevents cascading exception handling loops
}

// Generic no-op that returns null (for Object-returning methods patched to native)
extern "C" JNIEXPORT jobject JNICALL
Java_noop_return_null(JNIEnv*, jobject) {
  return nullptr;
}

// Generic no-op that returns true (for boolean-returning methods)
extern "C" JNIEXPORT jboolean JNICALL
Java_noop_return_true(JNIEnv*, jobject) {
  return JNI_TRUE;
}

// Generic no-op that returns false
extern "C" JNIEXPORT jboolean JNICALL
Java_noop_return_false(JNIEnv*, jobject) {
  return JNI_FALSE;
}

// Static no-op for void-returning methods.
extern "C" JNIEXPORT void JNICALL
Java_noop_return_void_static(JNIEnv*, jclass) {
}

static void PrintThrowableRaw(JNIEnv* env, jthrowable t) {
  if (t == nullptr) {
    fprintf(stderr, "[dalvikvm] raw exception: null\n");
    return;
  }
  jclass throwable_class = env->FindClass("java/lang/Throwable");
  if (env->ExceptionCheck()) env->ExceptionClear();
  jclass class_class = env->FindClass("java/lang/Class");
  if (env->ExceptionCheck()) env->ExceptionClear();
  jmethodID get_name = class_class != nullptr
      ? env->GetMethodID(class_class, "getNameNative", "()Ljava/lang/String;")
      : nullptr;
  if (env->ExceptionCheck()) { env->ExceptionClear(); get_name = nullptr; }
  jfieldID message_field = throwable_class != nullptr
      ? env->GetFieldID(throwable_class, "detailMessage", "Ljava/lang/String;")
      : nullptr;
  if (env->ExceptionCheck()) { env->ExceptionClear(); message_field = nullptr; }

  jclass exc_class = env->GetObjectClass(t);
  if (env->ExceptionCheck()) env->ExceptionClear();
  jstring name_j = (get_name != nullptr && exc_class != nullptr)
      ? reinterpret_cast<jstring>(env->CallObjectMethod(exc_class, get_name))
      : nullptr;
  if (env->ExceptionCheck()) { env->ExceptionClear(); name_j = nullptr; }
  const char* name = name_j != nullptr ? env->GetStringUTFChars(name_j, nullptr) : nullptr;

  jstring message_j = message_field != nullptr
      ? reinterpret_cast<jstring>(env->GetObjectField(t, message_field))
      : nullptr;
  if (env->ExceptionCheck()) { env->ExceptionClear(); message_j = nullptr; }
  const char* message = message_j != nullptr ? env->GetStringUTFChars(message_j, nullptr) : nullptr;

  fprintf(stderr, "[dalvikvm] raw exception: %s: %s\n",
          name != nullptr ? name : "<unknown>",
          message != nullptr ? message : "<no message>");

  if (message_j != nullptr && message != nullptr) {
    env->ReleaseStringUTFChars(message_j, message);
  }
  if (name_j != nullptr && name != nullptr) {
    env->ReleaseStringUTFChars(name_j, name);
  }
}

struct WestlakeUnsafeBinding {
  jclass cls;
  const char* desc;
  bool sun_style_object_field_offset;
};

static WestlakeUnsafeBinding ResolveWestlakeUnsafeBinding(JNIEnv* env) {
  WestlakeUnsafeBinding binding = {nullptr, nullptr, false};
  jclass unsafe_cls = env->FindClass("sun/misc/Unsafe");
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (unsafe_cls != nullptr) {
    binding.cls = unsafe_cls;
    binding.desc = "Lsun/misc/Unsafe;";
    binding.sun_style_object_field_offset = true;
    return binding;
  }

  unsafe_cls = env->FindClass("jdk/internal/misc/Unsafe");
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (unsafe_cls != nullptr) {
    binding.cls = unsafe_cls;
    binding.desc = "Ljdk/internal/misc/Unsafe;";
  }
  return binding;
}

static void SetConcurrentHashMapOffset(JNIEnv* env,
                                       jclass chm_cls,
                                       const char* static_name,
                                       jlong offset) {
  jfieldID static_field = env->GetStaticFieldID(chm_cls, static_name, "J");
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return;
  }
  if (static_field != nullptr) {
    env->SetStaticLongField(chm_cls, static_field, offset);
  }
}

static void PatchConcurrentHashMapUnsafe(JNIEnv* env, const char* phase_label) {
  jclass chm_cls = env->FindClass("java/util/concurrent/ConcurrentHashMap");
  if (env->ExceptionCheck()) env->ExceptionClear();
  WestlakeUnsafeBinding unsafe = ResolveWestlakeUnsafeBinding(env);
  if (chm_cls == nullptr || unsafe.cls == nullptr) {
    fprintf(stderr, "[dalvikvm] ConcurrentHashMap fixup skipped (%s): missing classes\n",
            phase_label);
    return;
  }

  jfieldID u_field = env->GetStaticFieldID(chm_cls, "U", unsafe.desc);
  if (env->ExceptionCheck()) env->ExceptionClear();
  jfieldID the_unsafe_field = env->GetStaticFieldID(unsafe.cls, "theUnsafe", unsafe.desc);
  if (env->ExceptionCheck()) env->ExceptionClear();

  jobject unsafe_obj = the_unsafe_field != nullptr
      ? env->GetStaticObjectField(unsafe.cls, the_unsafe_field)
      : nullptr;
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (unsafe_obj == nullptr) {
    unsafe_obj = env->AllocObject(unsafe.cls);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (unsafe_obj != nullptr && the_unsafe_field != nullptr) {
      env->SetStaticObjectField(unsafe.cls, the_unsafe_field, unsafe_obj);
      if (env->ExceptionCheck()) env->ExceptionClear();
      fprintf(stderr, "[dalvikvm] Created Unsafe.theUnsafe via AllocObject (%s)\n", phase_label);
    }
  }

  if (u_field != nullptr && unsafe_obj != nullptr) {
    env->SetStaticObjectField(chm_cls, u_field, unsafe_obj);
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr, "[dalvikvm] Set ConcurrentHashMap.U (%s)\n", phase_label);
  }

  if (unsafe_obj == nullptr) {
    return;
  }

  const char* instance_names[] = {"sizeCtl", "transferIndex", "baseCount", "cellsBusy"};
  const char* static_names[] = {"SIZECTL", "TRANSFERINDEX", "BASECOUNT", "CELLSBUSY"};
  if (unsafe.sun_style_object_field_offset) {
    jmethodID object_field_offset = env->GetMethodID(
        unsafe.cls, "objectFieldOffset", "(Ljava/lang/reflect/Field;)J");
    if (env->ExceptionCheck()) env->ExceptionClear();
    jclass class_cls = env->FindClass("java/lang/Class");
    if (env->ExceptionCheck()) env->ExceptionClear();
    jmethodID get_declared_field = class_cls != nullptr
        ? env->GetMethodID(class_cls,
                           "getDeclaredField",
                           "(Ljava/lang/String;)Ljava/lang/reflect/Field;")
        : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (object_field_offset != nullptr && get_declared_field != nullptr) {
      for (size_t i = 0; i < 4; ++i) {
        jstring name = env->NewStringUTF(instance_names[i]);
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          continue;
        }
        jobject reflected_field = env->CallObjectMethod(chm_cls, get_declared_field, name);
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          continue;
        }
        jlong offset = env->CallLongMethod(unsafe_obj, object_field_offset, reflected_field);
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          continue;
        }
        SetConcurrentHashMapOffset(env, chm_cls, static_names[i], offset);
      }
      fprintf(stderr, "[dalvikvm] Set ConcurrentHashMap field offsets via sun.misc.Unsafe (%s)\n",
              phase_label);
    }
  } else {
    jmethodID object_field_offset = env->GetMethodID(
        unsafe.cls, "objectFieldOffset", "(Ljava/lang/Class;Ljava/lang/String;)J");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (object_field_offset != nullptr) {
      for (size_t i = 0; i < 4; ++i) {
        jstring name = env->NewStringUTF(instance_names[i]);
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          continue;
        }
        jlong offset = env->CallLongMethod(unsafe_obj, object_field_offset, chm_cls, name);
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          continue;
        }
        SetConcurrentHashMapOffset(env, chm_cls, static_names[i], offset);
      }
      fprintf(stderr, "[dalvikvm] Set ConcurrentHashMap field offsets via jdk.internal.Unsafe (%s)\n",
              phase_label);
    }
  }

  jmethodID array_base_offset = env->GetMethodID(
      unsafe.cls, "arrayBaseOffset", "(Ljava/lang/Class;)I");
  if (env->ExceptionCheck()) env->ExceptionClear();
  jmethodID array_index_scale = env->GetMethodID(
      unsafe.cls, "arrayIndexScale", "(Ljava/lang/Class;)I");
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (array_base_offset != nullptr && array_index_scale != nullptr) {
    jclass node_array_cls = env->FindClass("[Ljava/util/concurrent/ConcurrentHashMap$Node;");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (node_array_cls == nullptr) {
      node_array_cls = env->FindClass("[Ljava/lang/Object;");
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (node_array_cls != nullptr) {
      jint abase = env->CallIntMethod(unsafe_obj, array_base_offset, node_array_cls);
      if (env->ExceptionCheck()) env->ExceptionClear();
      jint scale = env->CallIntMethod(unsafe_obj, array_index_scale, node_array_cls);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (scale > 0) {
        jint ashift = 0;
        while ((1 << ashift) < scale) {
          ashift++;
        }
        jfieldID abase_field = env->GetStaticFieldID(chm_cls, "ABASE", "I");
        if (env->ExceptionCheck()) env->ExceptionClear();
        jfieldID ashift_field = env->GetStaticFieldID(chm_cls, "ASHIFT", "I");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (abase_field != nullptr) {
          env->SetStaticIntField(chm_cls, abase_field, abase);
        }
        if (ashift_field != nullptr) {
          env->SetStaticIntField(chm_cls, ashift_field, ashift);
        }
        fprintf(stderr, "[dalvikvm] Set ConcurrentHashMap ABASE=%d ASHIFT=%d (%s)\n",
                abase,
                ashift,
                phase_label);
      }
    }
  }
  if (env->ExceptionCheck()) env->ExceptionClear();
}

// Static no-op that returns the second object argument (pass-through)
// For Phrase.d(Activity, Delegate) → returns Delegate unchanged
extern "C" JNIEXPORT jobject JNICALL
Java_noop_return_arg2(JNIEnv*, jclass, jobject, jobject arg2) {
  return arg2;
}

// String.lastIndexOf(int) native replacement — the interpreted version has a
// register corruption bug where length() return clobbers the ch parameter.
// Implementation: pure JNI, no callbacks into Java to avoid re-entrancy issues.
extern "C" JNIEXPORT jint JNICALL
Java_java_lang_String_lastIndexOf_native(JNIEnv* env, jobject thiz, jint ch) {
  fprintf(stderr, "[lastIdx] NATIVE called ch=%d thiz=%p\n", ch, thiz);
  fflush(stderr);
  jint len = env->GetStringLength((jstring)thiz);
  if (ch < 0x10000) {  // BMP character — scan backward through chars
    const jchar* chars = env->GetStringChars((jstring)thiz, nullptr);
    if (!chars) return -1;
    jint result = -1;
    for (jint i = (len < 0 ? 0 : len) - 1; i >= 0; i--) {
      if (chars[i] == (jchar)ch) { result = i; break; }
    }
    env->ReleaseStringChars((jstring)thiz, chars);
    return result;
  }
  // Supplementary character — encode as surrogate pair and search
  jchar hi = (jchar)((ch >> 10) + 0xD7C0);
  jchar lo = (jchar)((ch & 0x3FF) + 0xDC00);
  const jchar* chars = env->GetStringChars((jstring)thiz, nullptr);
  if (!chars) return -1;
  jint result = -1;
  for (jint i = len - 2; i >= 0; i--) {
    if (chars[i] == hi && chars[i+1] == lo) { result = i; break; }
  }
  env->ReleaseStringChars((jstring)thiz, chars);
  return result;
}

// UUID.randomUUID() stub — SecureRandom isn't initialized in standalone mode
// Returns sequential UUIDs to avoid crypto dependency
static std::atomic<uint64_t> uuid_counter{1};
extern "C" JNIEXPORT jobject JNICALL
Java_java_util_UUID_randomUUID_stub(JNIEnv* env, jclass) {
  uint64_t lo = uuid_counter.fetch_add(1);
  uint64_t hi = 0xDEADBEEF00000004ULL; // version 4 UUID marker
  jclass uuidCls = env->FindClass("java/util/UUID");
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (!uuidCls) return nullptr;
  jmethodID ctor = env->GetMethodID(uuidCls, "<init>", "(JJ)V");
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (!ctor) return nullptr;
  jobject uuid = env->NewObject(uuidCls, ctor, (jlong)hi, (jlong)lo);
  if (env->ExceptionCheck()) env->ExceptionClear();
  return uuid;
}

// Charset.defaultCharset() stub — always returns UTF-8
extern "C" JNIEXPORT jobject JNICALL
Java_java_nio_charset_Charset_defaultCharset_stub(JNIEnv* env, jclass) {
  jclass csCls = env->FindClass("java/nio/charset/StandardCharsets");
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (csCls) {
    jfieldID f = env->GetStaticFieldID(csCls, "UTF_8", "Ljava/nio/charset/Charset;");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (f) {
      jobject cs = env->GetStaticObjectField(csCls, f);
      if (cs) return cs;
    }
  }
  // Fallback: find UTF_8 singleton directly
  jclass utf8Cls = env->FindClass("sun/nio/cs/UTF_8");
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (utf8Cls) {
    jfieldID instF = env->GetStaticFieldID(utf8Cls, "INSTANCE", "Lsun/nio/cs/UTF_8;");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (instF) return env->GetStaticObjectField(utf8Cls, instF);
  }
  return nullptr;
}

// SecureRandom.nextBytes(byte[]) stub — fills with pseudo-random bytes
extern "C" JNIEXPORT void JNICALL
Java_java_security_SecureRandom_nextBytes_stub(JNIEnv* env, jobject, jbyteArray arr) {
  if (!arr) return;
  jint len = env->GetArrayLength(arr);
  jbyte* buf = env->GetByteArrayElements(arr, nullptr);
  if (buf) {
    static uint32_t seed = 0x12345678;
    for (int i = 0; i < len; i++) {
      seed = seed * 1103515245 + 12345;
      buf[i] = (jbyte)(seed >> 16);
    }
    env->ReleaseByteArrayElements(arr, buf, 0);
  }
}

// Sysprop stubs from framework_native_stubs.c
extern "C" jobject SocProperties_soc_manufacturer(JNIEnv*, jclass);
extern "C" jobject SocProperties_soc_model(JNIEnv*, jclass);
extern "C" jobject TelephonyProperties_baseband_version(JNIEnv*, jclass);

// TextUtils.formatSimple replacement — pure C, ZERO JNI calls to avoid
// recursive class loading. Only handles %s (from String args) and %d/%x.
extern "C" JNIEXPORT jstring JNICALL
Java_android_text_TextUtils_formatSimple(JNIEnv* env, jclass, jstring fmt, jobjectArray args) {
  if (!fmt) return env->NewStringUTF("");
  const char* fmtStr = env->GetStringUTFChars(fmt, nullptr);
  if (!fmtStr) return fmt;

  int argCount = args ? env->GetArrayLength(args) : 0;
  int argIdx = 0;
  char result[2048];
  int pos = 0;

  for (const char* p = fmtStr; *p && pos < 2040; p++) {
    if (*p == '%' && *(p+1)) {
      const char* spec = p + 1;
      // Skip flags/width/precision
      while (*spec == '-' || *spec == '+' || *spec == '0' || *spec == ' ' ||
             *spec == '#' || (*spec >= '0' && *spec <= '9') || *spec == '.') spec++;
      if (*spec == 's' && argIdx < argCount) {
        // %s — get arg as String directly (avoid toString() which triggers class loading)
        jobject arg = env->GetObjectArrayElement(args, argIdx++);
        if (arg && env->IsInstanceOf(arg, env->FindClass("java/lang/String"))) {
          const char* s = env->GetStringUTFChars((jstring)arg, nullptr);
          if (s) { int n = strlen(s); if (pos+n<2040) { memcpy(result+pos,s,n); pos+=n; } env->ReleaseStringUTFChars((jstring)arg, s); }
        } else {
          // For non-String args, just put "?" — avoids calling toString()
          result[pos++] = '?';
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        p = spec;
      } else if ((*spec == 'd' || *spec == 'x' || *spec == 'X') && argIdx < argCount) {
        // %d/%x — consume the arg but just put "0" (avoid Integer.intValue JNI call)
        argIdx++;
        result[pos++] = '0';
        p = spec;
      } else if (*spec == '%') {
        result[pos++] = '%'; p = spec;
      } else {
        // Unknown specifier — consume arg and skip
        if (argIdx < argCount) argIdx++;
        p = spec;
      }
    } else {
      result[pos++] = *p;
    }
  }
  result[pos] = 0;
  env->ReleaseStringUTFChars(fmt, fmtStr);
  // Clear any stale exceptions from FindClass
  if (env->ExceptionCheck()) env->ExceptionClear();
  return env->NewStringUTF(result);
}

// Thread.clone() override — returns 'this' instead of throwing CloneNotSupportedException.
extern "C" JNIEXPORT jobject JNICALL
Java_java_lang_Thread_clone(JNIEnv* env, jobject self) {
  return self;
}

// Float.toString and Double.toString bypasses — avoid FloatingDecimal which uses
// ThreadLocal and inner class arrays that can't be allocated by the A15 interpreter.
extern "C" JNIEXPORT jstring JNICALL
Java_java_lang_Float_toStringImpl(JNIEnv* env, jclass, jfloat val) {
  char buf[48];
  if (val != val) return env->NewStringUTF("NaN");
  if (val == 1.0f/0.0f) return env->NewStringUTF("Infinity");
  if (val == -1.0f/0.0f) return env->NewStringUTF("-Infinity");
  snprintf(buf, sizeof(buf), "%g", (double)val);
  return env->NewStringUTF(buf);
}
extern "C" JNIEXPORT jstring JNICALL
Java_java_lang_Double_toStringImpl(JNIEnv* env, jclass, jdouble val) {
  char buf[48];
  if (val != val) return env->NewStringUTF("NaN");
  if (val == 1.0/0.0) return env->NewStringUTF("Infinity");
  if (val == -1.0/0.0) return env->NewStringUTF("-Infinity");
  snprintf(buf, sizeof(buf), "%.17g", val);
  return env->NewStringUTF(buf);
}

// ThreadLocal.nextHashCode() bypass — avoid AtomicInteger/VarHandle boot image state.
static std::atomic<jint> g_threadlocal_hash_counter{0};
extern "C" JNIEXPORT jint JNICALL
Java_java_lang_ThreadLocal_nextHashCode_native(JNIEnv*, jclass) {
  static constexpr jint kHashIncrement = static_cast<jint>(0x61c88647u);
  return g_threadlocal_hash_counter.fetch_add(kHashIncrement);
}

extern "C" JNIEXPORT jint JNICALL
Java_art_io_Utf8Writer_nativeWrite(JNIEnv* env, jclass, jint fd, jbyteArray data, jint off, jint len) {
  fprintf(stderr, "[nativeWrite] fd=%d len=%d\n", fd, len); fflush(stderr);
  if (fd < 0 || data == nullptr || len <= 0) {
    return -1;
  }
  jbyte buf[8192];
  jint remaining = len;
  jint src_off = off;
  jint total = 0;
  while (remaining > 0) {
    jint chunk = remaining > 8192 ? 8192 : remaining;
    env->GetByteArrayRegion(data, src_off, chunk, buf);
    ssize_t written = write(fd, buf, chunk);
    if (written <= 0) break;
    total += written;
    src_off += written;
    remaining -= written;
  }
  return total;
}

namespace art {

static std::vector<std::string> g_vm_property_options;
static std::vector<std::string> g_main_args;
static std::string g_vm_class_path;

static jlong System_nanoTimeStandalone(JNIEnv*, jclass) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<jlong>(now.tv_sec) * 1000000000LL + now.tv_nsec;
}

static jlong System_currentTimeMillisStandalone(JNIEnv*, jclass) {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  return static_cast<jlong>(now.tv_sec) * 1000LL + now.tv_nsec / 1000000LL;
}

static jobjectArray ToBootStringArray(JNIEnv* env, char* const* argv) {
  if (argv == nullptr) {
    return nullptr;
  }
  jint count = 0;
  while (argv[count] != nullptr) {
    ++count;
  }

  ScopedLocalRef<jstring> sample(env, env->NewStringUTF(""));
  if (env->ExceptionCheck()) {
    fprintf(stderr, "[dalvikvm] ToBootStringArray: sample NewStringUTF failed\n");
    env->ExceptionDescribe();
    env->ExceptionClear();
    return nullptr;
  }
  jclass string_cls = sample.get() != nullptr ? env->GetObjectClass(sample.get()) : nullptr;
  if (env->ExceptionCheck()) {
    fprintf(stderr, "[dalvikvm] ToBootStringArray: GetObjectClass(sample) failed\n");
    env->ExceptionDescribe();
    env->ExceptionClear();
    return nullptr;
  }
  if (string_cls == nullptr) {
    fprintf(stderr, "[dalvikvm] ToBootStringArray: sample string class missing\n");
    return nullptr;
  }
  jclass found_string_cls = env->FindClass("java/lang/String");
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    found_string_cls = nullptr;
  }
  if (found_string_cls != nullptr && found_string_cls != string_cls) {
    fprintf(stderr,
            "[dalvikvm] ToBootStringArray: FindClass(java/lang/String)=%p differs from NewStringUTF class=%p\n",
            found_string_cls, string_cls);
  }

  jobjectArray out = env->NewObjectArray(count, string_cls, nullptr);
  if (env->ExceptionCheck()) {
    fprintf(stderr, "[dalvikvm] ToBootStringArray: NewObjectArray failed count=%d\n", count);
    env->ExceptionDescribe();
    env->ExceptionClear();
    return nullptr;
  }
  for (jint i = 0; i < count; ++i) {
    ScopedLocalRef<jstring> s(env, env->NewStringUTF(argv[i] != nullptr ? argv[i] : ""));
    if (env->ExceptionCheck()) {
      fprintf(stderr, "[dalvikvm] ToBootStringArray: NewStringUTF failed index=%d\n", i);
      env->ExceptionDescribe();
      env->ExceptionClear();
      return nullptr;
    }
    env->SetObjectArrayElement(out, i, s.get());
    if (env->ExceptionCheck()) {
      fprintf(stderr, "[dalvikvm] ToBootStringArray: SetObjectArrayElement failed index=%d value=%s\n",
              i, argv[i] != nullptr ? argv[i] : "<null>");
      env->ExceptionDescribe();
      env->ExceptionClear();
      return nullptr;
    }
  }
  fprintf(stderr, "[dalvikvm] ToBootStringArray: built %d args with boot java/lang/String\n", count);
  return out;
}

static jint Linux_getuidStandalone(JNIEnv*, jobject) {
  return static_cast<jint>(getuid());
}

static jint Linux_getpidStandalone(JNIEnv*, jobject) {
  return static_cast<jint>(getpid());
}

static jint Linux_geteuidStandalone(JNIEnv*, jobject) {
  return static_cast<jint>(geteuid());
}

static jint Linux_getegidStandalone(JNIEnv*, jobject) {
  return static_cast<jint>(getegid());
}

static jint Linux_getgidStandalone(JNIEnv*, jobject) {
  return static_cast<jint>(getgid());
}

static jint Linux_getppidStandalone(JNIEnv*, jobject) {
  return static_cast<jint>(getppid());
}

static jlong Linux_sysconfStandalone(JNIEnv*, jobject, jint name) {
  long result = sysconf(static_cast<int>(name));
  if ((name == _SC_NPROCESSORS_CONF || name == _SC_NPROCESSORS_ONLN) &&
      (result <= 0 || result > 64)) {
    result = 4;
  }
  return static_cast<jlong>(result);
}

static void ThrowErrnoExceptionStandalone(JNIEnv* env, const char* function_name, int errnum) {
  jclass cls = env->FindClass("android/system/ErrnoException");
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return;
  }
  if (cls == nullptr) {
    return;
  }
  jmethodID ctor = env->GetMethodID(cls, "<init>", "(Ljava/lang/String;I)V");
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return;
  }
  jstring name = env->NewStringUTF(function_name);
  if (name == nullptr || env->ExceptionCheck()) {
    env->ExceptionClear();
    return;
  }
  jobject exc = env->NewObject(cls, ctor, name, static_cast<jint>(errnum));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return;
  }
  if (exc != nullptr) {
    env->Throw(reinterpret_cast<jthrowable>(exc));
  }
}

static jint LinuxGetFdStandalone(JNIEnv* env, jobject fd_obj) {
  if (fd_obj == nullptr) {
    return -1;
  }
  jclass fd_cls = env->GetObjectClass(fd_obj);
  if (fd_cls == nullptr || env->ExceptionCheck()) {
    env->ExceptionClear();
    return -1;
  }
  jfieldID descriptor = env->GetFieldID(fd_cls, "descriptor", "I");
  if (descriptor == nullptr || env->ExceptionCheck()) {
    env->ExceptionClear();
    return -1;
  }
  return env->GetIntField(fd_obj, descriptor);
}

static jobject LinuxMakeFileDescriptorStandalone(JNIEnv* env, int fd) {
  jclass fd_cls = env->FindClass("java/io/FileDescriptor");
  if (fd_cls == nullptr || env->ExceptionCheck()) {
    env->ExceptionClear();
    return nullptr;
  }
  jmethodID ctor = env->GetMethodID(fd_cls, "<init>", "()V");
  if (ctor == nullptr || env->ExceptionCheck()) {
    env->ExceptionClear();
    return nullptr;
  }
  jobject fd_obj = env->NewObject(fd_cls, ctor);
  if (fd_obj == nullptr || env->ExceptionCheck()) {
    env->ExceptionClear();
    return nullptr;
  }
  jfieldID descriptor = env->GetFieldID(fd_cls, "descriptor", "I");
  if (descriptor == nullptr || env->ExceptionCheck()) {
    env->ExceptionClear();
    return nullptr;
  }
  env->SetIntField(fd_obj, descriptor, static_cast<jint>(fd));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return nullptr;
  }
  return fd_obj;
}

static jobject Linux_openStandalone(JNIEnv* env, jobject, jstring path_j, jint flags, jint mode) {
  if (path_j == nullptr) {
    ThrowErrnoExceptionStandalone(env, "open", EINVAL);
    return nullptr;
  }
  const char* path = env->GetStringUTFChars(path_j, nullptr);
  if (path == nullptr) {
    return nullptr;
  }
  int fd = open(path, static_cast<int>(flags), static_cast<mode_t>(mode));
  int saved_errno = errno;
  env->ReleaseStringUTFChars(path_j, path);
  if (fd < 0) {
    ThrowErrnoExceptionStandalone(env, "open", saved_errno);
    return nullptr;
  }
  jobject fd_obj = LinuxMakeFileDescriptorStandalone(env, fd);
  if (fd_obj == nullptr) {
    close(fd);
  }
  return fd_obj;
}

static void Linux_closeStandalone(JNIEnv* env, jobject, jobject fd_obj) {
  int fd = LinuxGetFdStandalone(env, fd_obj);
  if (fd >= 0 && close(fd) != 0) {
    ThrowErrnoExceptionStandalone(env, "close", errno);
  }
}

static jint Linux_readBytesStandalone(JNIEnv* env,
                                      jobject,
                                      jobject fd_obj,
                                      jobject buffer,
                                      jint offset,
                                      jint byte_count) {
  int fd = LinuxGetFdStandalone(env, fd_obj);
  if (fd < 0) {
    ThrowErrnoExceptionStandalone(env, "read", EBADF);
    return -1;
  }
  if (buffer == nullptr || byte_count < 0 || offset < 0) {
    ThrowErrnoExceptionStandalone(env, "read", EINVAL);
    return -1;
  }
  jbyteArray byte_array = reinterpret_cast<jbyteArray>(buffer);
  jsize length = env->GetArrayLength(byte_array);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    ThrowErrnoExceptionStandalone(env, "read", EINVAL);
    return -1;
  }
  if (offset > length || byte_count > length - offset) {
    ThrowErrnoExceptionStandalone(env, "read", EINVAL);
    return -1;
  }
  jbyte* bytes = env->GetByteArrayElements(byte_array, nullptr);
  if (bytes == nullptr) {
    return -1;
  }
  ssize_t n = read(fd, bytes + offset, static_cast<size_t>(byte_count));
  int saved_errno = errno;
  env->ReleaseByteArrayElements(byte_array, bytes, 0);
  if (n < 0) {
    ThrowErrnoExceptionStandalone(env, "read", saved_errno);
    return -1;
  }
  return static_cast<jint>(n);
}

static jint Linux_writeBytesStandalone(JNIEnv* env,
                                       jobject,
                                       jobject fd_obj,
                                       jobject buffer,
                                       jint offset,
                                       jint byte_count) {
  int fd = LinuxGetFdStandalone(env, fd_obj);
  if (fd < 0) {
    ThrowErrnoExceptionStandalone(env, "write", EBADF);
    return -1;
  }
  if (buffer == nullptr || byte_count < 0 || offset < 0) {
    ThrowErrnoExceptionStandalone(env, "write", EINVAL);
    return -1;
  }
  jbyteArray byte_array = reinterpret_cast<jbyteArray>(buffer);
  jsize length = env->GetArrayLength(byte_array);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    ThrowErrnoExceptionStandalone(env, "write", EINVAL);
    return -1;
  }
  if (offset > length || byte_count > length - offset) {
    ThrowErrnoExceptionStandalone(env, "write", EINVAL);
    return -1;
  }
  jbyte* bytes = env->GetByteArrayElements(byte_array, nullptr);
  if (bytes == nullptr) {
    return -1;
  }
  ssize_t n = write(fd, bytes + offset, static_cast<size_t>(byte_count));
  int saved_errno = errno;
  env->ReleaseByteArrayElements(byte_array, bytes, JNI_ABORT);
  if (n < 0) {
    ThrowErrnoExceptionStandalone(env, "write", saved_errno);
    return -1;
  }
  return static_cast<jint>(n);
}

static jobject Linux_fstatStandalone(JNIEnv* env, jobject, jobject fd_obj) {
  int fd = LinuxGetFdStandalone(env, fd_obj);
  struct stat sb;
  if (fd < 0 || fstat(fd, &sb) != 0) {
    ThrowErrnoExceptionStandalone(env, "fstat", fd < 0 ? EBADF : errno);
    return nullptr;
  }
  jclass cls = env->FindClass("android/system/StructStat");
  if (cls == nullptr || env->ExceptionCheck()) {
    env->ExceptionClear();
    return nullptr;
  }
  jmethodID ctor = env->GetMethodID(cls, "<init>", "(JJIJIIJJJJJJJ)V");
  if (ctor == nullptr || env->ExceptionCheck()) {
    env->ExceptionClear();
    return nullptr;
  }
  return env->NewObject(cls,
                        ctor,
                        static_cast<jlong>(sb.st_dev),
                        static_cast<jlong>(sb.st_ino),
                        static_cast<jint>(sb.st_mode),
                        static_cast<jlong>(sb.st_nlink),
                        static_cast<jint>(sb.st_uid),
                        static_cast<jint>(sb.st_gid),
                        static_cast<jlong>(sb.st_rdev),
                        static_cast<jlong>(sb.st_size),
                        static_cast<jlong>(sb.st_atime),
                        static_cast<jlong>(sb.st_mtime),
                        static_cast<jlong>(sb.st_ctime),
                        static_cast<jlong>(sb.st_blksize),
                        static_cast<jlong>(sb.st_blocks));
}

static jobject Linux_statStandalone(JNIEnv* env, jobject, jstring path_j) {
  if (path_j == nullptr) {
    ThrowErrnoExceptionStandalone(env, "stat", EINVAL);
    return nullptr;
  }
  const char* path = env->GetStringUTFChars(path_j, nullptr);
  if (path == nullptr) {
    return nullptr;
  }
  struct stat sb;
  int rc = stat(path, &sb);
  int saved_errno = errno;
  env->ReleaseStringUTFChars(path_j, path);
  if (rc != 0) {
    ThrowErrnoExceptionStandalone(env, "stat", saved_errno);
    return nullptr;
  }
  jclass cls = env->FindClass("android/system/StructStat");
  if (cls == nullptr || env->ExceptionCheck()) {
    env->ExceptionClear();
    return nullptr;
  }
  jmethodID ctor = env->GetMethodID(cls, "<init>", "(JJIJIIJJJJJJJ)V");
  if (ctor == nullptr || env->ExceptionCheck()) {
    env->ExceptionClear();
    return nullptr;
  }
  return env->NewObject(cls,
                        ctor,
                        static_cast<jlong>(sb.st_dev),
                        static_cast<jlong>(sb.st_ino),
                        static_cast<jint>(sb.st_mode),
                        static_cast<jlong>(sb.st_nlink),
                        static_cast<jint>(sb.st_uid),
                        static_cast<jint>(sb.st_gid),
                        static_cast<jlong>(sb.st_rdev),
                        static_cast<jlong>(sb.st_size),
                        static_cast<jlong>(sb.st_atime),
                        static_cast<jlong>(sb.st_mtime),
                        static_cast<jlong>(sb.st_ctime),
                        static_cast<jlong>(sb.st_blksize),
                        static_cast<jlong>(sb.st_blocks));
}

static jbyteArray NativeReadFileBytesStandalone(JNIEnv* env, jclass, jstring path_j) {
  if (path_j == nullptr) {
    return nullptr;
  }
  const char* path = env->GetStringUTFChars(path_j, nullptr);
  if (path == nullptr) {
    return nullptr;
  }
  int fd = open(path, O_RDONLY);
  env->ReleaseStringUTFChars(path_j, path);
  if (fd < 0) {
    return nullptr;
  }

  std::vector<jbyte> bytes;
  struct stat sb;
  if (fstat(fd, &sb) == 0 && sb.st_size > 0 && sb.st_size <= (128 * 1024 * 1024)) {
    bytes.reserve(static_cast<size_t>(sb.st_size));
  }

  jbyte chunk[8192];
  while (true) {
    ssize_t n = read(fd, chunk, sizeof(chunk));
    if (n < 0 && errno == EINTR) {
      continue;
    }
    if (n <= 0) {
      break;
    }
    bytes.insert(bytes.end(), chunk, chunk + n);
  }
  close(fd);

  jbyteArray out = env->NewByteArray(static_cast<jsize>(bytes.size()));
  if (out == nullptr) {
    return nullptr;
  }
  if (!bytes.empty()) {
    env->SetByteArrayRegion(out, 0, static_cast<jsize>(bytes.size()), bytes.data());
    if (env->ExceptionCheck()) {
      return nullptr;
    }
  }
  return out;
}

static jdouble Math_sinStandalone(JNIEnv*, jclass, jdouble value) {
  return sin(value);
}

static jdouble Math_cosStandalone(JNIEnv*, jclass, jdouble value) {
  return cos(value);
}

static jdouble Math_expStandalone(JNIEnv*, jclass, jdouble value) {
  return exp(value);
}

static jdouble Math_logStandalone(JNIEnv*, jclass, jdouble value) {
  return log(value);
}

static jdouble Math_log10Standalone(JNIEnv*, jclass, jdouble value) {
  return log10(value);
}

static jdouble Math_sqrtStandalone(JNIEnv*, jclass, jdouble value) {
  return sqrt(value);
}

static jdouble Math_floorStandalone(JNIEnv*, jclass, jdouble value) {
  return floor(value);
}

static jdouble Math_ceilStandalone(JNIEnv*, jclass, jdouble value) {
  return ceil(value);
}

static jdouble Math_atanStandalone(JNIEnv*, jclass, jdouble value) {
  return atan(value);
}

static jdouble Math_atan2Standalone(JNIEnv*, jclass, jdouble lhs, jdouble rhs) {
  return atan2(lhs, rhs);
}

static jdouble Math_powStandalone(JNIEnv*, jclass, jdouble lhs, jdouble rhs) {
  return pow(lhs, rhs);
}

static jdouble Math_hypotStandalone(JNIEnv*, jclass, jdouble lhs, jdouble rhs) {
  return hypot(lhs, rhs);
}

static jdouble Math_toDegreesStandalone(JNIEnv*, jclass, jdouble value) {
  return value * (180.0 / 3.14159265358979323846);
}

static jdouble Math_toRadiansStandalone(JNIEnv*, jclass, jdouble value) {
  return value * (3.14159265358979323846 / 180.0);
}

static void PatchMcDonaldsAppCoreUtils(JNIEnv* env) {
  ScopedLocalRef<jclass> app_core_cls(
      env, env->FindClass("com/mcdonalds/mcdcoreapp/common/util/AppCoreUtils"));
  if (app_core_cls.get() == nullptr) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr, "[dalvikvm] AppCoreUtils patch skip (class missing)\n");
    return;
  }

  bool patched_clinit = false;
  bool patched_validate = false;
  {
    ScopedObjectAccess soa(Thread::Current());
    ObjPtr<mirror::Class> cls = soa.Decode<mirror::Class>(app_core_cls.get());
    if (cls != nullptr) {
      for (ArtMethod& method : cls->GetDeclaredMethods(kRuntimePointerSize)) {
        const char* name = method.GetName();
        const std::string sig = method.GetSignature().ToString();
        if (strcmp(name, "<clinit>") == 0 && sig == "()V") {
          method.SetAccessFlags(method.GetAccessFlags() | kAccNative | kAccFastNative);
          method.SetCodeItem(nullptr, false);
          method.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_void_static));
          patched_clinit = true;
        } else if (strcmp(name, "validateFromWhitelist") == 0 &&
                   sig == "(Ljava/lang/String;)V") {
          method.SetAccessFlags(method.GetAccessFlags() | kAccNative | kAccFastNative);
          method.SetCodeItem(nullptr, false);
          method.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_void_static));
          patched_validate = true;
        }
      }
    }
  }

  jfieldID whitelist_field =
      env->GetStaticFieldID(app_core_cls.get(), "whitelistClasses", "Ljava/util/HashSet;");
  if (whitelist_field == nullptr) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr,
            "[dalvikvm] AppCoreUtils patch: whitelist field missing"
            " (clinit=%d validate=%d)\n",
            patched_clinit ? 1 : 0,
            patched_validate ? 1 : 0);
    return;
  }

  ScopedLocalRef<jclass> hash_set_cls(env, env->FindClass("java/util/HashSet"));
  if (hash_set_cls.get() == nullptr) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr,
            "[dalvikvm] AppCoreUtils patch: HashSet class missing"
            " (clinit=%d validate=%d)\n",
            patched_clinit ? 1 : 0,
            patched_validate ? 1 : 0);
    return;
  }

  jmethodID hash_set_ctor = env->GetMethodID(hash_set_cls.get(), "<init>", "()V");
  if (hash_set_ctor == nullptr) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr,
            "[dalvikvm] AppCoreUtils patch: HashSet ctor missing"
            " (clinit=%d validate=%d)\n",
            patched_clinit ? 1 : 0,
            patched_validate ? 1 : 0);
    return;
  }

  ScopedLocalRef<jobject> whitelist(env, env->NewObject(hash_set_cls.get(), hash_set_ctor));
  if (whitelist.get() == nullptr) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr,
            "[dalvikvm] AppCoreUtils patch: HashSet alloc failed"
            " (clinit=%d validate=%d)\n",
            patched_clinit ? 1 : 0,
            patched_validate ? 1 : 0);
    return;
  }

  env->SetStaticObjectField(app_core_cls.get(), whitelist_field, whitelist.get());
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    fprintf(stderr,
            "[dalvikvm] AppCoreUtils patch: SetStaticObjectField failed"
            " (clinit=%d validate=%d)\n",
            patched_clinit ? 1 : 0,
            patched_validate ? 1 : 0);
    return;
  }

  fprintf(stderr,
          "[dalvikvm] AppCoreUtils patched: clinit=%d validate=%d whitelist seeded\n",
          patched_clinit ? 1 : 0,
          patched_validate ? 1 : 0);
}

static void RegisterStandaloneNativeMethods(JNIEnv* env,
                                            const char* class_name,
                                            const JNINativeMethod* methods,
                                            int method_count,
                                            bool is_static,
                                            bool probe_before_register = true) {
  ScopedLocalRef<jclass> klass(env, env->FindClass(class_name));
  if (klass.get() == nullptr) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr, "[dalvikvm] RegisterNatives skip %s (class missing)\n", class_name);
    return;
  }

  int registered = 0;
  int skipped = 0;
  int failed = 0;
  for (int i = 0; i < method_count; ++i) {
    const JNINativeMethod& method = methods[i];
    if (probe_before_register) {
      fprintf(stderr, "[dalvikvm] Probe %s.%s%s static=%d\n",
              class_name, method.name, method.signature, is_static ? 1 : 0);
      jmethodID probe = is_static
          ? env->GetStaticMethodID(klass.get(), method.name, method.signature)
          : env->GetMethodID(klass.get(), method.name, method.signature);
      if (probe == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        skipped++;
        fprintf(stderr, "[dalvikvm] RegisterNatives skip %s.%s%s (method missing)\n",
                class_name, method.name, method.signature);
        continue;
      }
    }

    int rc = env->RegisterNatives(klass.get(), &method, 1);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (rc == 0) {
      registered++;
    } else {
      failed++;
      fprintf(stderr, "[dalvikvm] RegisterNatives failed %s.%s%s\n",
              class_name, method.name, method.signature);
    }
  }

  fprintf(stderr, "[dalvikvm] RegisterNatives for %s: registered=%d skipped=%d failed=%d\n",
          class_name, registered, skipped, failed);
}

static void ForceStandaloneNativeEntryPoint(const char* class_desc,
                                            const char* method_name,
                                            const char* signature,
                                            void* native_func) {
  Thread* self = Thread::Current();
  if (self == nullptr) {
    return;
  }
  ScopedObjectAccess soa(self);
  StackHandleScope<1> hs(self);
  Handle<mirror::Class> cls =
      hs.NewHandle(Runtime::Current()->GetClassLinker()->FindSystemClass(self, class_desc));
  if (cls == nullptr) {
    if (self->IsExceptionPending()) self->ClearException();
    fprintf(stderr, "[dalvikvm] ForceNative skip %s.%s%s (class missing)\n",
            class_desc, method_name, signature);
    return;
  }

  const char* old_cause = self->StartAssertNoThreadSuspension("ForceStandaloneNativeEntryPoint");
  bool patched = false;
  for (ArtMethod& method : cls->GetDeclaredMethods(kRuntimePointerSize)) {
    if (strcmp(method.GetName(), method_name) == 0 &&
        method.GetSignature().ToString() == signature) {
      method.SetAccessFlags(method.GetAccessFlags() | kAccNative | kAccFastNative);
      method.SetCodeItem(nullptr, false);
      method.SetEntryPointFromQuickCompiledCode(GetQuickGenericJniStub());
      method.SetEntryPointFromJni(native_func);
      patched = true;
      break;
    }
  }
  self->EndAssertNoThreadSuspension(old_cause);
  if (self->IsExceptionPending()) self->ClearException();
  fprintf(stderr, "[dalvikvm] ForceNative %s.%s%s => %s (%p)\n",
          class_desc, method_name, signature, patched ? "patched" : "missing", native_func);
}

static void RegisterStandaloneCoreNatives(JNIEnv* env) {
  fprintf(stderr, "[dalvikvm] RegisterStandaloneCoreNatives begin\n");
  JNINativeMethod system_methods[] = {
      {"nanoTime", "()J", reinterpret_cast<void*>(System_nanoTimeStandalone)},
      {"currentTimeMillis", "()J", reinterpret_cast<void*>(System_currentTimeMillisStandalone)},
  };
  RegisterStandaloneNativeMethods(env,
                                  "java/lang/System",
                                  system_methods,
                                  sizeof(system_methods) / sizeof(system_methods[0]),
                                  true,
                                  false);
  ForceStandaloneNativeEntryPoint("Ljava/lang/System;",
                                  "nanoTime",
                                  "()J",
                                  reinterpret_cast<void*>(System_nanoTimeStandalone));
  ForceStandaloneNativeEntryPoint("Ljava/lang/System;",
                                  "currentTimeMillis",
                                  "()J",
                                  reinterpret_cast<void*>(System_currentTimeMillisStandalone));

  JNINativeMethod linux_methods[] = {
      {"getuid", "()I", reinterpret_cast<void*>(Linux_getuidStandalone)},
      {"nativeGetuid", "()I", reinterpret_cast<void*>(Linux_getuidStandalone)},
      {"getpid", "()I", reinterpret_cast<void*>(Linux_getpidStandalone)},
      {"nativeGetpid", "()I", reinterpret_cast<void*>(Linux_getpidStandalone)},
      {"geteuid", "()I", reinterpret_cast<void*>(Linux_geteuidStandalone)},
      {"nativeGeteuid", "()I", reinterpret_cast<void*>(Linux_geteuidStandalone)},
      {"getegid", "()I", reinterpret_cast<void*>(Linux_getegidStandalone)},
      {"nativeGetegid", "()I", reinterpret_cast<void*>(Linux_getegidStandalone)},
      {"getgid", "()I", reinterpret_cast<void*>(Linux_getgidStandalone)},
      {"nativeGetgid", "()I", reinterpret_cast<void*>(Linux_getgidStandalone)},
      {"getppid", "()I", reinterpret_cast<void*>(Linux_getppidStandalone)},
      {"nativeGetppid", "()I", reinterpret_cast<void*>(Linux_getppidStandalone)},
      {"sysconf", "(I)J", reinterpret_cast<void*>(Linux_sysconfStandalone)},
      {"nativeSysconf", "(I)J", reinterpret_cast<void*>(Linux_sysconfStandalone)},
      {"open", "(Ljava/lang/String;II)Ljava/io/FileDescriptor;", reinterpret_cast<void*>(Linux_openStandalone)},
      {"close", "(Ljava/io/FileDescriptor;)V", reinterpret_cast<void*>(Linux_closeStandalone)},
      {"readBytes", "(Ljava/io/FileDescriptor;Ljava/lang/Object;II)I", reinterpret_cast<void*>(Linux_readBytesStandalone)},
      {"read", "(Ljava/io/FileDescriptor;[BII)I", reinterpret_cast<void*>(Linux_readBytesStandalone)},
      {"writeBytes", "(Ljava/io/FileDescriptor;Ljava/lang/Object;II)I", reinterpret_cast<void*>(Linux_writeBytesStandalone)},
      {"write", "(Ljava/io/FileDescriptor;[BII)I", reinterpret_cast<void*>(Linux_writeBytesStandalone)},
      {"fstat", "(Ljava/io/FileDescriptor;)Landroid/system/StructStat;", reinterpret_cast<void*>(Linux_fstatStandalone)},
      {"stat", "(Ljava/lang/String;)Landroid/system/StructStat;", reinterpret_cast<void*>(Linux_statStandalone)},
  };
  RegisterStandaloneNativeMethods(env,
                                  "libcore/io/Linux",
                                  linux_methods,
                                  sizeof(linux_methods) / sizeof(linux_methods[0]),
                                  false,
                                  false);
  for (const JNINativeMethod& method : linux_methods) {
    ForceStandaloneNativeEntryPoint("Llibcore/io/Linux;",
                                    method.name,
                                    method.signature,
                                    method.fnPtr);
  }

  JNINativeMethod apk_loader_methods[] = {
      {"nativeReadFileBytes", "(Ljava/lang/String;)[B", reinterpret_cast<void*>(NativeReadFileBytesStandalone)},
  };
  RegisterStandaloneNativeMethods(env,
                                  "android/app/ApkLoader",
                                  apk_loader_methods,
                                  sizeof(apk_loader_methods) / sizeof(apk_loader_methods[0]),
                                  true,
                                  false);

  JNINativeMethod math_methods[] = {
      {"sin", "(D)D", reinterpret_cast<void*>(Math_sinStandalone)},
      {"cos", "(D)D", reinterpret_cast<void*>(Math_cosStandalone)},
      {"exp", "(D)D", reinterpret_cast<void*>(Math_expStandalone)},
      {"log", "(D)D", reinterpret_cast<void*>(Math_logStandalone)},
      {"log10", "(D)D", reinterpret_cast<void*>(Math_log10Standalone)},
      {"sqrt", "(D)D", reinterpret_cast<void*>(Math_sqrtStandalone)},
      {"floor", "(D)D", reinterpret_cast<void*>(Math_floorStandalone)},
      {"ceil", "(D)D", reinterpret_cast<void*>(Math_ceilStandalone)},
      {"atan", "(D)D", reinterpret_cast<void*>(Math_atanStandalone)},
      {"atan2", "(DD)D", reinterpret_cast<void*>(Math_atan2Standalone)},
      {"pow", "(DD)D", reinterpret_cast<void*>(Math_powStandalone)},
      {"hypot", "(DD)D", reinterpret_cast<void*>(Math_hypotStandalone)},
      {"toDegrees", "(D)D", reinterpret_cast<void*>(Math_toDegreesStandalone)},
      {"toRadians", "(D)D", reinterpret_cast<void*>(Math_toRadiansStandalone)},
  };
  RegisterStandaloneNativeMethods(env,
                                  "java/lang/Math",
                                  math_methods,
                                  sizeof(math_methods) / sizeof(math_methods[0]),
                                  true,
                                  false);
  RegisterStandaloneNativeMethods(env,
                                  "java/lang/StrictMath",
                                  math_methods,
                                  sizeof(math_methods) / sizeof(math_methods[0]),
                                  true,
                                  false);

  struct ForcedMathNative {
    const char* name;
    const char* signature;
    void* fn;
  };
  const ForcedMathNative forced_math[] = {
      {"sin", "(D)D", reinterpret_cast<void*>(Math_sinStandalone)},
      {"cos", "(D)D", reinterpret_cast<void*>(Math_cosStandalone)},
      {"exp", "(D)D", reinterpret_cast<void*>(Math_expStandalone)},
      {"log", "(D)D", reinterpret_cast<void*>(Math_logStandalone)},
      {"log10", "(D)D", reinterpret_cast<void*>(Math_log10Standalone)},
      {"sqrt", "(D)D", reinterpret_cast<void*>(Math_sqrtStandalone)},
      {"floor", "(D)D", reinterpret_cast<void*>(Math_floorStandalone)},
      {"ceil", "(D)D", reinterpret_cast<void*>(Math_ceilStandalone)},
      {"atan", "(D)D", reinterpret_cast<void*>(Math_atanStandalone)},
      {"atan2", "(DD)D", reinterpret_cast<void*>(Math_atan2Standalone)},
      {"pow", "(DD)D", reinterpret_cast<void*>(Math_powStandalone)},
      {"hypot", "(DD)D", reinterpret_cast<void*>(Math_hypotStandalone)},
      {"toDegrees", "(D)D", reinterpret_cast<void*>(Math_toDegreesStandalone)},
      {"toRadians", "(D)D", reinterpret_cast<void*>(Math_toRadiansStandalone)},
  };
  for (const ForcedMathNative& method : forced_math) {
    ForceStandaloneNativeEntryPoint("Ljava/lang/Math;",
                                    method.name,
                                    method.signature,
                                    method.fn);
    ForceStandaloneNativeEntryPoint("Ljava/lang/StrictMath;",
                                    method.name,
                                    method.signature,
                                    method.fn);
  }
  fprintf(stderr, "[dalvikvm] RegisterStandaloneCoreNatives end\n");
}

// Determine whether or not the specified method is public.
static bool IsMethodPublic(JNIEnv* env, jclass c, jmethodID method_id) {
  ScopedLocalRef<jobject> reflected(env, env->ToReflectedMethod(c, method_id, JNI_FALSE));
  if (reflected.get() == nullptr) {
    fprintf(stderr, "Failed to get reflected method\n");
    return false;
  }
  // We now have a Method instance.  We need to call its
  // getModifiers() method.
  jclass method_class = env->FindClass("java/lang/reflect/Method");
  if (method_class == nullptr) {
    fprintf(stderr, "Failed to find class java.lang.reflect.Method\n");
    return false;
  }
  jmethodID mid = env->GetMethodID(method_class, "getModifiers", "()I");
  if (mid == nullptr) {
    fprintf(stderr, "Failed to find java.lang.reflect.Method.getModifiers\n");
    return false;
  }
  int modifiers = env->CallIntMethod(reflected.get(), mid);
  static const int PUBLIC = 0x0001;  // java.lang.reflect.Modifiers.PUBLIC
  if ((modifiers & PUBLIC) == 0) {
    fprintf(stderr, "Modifiers mismatch\n");
    return false;
  }
  return true;
}

// Try to create a PathClassLoader for the classpath DEX and load a class through it.
// Falls back to FindClass (boot class loader) if PathClassLoader creation fails.
static jclass LoadClassFromClasspath(JNIEnv* env, const char* class_name_jni) {
  // First try the boot class loader (works if class is on boot classpath).
  // In standalone builds, -classpath DEXes are appended to boot classpath,
  // so this should find both system and app classes.
  jclass klass = env->FindClass(class_name_jni);
  if (klass != nullptr) {
    return klass;
  }
  env->ExceptionClear();

  // Try to create a PathClassLoader for the -classpath DEX.
  // dalvik.system.PathClassLoader(String dexPath, ClassLoader parent)
  jclass pcl_class = env->FindClass("dalvik/system/PathClassLoader");
  if (pcl_class == nullptr) {
    env->ExceptionClear();
    fprintf(stderr, "PathClassLoader class not found, cannot load user classes\n");
    return nullptr;
  }

  jmethodID pcl_init = env->GetMethodID(pcl_class, "<init>",
      "(Ljava/lang/String;Ljava/lang/ClassLoader;)V");
  if (pcl_init == nullptr) {
    env->ExceptionClear();
    fprintf(stderr, "PathClassLoader constructor not found\n");
    return nullptr;
  }

  // Get the classpath from the runtime options
  // The classpath is stored in the runtime's class_path_ member.
  // We can get it from the system property or we need to pass it in.
  // For now, use the environment: the -classpath arg was stored by the VM.
  const char* cp = getenv("CLASSPATH");
  // If CLASSPATH env isn't set, try to get it from RuntimeOptions
  // Actually, the Runtime stores classpath internally. Let's use Thread context.

  // Use Class.forName(name, true, classLoader) approach via the boot classpath.
  // The classpath is passed as a JVM option, so the runtime should know about it.
  // Let's try using the DexPathList approach.

  // Get the classpath from Java's system properties
  jclass system_class = env->FindClass("java/lang/System");
  if (system_class == nullptr) {
    env->ExceptionClear();
    return nullptr;
  }
  jmethodID get_prop = env->GetStaticMethodID(system_class, "getProperty",
      "(Ljava/lang/String;)Ljava/lang/String;");
  if (get_prop == nullptr) {
    env->ExceptionClear();
    return nullptr;
  }
  ScopedLocalRef<jstring> cp_key(env, env->NewStringUTF("java.class.path"));
  ScopedLocalRef<jstring> cp_val(env,
      (jstring) env->CallStaticObjectMethod(system_class, get_prop, cp_key.get()));
  if (cp_val.get() == nullptr) {
    env->ExceptionClear();
    fprintf(stderr, "java.class.path property not set\n");
    return nullptr;
  }

  // Create PathClassLoader(classpath, null)
  ScopedLocalRef<jobject> class_loader(env,
      env->NewObject(pcl_class, pcl_init, cp_val.get(), nullptr));
  if (class_loader.get() == nullptr) {
    env->ExceptionClear();
    fprintf(stderr, "Failed to create PathClassLoader\n");
    return nullptr;
  }

  fprintf(stderr, "Created PathClassLoader for classpath\n");

  // Use Class.forName(name, true, classLoader)
  jclass class_class = env->FindClass("java/lang/Class");
  if (class_class == nullptr) { env->ExceptionClear(); return nullptr; }
  jmethodID for_name = env->GetStaticMethodID(class_class, "forName",
      "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");
  if (for_name == nullptr) { env->ExceptionClear(); return nullptr; }

  // Convert JNI name (com/example/Foo) back to Java name (com.example.Foo)
  std::string java_name(class_name_jni);
  std::replace(java_name.begin(), java_name.end(), '/', '.');

  ScopedLocalRef<jstring> name_str(env, env->NewStringUTF(java_name.c_str()));
  klass = (jclass) env->CallStaticObjectMethod(class_class, for_name,
      name_str.get(), JNI_TRUE, class_loader.get());
  if (klass == nullptr || env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    fprintf(stderr, "Class.forName('%s') via PathClassLoader failed\n", java_name.c_str());
    return nullptr;
  }

  fprintf(stderr, "Loaded class '%s' via PathClassLoader\n", java_name.c_str());
  return klass;
}

static int InvokeMain(JNIEnv* env, char** argv) {
  g_main_args.clear();
  for (int i = 0; argv[i] != nullptr && i < 16; ++i) {
    fprintf(stderr, "[dalvikvm] main argv[%d]=%s\n", i, argv[i]);
    if (i > 0) {
      g_main_args.emplace_back(argv[i]);
    }
  }
  // On the current standalone guest path, JNI array construction for
  // java.lang.String[] is split-brained across loaders before WestlakeLauncher
  // even enters Java. Skip Java argv marshaling entirely and let the launcher
  // read its flags from nativeVmArg()/nativePrimeLaunchConfig().
  ScopedLocalRef<jobjectArray> args(env, nullptr);
  fprintf(stderr, "[dalvikvm] Skipping Java String[] argv; launcher will use nativeVmArg()\n");

  // Find [class].main(String[]).

  // Convert "com.android.Blah" to "com/android/Blah".
  std::string class_name(argv[0]);
  std::replace(class_name.begin(), class_name.end(), '.', '/');

  fprintf(stderr, "[dalvikvm] Looking for class '%s'\n", class_name.c_str());
  ScopedLocalRef<jclass> klass(env, LoadClassFromClasspath(env, class_name.c_str()));
  if (klass.get() == nullptr) {
    fprintf(stderr, "Unable to locate class '%s'\n", class_name.c_str());
    env->ExceptionDescribe();
    return EXIT_FAILURE;
  }
  fprintf(stderr, "[dalvikvm] Class found: %p\n", klass.get());

  // Register McdLoader helper natives (nativeAllocInstance, nativeLog)
  {
    static auto allocInstance = +[](JNIEnv* e, jclass, jclass target) -> jobject {
      jobject obj = e->AllocObject(target);
      if (e->ExceptionCheck()) {
        // Log and clear — caller will catch
        jthrowable ex = e->ExceptionOccurred();
        e->ExceptionClear();
        jclass exCls = e->GetObjectClass(ex);
        jmethodID getName = e->GetMethodID(e->FindClass("java/lang/Class"), "getName", "()Ljava/lang/String;");
        jstring name = (jstring)e->CallObjectMethod(exCls, getName);
        const char* nameStr = e->GetStringUTFChars(name, nullptr);
        fprintf(stderr, "[nativeAllocInstance] AllocObject failed: %s\n", nameStr);
        e->ReleaseStringUTFChars(name, nameStr);
        e->Throw(ex); // re-throw for Java catch
      }
      return obj;
    };
    static auto nativeLog = +[](JNIEnv* e, jclass, jstring msg) {
      if (!msg) return;
      const char* s = e->GetStringUTFChars(msg, nullptr);
      if (s) { write(STDERR_FILENO, s, strlen(s)); write(STDERR_FILENO, "\n", 1); e->ReleaseStringUTFChars(msg, s); }
    };
    static auto nativePatchClassNoop = +[](JNIEnv* e, jclass, jstring class_name_j,
                                           jobject class_loader) -> jboolean {
      if (class_name_j == nullptr || class_loader == nullptr) {
        return JNI_FALSE;
      }
      jclass class_class = e->FindClass("java/lang/Class");
      if (class_class == nullptr) {
        if (e->ExceptionCheck()) e->ExceptionClear();
        return JNI_FALSE;
      }
      jmethodID for_name = e->GetStaticMethodID(
          class_class,
          "forName",
          "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");
      if (for_name == nullptr) {
        if (e->ExceptionCheck()) e->ExceptionClear();
        return JNI_FALSE;
      }

      ScopedLocalRef<jclass> cls(
          e,
          reinterpret_cast<jclass>(
              e->CallStaticObjectMethod(class_class, for_name, class_name_j, JNI_FALSE, class_loader)));
      if (cls.get() == nullptr || e->ExceptionCheck()) {
        if (e->ExceptionCheck()) e->ExceptionClear();
        return JNI_FALSE;
      }

      std::string class_name("<unknown>");
      const char* class_name_c = e->GetStringUTFChars(class_name_j, nullptr);
      if (class_name_c != nullptr) {
        class_name.assign(class_name_c);
        e->ReleaseStringUTFChars(class_name_j, class_name_c);
      }

      int patched = 0;
      {
        ScopedObjectAccess soa(Thread::Current());
        ObjPtr<mirror::Class> mirror = soa.Decode<mirror::Class>(cls.get());
        if (mirror == nullptr) {
          return JNI_FALSE;
        }
        for (ArtMethod& m : mirror->GetDeclaredMethods(kRuntimePointerSize)) {
          if (m.IsAbstract()) {
            continue;
          }
          const char* method_name = m.GetName();
          if (strcmp(method_name, "<init>") == 0) {
            continue;
          }
          std::string sig = m.GetSignature().ToString();
          void* entry = nullptr;
          if (sig.find(")V") != std::string::npos) {
            entry = m.IsStatic()
                ? reinterpret_cast<void*>(Java_noop_return_void_static)
                : reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop);
          } else if (sig.find(")Z") != std::string::npos) {
            entry = reinterpret_cast<void*>(Java_noop_return_false);
          } else if (sig.find(")L") != std::string::npos ||
                     sig.find(")[") != std::string::npos) {
            entry = reinterpret_cast<void*>(Java_noop_return_null);
          }
          if (entry == nullptr) {
            continue;
          }
          m.SetAccessFlags(m.GetAccessFlags() | kAccNative | kAccFastNative);
          m.SetCodeItem(nullptr, false);
          m.SetEntryPointFromJni(entry);
          patched++;
        }
      }
      if (patched > 0) {
        fprintf(stderr, "[dalvikvm] nativePatchClassNoop %s patched=%d\n",
                class_name.c_str(), patched);
      }
      return patched > 0 ? JNI_TRUE : JNI_FALSE;
    };
    static auto nativeCanOpenFile = +[](JNIEnv* e, jclass, jstring path_j) -> jboolean {
      if (!path_j) return JNI_FALSE;
      const char* path = e->GetStringUTFChars(path_j, nullptr);
      if (!path) {
        return JNI_FALSE;
      }
      int fd = open(path, O_RDONLY);
      e->ReleaseStringUTFChars(path_j, path);
      if (fd < 0) {
        return JNI_FALSE;
      }
      close(fd);
      return JNI_TRUE;
    };
	    static auto nativeReadFileBytes = +[](JNIEnv* e, jclass, jstring path_j) -> jbyteArray {
	      if (!path_j) return nullptr;
	      const char* path = e->GetStringUTFChars(path_j, nullptr);
      if (!path) {
        return nullptr;
      }
      int fd = open(path, O_RDONLY);
      e->ReleaseStringUTFChars(path_j, path);
      if (fd < 0) {
        return nullptr;
      }

      std::vector<jbyte> bytes;
      struct stat sb;
      if (fstat(fd, &sb) == 0 && sb.st_size > 0 && sb.st_size <= (128 * 1024 * 1024)) {
        bytes.reserve(static_cast<size_t>(sb.st_size));
      }

      jbyte chunk[8192];
      while (true) {
        ssize_t n = read(fd, chunk, sizeof(chunk));
        if (n == 0) {
          break;
        }
        if (n < 0) {
          if (errno == EINTR) {
            continue;
          }
          close(fd);
          return nullptr;
        }
        bytes.insert(bytes.end(), chunk, chunk + n);
      }
      close(fd);

      jbyteArray out = e->NewByteArray(bytes.size());
      if (!out) {
        return nullptr;
      }
      if (!bytes.empty()) {
        e->SetByteArrayRegion(out, 0, bytes.size(), bytes.data());
        if (e->ExceptionCheck()) {
          return nullptr;
        }
	      }
	      return out;
	    };
	    static auto nativeAppendFileLine = +[](JNIEnv* e, jclass, jstring path_j,
	                                           jstring line_j) -> jboolean {
	      if (!path_j || !line_j) {
	        return JNI_FALSE;
	      }
	      const char* path = e->GetStringUTFChars(path_j, nullptr);
	      if (!path) {
	        return JNI_FALSE;
	      }
	      const char* line = e->GetStringUTFChars(line_j, nullptr);
	      if (!line) {
	        e->ReleaseStringUTFChars(path_j, path);
	        return JNI_FALSE;
	      }
	      int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0666);
	      bool ok = fd >= 0;
	      if (ok) {
	        const char* cursor = line;
	        size_t remaining = strlen(line);
	        while (remaining > 0) {
	          ssize_t n = TEMP_FAILURE_RETRY(write(fd, cursor, remaining));
	          if (n <= 0) {
	            ok = false;
	            break;
	          }
	          cursor += n;
	          remaining -= static_cast<size_t>(n);
	        }
	        if (ok) {
	          ssize_t n = TEMP_FAILURE_RETRY(write(fd, "\n", 1));
	          ok = n == 1;
	        }
	        close(fd);
	      }
	      e->ReleaseStringUTFChars(line_j, line);
	      e->ReleaseStringUTFChars(path_j, path);
	      return ok ? JNI_TRUE : JNI_FALSE;
	    };
	    static auto lookup_launch_value = +[](const char* key, std::string* out) -> bool {
	      if (key == nullptr || out == nullptr) {
	        return false;
      }
      const size_t key_len = strlen(key);
      for (const std::string& opt_str : g_vm_property_options) {
        const char* opt = opt_str.c_str();
        if (opt == nullptr || strncmp(opt, "-D", 2) != 0) {
          continue;
        }
        const char* entry = opt + 2;
        if (strncmp(entry, key, key_len) != 0) {
          continue;
        }
        if (entry[key_len] == '=') {
          out->assign(entry + key_len + 1);
          return true;
        }
        if (entry[key_len] == '\0') {
          out->clear();
          return true;
        }
      }
      const char* arg_flag = nullptr;
      if (strcmp(key, "westlake.apk.path") == 0) {
        arg_flag = "--apk-path";
      } else if (strcmp(key, "westlake.apk.activity") == 0) {
        arg_flag = "--apk-activity";
      } else if (strcmp(key, "westlake.apk.package") == 0) {
        arg_flag = "--apk-package";
      } else if (strcmp(key, "westlake.apk.resdir") == 0) {
        arg_flag = "--apk-resdir";
      } else if (strcmp(key, "westlake.apk.manifest") == 0) {
        arg_flag = "--apk-manifest";
      } else if (strcmp(key, "westlake.canary.stage") == 0) {
        arg_flag = "--canary-stage";
      }
      if (arg_flag != nullptr) {
        for (size_t i = 0; i + 1 < g_main_args.size(); ++i) {
          if (g_main_args[i] == arg_flag) {
            out->assign(g_main_args[i + 1]);
            return true;
          }
        }
      }
      int fd = open("/proc/self/cmdline", O_RDONLY);
      if (fd >= 0) {
        std::vector<char> cmdline(32768);
        ssize_t n = TEMP_FAILURE_RETRY(read(fd, cmdline.data(), cmdline.size() - 1));
        close(fd);
        if (n > 0) {
          cmdline[static_cast<size_t>(n)] = '\0';
          size_t pos = 0;
          while (pos < static_cast<size_t>(n)) {
            const char* entry = cmdline.data() + pos;
            size_t entry_len = strlen(entry);
            if (entry_len == 0) {
              ++pos;
              continue;
            }
            if (entry[0] == '-' && entry[1] == 'D') {
              const char* opt = entry + 2;
              if (strncmp(opt, key, key_len) == 0) {
                if (opt[key_len] == '=') {
                  out->assign(opt + key_len + 1);
                  return true;
                }
                if (opt[key_len] == '\0') {
                  out->clear();
                  return true;
                }
              }
            }
            pos += entry_len + 1;
          }
        }
      }
      std::string env_name;
      env_name.reserve(key_len);
      for (size_t i = 0; i < key_len; ++i) {
        unsigned char ch = static_cast<unsigned char>(key[i]);
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
          env_name.push_back(static_cast<char>(toupper(ch)));
        } else {
          env_name.push_back('_');
        }
      }
      const char* env_value = getenv(env_name.c_str());
      if (env_value != nullptr) {
        out->assign(env_value);
        return true;
      }
      const char* launch_paths[] = {
          "/data/local/tmp/westlake/westlake-launch.properties",
          "/data/local/tmp/westlake/launch.properties",
      };
      for (const char* path : launch_paths) {
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
          continue;
        }
        std::string contents;
        char buf[4096];
        while (true) {
          ssize_t n = TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)));
          if (n <= 0) {
            break;
          }
          contents.append(buf, static_cast<size_t>(n));
          if (contents.size() > 128 * 1024) {
            break;
          }
        }
        close(fd);
        size_t start = 0;
        while (start < contents.size()) {
          size_t end = contents.find('\n', start);
          if (end == std::string::npos) {
            end = contents.size();
          }
          size_t line_end = end;
          if (line_end > start && contents[line_end - 1] == '\r') {
            --line_end;
          }
          if (line_end > start + key_len
              && contents.compare(start, key_len, key) == 0
              && contents[start + key_len] == '=') {
            out->assign(contents.data() + start + key_len + 1,
                        line_end - start - key_len - 1);
            return true;
          }
          start = end + 1;
        }
      }
      return false;
    };
    static auto nativeVmProperty = +[](JNIEnv* e, jclass, jstring key_j) -> jstring {
      if (key_j == nullptr) {
        return nullptr;
      }
      const char* key = e->GetStringUTFChars(key_j, nullptr);
      if (key == nullptr) {
        return nullptr;
      }
      std::string value;
      bool found = lookup_launch_value(key, &value);
      e->ReleaseStringUTFChars(key_j, key);
      if (!found) {
        return nullptr;
      }
      return e->NewStringUTF(value.c_str());
    };
    static auto is_cutoff_canary_launch = +[]() -> bool {
      std::string value;
      if (lookup_launch_value("westlake.canary.stage", &value) && !value.empty()) {
        return true;
      }
      if (lookup_launch_value("westlake.apk.package", &value)
          && value == "com.westlake.cutoffcanary") {
        return true;
      }
      if (lookup_launch_value("westlake.apk.activity", &value)
          && value == "com.westlake.cutoffcanary.StageActivity") {
        return true;
      }
      if (lookup_launch_value("westlake.apk.path", &value)
          && value.find("cutoffcanary") != std::string::npos) {
        return true;
      }
      int fd = open("/data/local/tmp/westlake/com_westlake_cutoffcanary.apk", O_RDONLY);
      if (fd >= 0) {
        close(fd);
        return true;
      }
      return false;
    };
    static auto nativeIsCutoffCanaryLaunch = +[](JNIEnv*, jclass) -> jboolean {
      bool result = is_cutoff_canary_launch();
      if (result) {
        fprintf(stderr, "[dalvikvm] native cutoff canary launch detected\n");
      }
      return result ? JNI_TRUE : JNI_FALSE;
    };
    static auto nativeVmArgCount = +[](JNIEnv*, jclass) -> jint {
      return static_cast<jint>(g_main_args.size());
    };
    static auto nativeVmArg = +[](JNIEnv* e, jclass, jint index) -> jstring {
      if (index < 0 || static_cast<size_t>(index) >= g_main_args.size()) {
        return nullptr;
      }
      return e->NewStringUTF(g_main_args[static_cast<size_t>(index)].c_str());
    };
    static auto nativeSystemClassLoader = +[](JNIEnv* e, jclass) -> jobject {
      jobject system_loader = Runtime::Current()->GetSystemClassLoader();
      return system_loader != nullptr ? e->NewLocalRef(system_loader) : nullptr;
    };
    static auto nativeFindClass = +[](JNIEnv* e, jclass, jstring class_name_j) -> jclass {
      if (class_name_j == nullptr) {
        return nullptr;
      }
      const char* class_name = e->GetStringUTFChars(class_name_j, nullptr);
      if (class_name == nullptr) {
        return nullptr;
      }
      std::string descriptor(class_name);
      e->ReleaseStringUTFChars(class_name_j, class_name);
      if (descriptor.empty()) {
        return nullptr;
      }
      if (descriptor[0] != '[') {
        descriptor.insert(descriptor.begin(), 'L');
        descriptor.push_back(';');
      }
      for (char& ch : descriptor) {
        if (ch == '.') {
          ch = '/';
        }
      }

      ScopedObjectAccess soa(Thread::Current());
      jobject system_loader = Runtime::Current()->GetSystemClassLoader();
      StackHandleScope<1> hs(soa.Self());
      Handle<mirror::ClassLoader> loader =
          hs.NewHandle(system_loader != nullptr
                           ? soa.Decode<mirror::ClassLoader>(system_loader)
                           : ObjPtr<mirror::ClassLoader>(nullptr));
      ObjPtr<mirror::Class> found =
          Runtime::Current()->GetClassLinker()->FindClass(soa.Self(), descriptor.c_str(), loader);
      if (found == nullptr) {
        if (soa.Self()->IsExceptionPending()) {
          soa.Self()->ClearException();
        }
        return nullptr;
      }
      return soa.AddLocalReference<jclass>(found);
    };
    static auto nativePrimeLaunchConfig = +[](JNIEnv* e, jclass clazz) {
      struct FieldSpec {
        const char* field_name;
        const char* key;
      };
      const FieldSpec specs[] = {
          {"sBootApkPath", "westlake.apk.path"},
          {"sBootActivityName", "westlake.apk.activity"},
          {"sBootPackageName", "westlake.apk.package"},
          {"sBootManifestPath", "westlake.apk.manifest"},
          {"sBootResDir", "westlake.apk.resdir"},
      };
      for (const FieldSpec& spec : specs) {
        std::string value;
        if (!lookup_launch_value(spec.key, &value) || value.empty()) {
          continue;
        }
        jfieldID field = e->GetStaticFieldID(clazz, spec.field_name, "Ljava/lang/String;");
        if (field == nullptr) {
          if (e->ExceptionCheck()) e->ExceptionClear();
          continue;
        }
        jstring value_j = e->NewStringUTF(value.c_str());
        if (value_j == nullptr) {
          if (e->ExceptionCheck()) e->ExceptionClear();
          continue;
        }
        e->SetStaticObjectField(clazz, field, value_j);
        if (e->ExceptionCheck()) {
          e->ExceptionClear();
          continue;
        }
      }
      bool cutoff_canary_launch = is_cutoff_canary_launch();
      if (cutoff_canary_launch) {
        fprintf(stderr, "[dalvikvm] prime cutoff canary launch\n");
      }
      if (cutoff_canary_launch) {
        jfieldID field = e->GetStaticFieldID(clazz, "sBootCutoffCanaryLaunch", "Z");
        if (field == nullptr) {
          if (e->ExceptionCheck()) e->ExceptionClear();
        } else {
          e->SetStaticBooleanField(clazz, field, JNI_TRUE);
          if (e->ExceptionCheck()) e->ExceptionClear();
        }
      }
    };
    static auto setApkAssetsNative = +[](JNIEnv* e, jclass, jobject assetMgr, jobjectArray apkArr, jlong nativePtr) {
      // Call AssetManager.nativeSetApkAssets(long, ApkAssets[], boolean, boolean) from C
      // This bypasses Java's Method.invoke type checking
      jclass amCls = e->GetObjectClass(assetMgr);
      // Find nativeSetApkAssets
      jclass apkCls = e->FindClass("android/content/res/ApkAssets");
      if (e->ExceptionCheck()) e->ExceptionClear();
      if (!apkCls) { fprintf(stderr, "[setApkAssets] ApkAssets class not found\n"); return; }
      // Create properly typed ApkAssets[] from Object[]
      jint len = e->GetArrayLength(apkArr);
      jobjectArray typedArr = e->NewObjectArray(len, apkCls, nullptr);
      if (e->ExceptionCheck()) { e->ExceptionClear(); fprintf(stderr, "[setApkAssets] NewObjectArray failed\n"); return; }
      for (int i = 0; i < len; i++) {
        jobject elem = e->GetObjectArrayElement(apkArr, i);
        e->SetObjectArrayElement(typedArr, i, elem);
        if (e->ExceptionCheck()) { e->ExceptionClear(); break; }
      }
      // Find and call nativeSetApkAssets
      jmethodID setM = e->GetStaticMethodID(amCls, "nativeSetApkAssets",
          "(J[Landroid/content/res/ApkAssets;ZZ)V");
      if (e->ExceptionCheck()) e->ExceptionClear();
      if (setM) {
        e->CallStaticVoidMethod(amCls, setM, nativePtr, typedArr, (jboolean)false, (jboolean)false);
        if (e->ExceptionCheck()) {
          fprintf(stderr, "[setApkAssets] nativeSetApkAssets threw exception\n");
          e->ExceptionClear();
        } else {
          fprintf(stderr, "[setApkAssets] RESOURCES CONNECTED! %d ApkAssets on AM ptr=%ld\n", len, (long)nativePtr);
        }
      } else {
        fprintf(stderr, "[setApkAssets] nativeSetApkAssets method not found\n");
      }
    };
    static auto printException = +[](JNIEnv* e, jclass, jthrowable t) {
      if (!t) { fprintf(stderr, "[EXEC] null exception\n"); return; }
      // Walk cause chain, get details via field access (no Java string ops)
      jclass throwCls = e->FindClass("java/lang/Throwable");
      if (e->ExceptionCheck()) e->ExceptionClear();
      jclass classCls = e->FindClass("java/lang/Class");
      if (e->ExceptionCheck()) e->ExceptionClear();
      jmethodID getNameM = classCls ? e->GetMethodID(classCls, "getNameNative", "()Ljava/lang/String;") : nullptr;
      if (e->ExceptionCheck()) { e->ExceptionClear(); getNameM = nullptr; }
      jfieldID msgField = throwCls ? e->GetFieldID(throwCls, "detailMessage", "Ljava/lang/String;") : nullptr;
      if (e->ExceptionCheck()) e->ExceptionClear();
      jfieldID causeField = throwCls ? e->GetFieldID(throwCls, "cause", "Ljava/lang/Throwable;") : nullptr;
      if (e->ExceptionCheck()) e->ExceptionClear();
      jfieldID stField = throwCls ? e->GetFieldID(throwCls, "stackTrace", "[Ljava/lang/StackTraceElement;") : nullptr;
      if (e->ExceptionCheck()) e->ExceptionClear();

      jthrowable cur = t;
      for (int depth = 0; depth < 5 && cur; depth++) {
        jclass cls = e->GetObjectClass(cur);
        jstring nameJ = (getNameM && cls) ? (jstring)e->CallObjectMethod(cls, getNameM) : nullptr;
        if (e->ExceptionCheck()) e->ExceptionClear();
        const char* name = nameJ ? e->GetStringUTFChars(nameJ, nullptr) : "???";
        jstring msgJ = msgField ? (jstring)e->GetObjectField(cur, msgField) : nullptr;
        if (e->ExceptionCheck()) e->ExceptionClear();
        const char* msg = msgJ ? e->GetStringUTFChars(msgJ, nullptr) : nullptr;
        fprintf(stderr, "[EXEC] %s%s: %s\n", depth ? "Caused by: " : "", name, msg ? msg : "(no message)");
        if (msgJ) e->ReleaseStringUTFChars(msgJ, msg);
        if (nameJ) e->ReleaseStringUTFChars(nameJ, name);

        // Populate stackTrace by calling getStackTrace() (converts native backtrace)
        jmethodID getSTM = throwCls ? e->GetMethodID(throwCls, "getStackTrace",
            "()[Ljava/lang/StackTraceElement;") : nullptr;
        if (e->ExceptionCheck()) e->ExceptionClear();
        if (getSTM) {
          e->CallObjectMethod(cur, getSTM); // populates the field
          if (e->ExceptionCheck()) e->ExceptionClear();
        }
        // Print stack trace from stackTrace field (StackTraceElement[])
        jobjectArray frames = stField ? (jobjectArray)e->GetObjectField(cur, stField) : nullptr;
        if (e->ExceptionCheck()) e->ExceptionClear();
        if (frames) {
          int len = e->GetArrayLength(frames);
          fprintf(stderr, "[EXEC]   (%d frames)\n", len);
          jclass steCls = e->FindClass("java/lang/StackTraceElement");
          if (e->ExceptionCheck()) e->ExceptionClear();
          jfieldID dclsF = steCls ? e->GetFieldID(steCls, "declaringClass", "Ljava/lang/String;") : nullptr;
          if (e->ExceptionCheck()) e->ExceptionClear();
          jfieldID methF = steCls ? e->GetFieldID(steCls, "methodName", "Ljava/lang/String;") : nullptr;
          if (e->ExceptionCheck()) e->ExceptionClear();
          jfieldID lineF = steCls ? e->GetFieldID(steCls, "lineNumber", "I") : nullptr;
          if (e->ExceptionCheck()) e->ExceptionClear();
          int show = len < 20 ? len : 20;
          for (int i = 0; i < show; i++) {
            jobject frame = e->GetObjectArrayElement(frames, i);
            if (e->ExceptionCheck()) { e->ExceptionClear(); break; }
            if (!frame) continue;
            jstring dcls = dclsF ? (jstring)e->GetObjectField(frame, dclsF) : nullptr;
            jstring meth = methF ? (jstring)e->GetObjectField(frame, methF) : nullptr;
            jint line = lineF ? e->GetIntField(frame, lineF) : -1;
            const char* dc = dcls ? e->GetStringUTFChars(dcls, nullptr) : "?";
            const char* mn = meth ? e->GetStringUTFChars(meth, nullptr) : "?";
            fprintf(stderr, "[EXEC]   at %s.%s:%d\n", dc, mn, line);
            if (dcls) e->ReleaseStringUTFChars(dcls, dc);
            if (meth) e->ReleaseStringUTFChars(meth, mn);
          }
          if (len > show) fprintf(stderr, "[EXEC]   ... %d more\n", len - show);
        } else {
          fprintf(stderr, "[EXEC]   (no stack trace available)\n");
        }
        // Get cause — try 'cause' field first, then 'target' (InvocationTargetException)
        jthrowable next = causeField ? (jthrowable)e->GetObjectField(cur, causeField) : nullptr;
        if (e->ExceptionCheck()) e->ExceptionClear();
        if (!next || e->IsSameObject(next, cur)) {
          // Try 'target' field for InvocationTargetException
          jclass curCls = e->GetObjectClass(cur);
          jfieldID targetF = curCls ? e->GetFieldID(curCls, "target", "Ljava/lang/Throwable;") : nullptr;
          if (e->ExceptionCheck()) e->ExceptionClear();
          if (targetF) next = (jthrowable)e->GetObjectField(cur, targetF);
          if (e->ExceptionCheck()) e->ExceptionClear();
        }
        if (!next || e->IsSameObject(next, cur)) break;
        cur = next;
      }
      fflush(stderr);
    };
    JNINativeMethod methods[] = {
      {"nativeAllocInstance", "(Ljava/lang/Class;)Ljava/lang/Object;", (void*)+allocInstance},
      {"nativeLog", "(Ljava/lang/String;)V", (void*)+nativeLog},
	      {"nativeCanOpenFile", "(Ljava/lang/String;)Z", (void*)+nativeCanOpenFile},
	      {"nativeReadFileBytes", "(Ljava/lang/String;)[B", (void*)+nativeReadFileBytes},
	      {"nativeAppendFileLine", "(Ljava/lang/String;Ljava/lang/String;)Z",
	          (void*)+nativeAppendFileLine},
	      {"nativeVmProperty", "(Ljava/lang/String;)Ljava/lang/String;", (void*)+nativeVmProperty},
      {"nativeVmArgCount", "()I", (void*)+nativeVmArgCount},
      {"nativeVmArg", "(I)Ljava/lang/String;", (void*)+nativeVmArg},
      {"nativeSystemClassLoader", "()Ljava/lang/ClassLoader;", (void*)+nativeSystemClassLoader},
      {"nativeFindClass", "(Ljava/lang/String;)Ljava/lang/Class;", (void*)+nativeFindClass},
      {"nativePatchClassNoop", "(Ljava/lang/String;Ljava/lang/ClassLoader;)Z",
          (void*)+nativePatchClassNoop},
      {"nativePrimeLaunchConfig", "()V", (void*)+nativePrimeLaunchConfig},
      {"nativeIsCutoffCanaryLaunch", "()Z", (void*)+nativeIsCutoffCanaryLaunch},
      {"nativePrintException", "(Ljava/lang/Throwable;)V", (void*)+printException},
      {"nativeSetApkAssets", "(Ljava/lang/Object;[Ljava/lang/Object;J)V", (void*)+setApkAssetsNative},
    };
    int registered = 0;
    int skipped = 0;
    int failed = 0;
    for (const JNINativeMethod& method : methods) {
      jmethodID probe = env->GetStaticMethodID(klass.get(), method.name, method.signature);
      if (probe == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        skipped++;
        fprintf(stderr, "[dalvikvm] RegisterNatives skip %s.%s%s (method missing)\n",
                class_name.c_str(), method.name, method.signature);
        continue;
      }
      int rc = env->RegisterNatives(klass.get(), &method, 1);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (rc == 0) {
        registered++;
      } else {
        failed++;
        fprintf(stderr, "[dalvikvm] RegisterNatives failed %s.%s%s\n",
                class_name.c_str(), method.name, method.signature);
      }
    }
    fprintf(stderr, "[dalvikvm] RegisterNatives for %s: registered=%d skipped=%d failed=%d\n",
            class_name.c_str(), registered, skipped, failed);
  }

  RegisterStandaloneCoreNatives(env);

  jmethodID method = nullptr;
  {
    ScopedObjectAccess soa(Thread::Current());
    ObjPtr<mirror::Class> mirror = soa.Decode<mirror::Class>(klass.get());
    if (mirror != nullptr) {
      for (ArtMethod& m : mirror->GetDeclaredMethods(kRuntimePointerSize)) {
        const char* method_name = m.GetName();
        if (method_name == nullptr || strcmp(method_name, "main") != 0 || !m.IsStatic()) {
          continue;
        }
        if (m.GetSignature().ToString() == "([Ljava/lang/String;)V") {
          method = reinterpret_cast<jmethodID>(&m);
          fprintf(stderr, "[dalvikvm] main() method found via direct ART lookup: %s\n",
                  m.PrettyMethod().c_str());
          break;
        }
      }
    }
  }
  if (method == nullptr) {
    fprintf(stderr, "Unable to find static main(String[]) in '%s' via direct ART lookup\n",
            class_name.c_str());
    return EXIT_FAILURE;
  }

  // Skip IsMethodPublic check -- in standalone builds, the reflect API
  // may not be fully initialized, causing false negatives.
  // The DEX format already encodes access flags; if GetStaticMethodID succeeded,
  // the method exists and is static.
  if (false) {

  // Check class init status for key classes
  {
    const char* classes_to_check[] = {
      "java/util/concurrent/atomic/AtomicInteger",
      "java/lang/invoke/VarHandle",
      "java/lang/Integer",
      // Don't check System — it triggers <clinit> which may fail
    };
    for (const char* cls_name : classes_to_check) {
      jclass cls = env->FindClass(cls_name);
      if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
      if (!cls) continue;
      // Check if class has been initialized by checking a static field
      // For AtomicInteger, check if serialVersionUID exists (set by clinit)
      jfieldID svuid = env->GetStaticFieldID(cls, "serialVersionUID", "J");
      if (env->ExceptionCheck()) env->ExceptionClear();
      fprintf(stderr, "[dalvikvm] Class %s: loaded=%p serialVersionUID=%s\n",
              cls_name, cls, svuid ? "found" : "not-found");
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    fflush(stderr);
  }

  // Fix primitive TYPE fields: boot image may not preserve Integer.TYPE etc.
  // These are needed by VarHandle/MethodHandles for field type identity checks.
  {
    struct { const char* wrapper; const char* prim; } prims[] = {
      {"java/lang/Integer", "int"},
      {"java/lang/Long", "long"},
      {"java/lang/Boolean", "boolean"},
      {"java/lang/Byte", "byte"},
      {"java/lang/Character", "char"},
      {"java/lang/Short", "short"},
      {"java/lang/Float", "float"},
      {"java/lang/Double", "double"},
      {"java/lang/Void", "void"},
    };
    jclass classCls = env->FindClass("java/lang/Class");
    jmethodID getPrimClass = classCls ? env->GetStaticMethodID(classCls, "getPrimitiveClass",
                                          "(Ljava/lang/String;)Ljava/lang/Class;") : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();

    if (getPrimClass) {
      for (auto& p : prims) {
        jclass wrapperCls = env->FindClass(p.wrapper);
        if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
        if (!wrapperCls) continue;
        jfieldID typeField = env->GetStaticFieldID(wrapperCls, "TYPE", "Ljava/lang/Class;");
        if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
        if (!typeField) continue;
        jobject current = env->GetStaticObjectField(wrapperCls, typeField);
        if (!current) {
          jstring primName = env->NewStringUTF(p.prim);
          jobject primClass = env->CallStaticObjectMethod(classCls, getPrimClass, primName);
          if (primClass && !env->ExceptionCheck()) {
            env->SetStaticObjectField(wrapperCls, typeField, primClass);
            fprintf(stderr, "[dalvikvm] Fixed %s.TYPE = %p\n", p.wrapper, primClass);
          }
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    fflush(stderr);
  }

  // Diagnostic: check if int.class identity is broken in boot image
  {
    jclass integerClass = env->FindClass("java/lang/Integer");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (integerClass) {
      jfieldID typeField = env->GetStaticFieldID(integerClass, "TYPE", "Ljava/lang/Class;");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (typeField) {
        jobject integerType = env->GetStaticObjectField(integerClass, typeField);
        fprintf(stderr, "[dalvikvm] Integer.TYPE=%p (null means Integer.<clinit> failed or TYPE not preserved)\n",
                integerType);
        // Try to trigger Integer.<clinit> by calling Integer.valueOf(0)
        jmethodID valueOf = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");
        if (valueOf && !env->ExceptionCheck()) {
          env->CallStaticObjectMethod(integerClass, valueOf, 0);
          if (env->ExceptionCheck()) { fprintf(stderr, "[dalvikvm] Integer.valueOf(0) failed\n"); env->ExceptionDescribe(); env->ExceptionClear(); }
          // Re-check TYPE
          integerType = env->GetStaticObjectField(integerClass, typeField);
          fprintf(stderr, "[dalvikvm] Integer.TYPE after valueOf=%p\n", integerType);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();

    // Test getDeclaredField directly
    jclass atomicIntCls = env->FindClass("java/util/concurrent/atomic/AtomicInteger");
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    if (atomicIntCls) {
      jmethodID gdf = env->GetMethodID(env->FindClass("java/lang/Class"), "getDeclaredField",
                                         "(Ljava/lang/String;)Ljava/lang/reflect/Field;");
      if (gdf && !env->ExceptionCheck()) {
        jstring valueName = env->NewStringUTF("value");
        jobject field = env->CallObjectMethod(atomicIntCls, gdf, valueName);
        if (field && !env->ExceptionCheck()) {
          // Get field.getType()
          jmethodID getType = env->GetMethodID(env->FindClass("java/lang/reflect/Field"),
                                                "getType", "()Ljava/lang/Class;");
          if (getType) {
            jobject fieldType = env->CallObjectMethod(field, getType);
            fprintf(stderr, "[dalvikvm] AtomicInteger.value field found! fieldType=%p\n", fieldType);
          // Check if fieldType name is "int"
          if (fieldType) {
            jmethodID getName = env->GetMethodID(env->FindClass("java/lang/Class"), "getName", "()Ljava/lang/String;");
            if (getName && !env->ExceptionCheck()) {
              jstring typeName = (jstring)env->CallObjectMethod(fieldType, getName);
              if (typeName && !env->ExceptionCheck()) {
                const char* tn = env->GetStringUTFChars(typeName, nullptr);
                fprintf(stderr, "[dalvikvm] fieldType.getName()=%s\n", tn ? tn : "(null)");
                if (tn) env->ReleaseStringUTFChars(typeName, tn);
              }
            }
          }
          if (env->ExceptionCheck()) env->ExceptionClear();
          }
        } else {
          fprintf(stderr, "[dalvikvm] AtomicInteger.getDeclaredField('value') FAILED\n");
          if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
        }
      }
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
    fflush(stderr);
  }

  // Set up System.out / System.err before calling main().
  // System.<clinit> may fail, but FileDescriptor/FileOutputStream/PrintStream are
  // pre-initialized in the boot image, so we can create them via JNI.
  {
    fprintf(stderr, "[dalvikvm] Setting up System.out/err via JNI...\n");
    fflush(stderr);
    jclass systemCls = env->FindClass("java/lang/System");
    jclass fdCls = env->FindClass("java/io/FileDescriptor");
    jclass fosCls = env->FindClass("java/io/FileOutputStream");
    jclass psCls = env->FindClass("java/io/PrintStream");
    if (env->ExceptionCheck()) env->ExceptionClear();

    // Replace Charset.cache2 with a fresh HashMap AND also set cache1 to null.
    // Boot image collection objects have broken itable dispatch after relocation.
    {
      jclass charsetCls = env->FindClass("java/nio/charset/Charset");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (charsetCls) {
        // Replace cache2 (HashMap)
        jfieldID cache2Field = env->GetStaticFieldID(charsetCls, "cache2", "Ljava/util/HashMap;");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (cache2Field) {
          jclass hmCls = env->FindClass("java/util/HashMap");
          jmethodID hmInit = hmCls ? env->GetMethodID(hmCls, "<init>", "()V") : nullptr;
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (hmInit) {
            jobject newMap = env->NewObject(hmCls, hmInit);
            if (newMap && !env->ExceptionCheck()) {
              env->SetStaticObjectField(charsetCls, cache2Field, newMap);
              fprintf(stderr, "[dalvikvm] Replaced Charset.cache2\n");
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
          }
        }
        // Also null out cache1 (Map.Entry<String,Charset>)
        jfieldID cache1Field = env->GetStaticFieldID(charsetCls, "cache1", "Ljava/util/Map$Entry;");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (cache1Field) {
          env->SetStaticObjectField(charsetCls, cache1Field, nullptr);
          fprintf(stderr, "[dalvikvm] Cleared Charset.cache1\n");
        }
        if (env->ExceptionCheck()) env->ExceptionClear();

        // Also force Charset class status to initialized (it may be in error state)
        {
          ScopedObjectAccess soa(art::Thread::Current());
          art::ObjPtr<art::mirror::Class> cs =
              art::Runtime::Current()->GetClassLinker()->FindSystemClass(soa.Self(), "Ljava/nio/charset/Charset;");
          if (cs != nullptr && !cs->IsVisiblyInitialized()) {
            art::StackHandleScope<1> hs2(soa.Self());
            art::Handle<art::mirror::Class> h(hs2.NewHandle(cs));
            art::ObjectLock<art::mirror::Class> lock(soa.Self(), h);
            art::mirror::Class::SetStatus(h, art::ClassStatus::kVisiblyInitialized, soa.Self());
            fprintf(stderr, "[dalvikvm] Forced Charset to kVisiblyInitialized\n");
          }
          if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
        }
      }
      if (env->ExceptionCheck()) env->ExceptionClear();

      // Also replace CharsetEncoderICU.DEFAULT_REPLACEMENTS with a fresh HashMap.
      // The static HashMap may have entries with null Strings after boot image relocation,
      // or its internal Node[] has corrupted references.
      {
        jclass encICU = env->FindClass("com/android/icu/charset/CharsetEncoderICU");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (encICU) {
          jfieldID drField = env->GetStaticFieldID(encICU, "DEFAULT_REPLACEMENTS", "Ljava/util/Map;");
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (drField) {
            jclass hmCls2 = env->FindClass("java/util/HashMap");
            jmethodID hmInit2 = hmCls2 ? env->GetMethodID(hmCls2, "<init>", "()V") : nullptr;
            jmethodID hmPut = hmCls2 ? env->GetMethodID(hmCls2, "put",
                "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;") : nullptr;
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (hmInit2 && hmPut) {
              jobject newDR = env->NewObject(hmCls2, hmInit2);
              if (newDR) {
                // Add standard replacements
                jbyteArray qm = env->NewByteArray(1);
                jbyte qmByte = 0x3F;  // '?'
                env->SetByteArrayRegion(qm, 0, 1, &qmByte);
                env->CallObjectMethod(newDR, hmPut, env->NewStringUTF("UTF-8"), qm);
                env->CallObjectMethod(newDR, hmPut, env->NewStringUTF("ISO-8859-1"), qm);
                env->CallObjectMethod(newDR, hmPut, env->NewStringUTF("US-ASCII"), qm);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->SetStaticObjectField(encICU, drField, newDR);
                fprintf(stderr, "[dalvikvm] Replaced CharsetEncoderICU.DEFAULT_REPLACEMENTS\n");
              }
            }
          }
          if (env->ExceptionCheck()) env->ExceptionClear();
          // Force CharsetEncoderICU to kVisiblyInitialized if <clinit> failed
          {
            ScopedObjectAccess soa(art::Thread::Current());
            art::ObjPtr<art::mirror::Class> eiCls =
                art::Runtime::Current()->GetClassLinker()->FindSystemClass(soa.Self(),
                    "Lcom/android/icu/charset/CharsetEncoderICU;");
            if (eiCls != nullptr && !eiCls->IsVisiblyInitialized()) {
              art::StackHandleScope<1> hs3(soa.Self());
              art::Handle<art::mirror::Class> h3(hs3.NewHandle(eiCls));
              art::ObjectLock<art::mirror::Class> lock3(soa.Self(), h3);
              art::mirror::Class::SetStatus(h3, art::ClassStatus::kVisiblyInitialized, soa.Self());
              fprintf(stderr, "[dalvikvm] Forced CharsetEncoderICU to kVisiblyInitialized\n");
            }
            if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
          }
        }
      }
      fflush(stderr);
    }

    if (systemCls && fdCls && fosCls && psCls) {
      // System force-init now done in class_linker.cc RunEarlyRootClinits
      if (env->ExceptionCheck()) env->ExceptionClear();

      // ---- Early test: can we access static fields? ----
      {
        // Force-init classes whose <clinit> might crash in artFindNativeMethod
        // (native method trampoline issue in boot image relocation).
        // Only force-init classes that are NOT already kVisiblyInitialized.
        {
          ScopedObjectAccess soa(art::Thread::Current());
          auto forceInit = [&](const char* desc) {
            art::StackHandleScope<1> hs3(soa.Self());
            art::ObjPtr<art::mirror::Class> cls =
                art::Runtime::Current()->GetClassLinker()->FindSystemClass(soa.Self(), desc);
            if (cls != nullptr) {
              art::Handle<art::mirror::Class> h(hs3.NewHandle(cls));
              // Only force-init if not already initialized
              if (h->GetStatus() < art::ClassStatus::kVisiblyInitialized) {
                art::ObjectLock<art::mirror::Class> lock(soa.Self(), h);
                art::mirror::Class::SetStatus(h, art::ClassStatus::kVisiblyInitialized, soa.Self());
                fprintf(stderr, "[dalvikvm] Force-init %s OK\n", desc);
              }
            }
            if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
          };
          forceInit("Ljava/io/FileDescriptor;");
          forceInit("Ljava/io/FileOutputStream;");
          forceInit("Ljava/io/PrintStream;");
          forceInit("Ljava/io/OutputStream;");
          forceInit("Ljava/io/FilterOutputStream;");
          forceInit("Ljava/io/BufferedWriter;");
          forceInit("Ljava/io/Writer;");
          forceInit("Ljava/io/OutputStreamWriter;");
        }
        if (env->ExceptionCheck()) env->ExceptionClear();

        // Manually create FileDescriptor.in/out/err since we skipped <clinit>
        {
          jmethodID fdDefaultInit = env->GetMethodID(fdCls, "<init>", "()V");
          if (env->ExceptionCheck()) env->ExceptionClear();
          jfieldID fdField = env->GetFieldID(fdCls, "descriptor", "I");
          if (env->ExceptionCheck()) { env->ExceptionClear(); fdField = nullptr; }
          // A15 uses "fd" not "descriptor"
          if (!fdField) {
            fdField = env->GetFieldID(fdCls, "fd", "I");
            if (env->ExceptionCheck()) env->ExceptionClear();
          }

          jfieldID fdInF = env->GetStaticFieldID(fdCls, "in", "Ljava/io/FileDescriptor;");
          if (env->ExceptionCheck()) env->ExceptionClear();
          jfieldID fdOutF = env->GetStaticFieldID(fdCls, "out", "Ljava/io/FileDescriptor;");
          if (env->ExceptionCheck()) env->ExceptionClear();
          jfieldID fdErrF = env->GetStaticFieldID(fdCls, "err", "Ljava/io/FileDescriptor;");
          if (env->ExceptionCheck()) env->ExceptionClear();

          if (fdDefaultInit && fdField && fdInF && fdOutF && fdErrF) {
            // Create FileDescriptor(0) for stdin
            jobject fdIn = env->NewObject(fdCls, fdDefaultInit);
            if (env->ExceptionCheck()) { env->ExceptionClear(); fdIn = nullptr; }
            if (fdIn) { env->SetIntField(fdIn, fdField, 0); env->SetStaticObjectField(fdCls, fdInF, fdIn); }

            // Create FileDescriptor(1) for stdout
            jobject fdOutObj = env->NewObject(fdCls, fdDefaultInit);
            if (env->ExceptionCheck()) { env->ExceptionClear(); fdOutObj = nullptr; }
            if (fdOutObj) { env->SetIntField(fdOutObj, fdField, 1); env->SetStaticObjectField(fdCls, fdOutF, fdOutObj); }

            // Create FileDescriptor(2) for stderr
            jobject fdErrObj = env->NewObject(fdCls, fdDefaultInit);
            if (env->ExceptionCheck()) { env->ExceptionClear(); fdErrObj = nullptr; }
            if (fdErrObj) { env->SetIntField(fdErrObj, fdField, 2); env->SetStaticObjectField(fdCls, fdErrF, fdErrObj); }

            fprintf(stderr, "[dalvikvm] Created FileDescriptor.in/out/err manually\n");
          } else {
            fprintf(stderr, "[dalvikvm] WARN: could not create FileDescriptor objects (init=%p fd=%p)\n",
                    fdDefaultInit, fdField);
          }
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
      }

      // ---- Rebuild System.props / System.unchangeableProps from scratch ----
      // Reusing a partially initialized System.props object is unsafe here: if
      // System.<clinit> failed mid-flight we may inherit a poisoned
      // PropertiesWithNonOverrideableDefaults state where required keys still
      // resolve to null. Build a fresh unchangeable map, then rebuild props on
      // top of it so StaticProperty/File I/O sees the expected defaults chain.
      {
        jclass propsCls = env->FindClass("java/util/Properties");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (propsCls) {
          jmethodID propsInit = env->GetMethodID(propsCls, "<init>", "()V");
          if (env->ExceptionCheck()) env->ExceptionClear();
          jmethodID propsPut = propsCls ? env->GetMethodID(propsCls, "setProperty",
              "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;") : nullptr;
          jmethodID propsGet = propsCls ? env->GetMethodID(propsCls, "getProperty",
              "(Ljava/lang/String;)Ljava/lang/String;") : nullptr;
          if (env->ExceptionCheck()) env->ExceptionClear();

          if (propsInit && propsPut && propsGet) {
            jclass propsWithDefaultsCls =
                env->FindClass("java/lang/System$PropertiesWithNonOverrideableDefaults");
            if (env->ExceptionCheck()) env->ExceptionClear();
            jmethodID propsWithDefaultsInit =
                propsWithDefaultsCls
                    ? env->GetMethodID(propsWithDefaultsCls, "<init>",
                                       "(Ljava/util/Properties;)V")
                    : nullptr;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jfieldID propsField = env->GetStaticFieldID(systemCls, "props", "Ljava/util/Properties;");
            if (env->ExceptionCheck()) env->ExceptionClear();
            jfieldID ucPropsField = env->GetStaticFieldID(systemCls, "unchangeableProps", "Ljava/util/Properties;");
            if (env->ExceptionCheck()) env->ExceptionClear();
            jmethodID setUnchangeableProp = env->GetStaticMethodID(
                systemCls,
                "setUnchangeableSystemProperty",
                "(Ljava/lang/String;Ljava/lang/String;)V");
            if (env->ExceptionCheck()) {
              env->ExceptionClear();
              setUnchangeableProp = nullptr;
            }
            jmethodID setProperties = env->GetStaticMethodID(
                systemCls,
                "setProperties",
                "(Ljava/util/Properties;)V");
            if (env->ExceptionCheck()) {
              env->ExceptionClear();
              setProperties = nullptr;
            }

            jobject ucProps = env->NewObject(propsCls, propsInit);
            jobject seedProps = env->NewObject(propsCls, propsInit);
            if (env->ExceptionCheck()) {
              env->ExceptionClear();
              ucProps = nullptr;
              seedProps = nullptr;
            }
            if (ucProps && ucPropsField) {
              env->SetStaticObjectField(systemCls, ucPropsField, ucProps);
              if (env->ExceptionCheck()) env->ExceptionClear();
            }
            if (ucProps && seedProps && !env->ExceptionCheck()) {
              auto put_prop = [&](jobject target, const char* key, const char* val) -> bool {
                if (target == nullptr || key == nullptr || val == nullptr) {
                  return false;
                }
                jstring k = env->NewStringUTF(key);
                jstring v = env->NewStringUTF(val);
                bool ok = false;
                if (k != nullptr && v != nullptr && !env->ExceptionCheck()) {
                  env->CallObjectMethod(target, propsPut, k, v);
                  ok = !env->ExceptionCheck();
                  if (!ok) env->ExceptionClear();
                } else if (env->ExceptionCheck()) {
                  env->ExceptionClear();
                }
                env->DeleteLocalRef(k);
                env->DeleteLocalRef(v);
                return ok;
              };

              auto set_unchangeable_prop = [&](const char* key, const char* val) {
                if (key == nullptr || val == nullptr) {
                  return;
                }
                if (setUnchangeableProp != nullptr) {
                  jstring k = env->NewStringUTF(key);
                  jstring v = env->NewStringUTF(val);
                  if (k != nullptr && v != nullptr && !env->ExceptionCheck()) {
                    env->CallStaticVoidMethod(systemCls, setUnchangeableProp, k, v);
                    if (!env->ExceptionCheck()) {
                      env->DeleteLocalRef(k);
                      env->DeleteLocalRef(v);
                      return;
                    }
                    env->ExceptionClear();
                  } else if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                  }
                  env->DeleteLocalRef(k);
                  env->DeleteLocalRef(v);
                }
                put_prop(ucProps, key, val);
              };

              // Get cwd for user.dir
              char cwdBuf[4096];
              const char* cwd = getcwd(cwdBuf, sizeof(cwdBuf));
              if (!cwd) cwd = "/";
              const char* home = getenv("HOME");
              if (home == nullptr || home[0] == '\0') home = "/";
              const char* user = getenv("USER");
              if (user == nullptr || user[0] == '\0') user = "westlake";
              const char* tmpdir = getenv("TMPDIR");
              if (tmpdir == nullptr || tmpdir[0] == '\0') tmpdir = "/tmp";
              const char* art_root = getenv("ANDROID_ART_ROOT");
              if (art_root == nullptr || art_root[0] == '\0') art_root = "/apex/com.android.art";
              if (art_root[0] == '\0') art_root = "/";
              const char* class_path = getenv("CLASSPATH");
              if (class_path == nullptr) class_path = "";
              const char* library_path = getenv("LD_LIBRARY_PATH");
              if (library_path == nullptr || library_path[0] == '\0') {
                library_path =
                    "/data/local/tmp/westlake:/apex/com.android.runtime/lib64:"
                    "/apex/com.android.art/lib64:/system/lib64:/vendor/lib64";
              }
#if defined(__aarch64__)
              const char* os_arch = "aarch64";
#elif defined(__x86_64__)
              const char* os_arch = "x86_64";
#elif defined(__arm__)
              const char* os_arch = "arm";
#else
              const char* os_arch = "unknown";
#endif

              struct { const char* key; const char* val; } sysProps[] = {
                {"user.language", "en"},
                {"user.region", "US"},
                {"user.country", "US"},
                {"user.locale", "en-US"},
                {"user.script", ""},
                {"user.variant", ""},
                {"file.encoding", "UTF-8"},
                {"file.separator", "/"},
                {"path.separator", ":"},
                {"line.separator", "\n"},
                {"user.home", home},
                {"user.dir", cwd},
                {"user.name", user},
                {"java.io.tmpdir", tmpdir},
                {"os.name", "Linux"},
                {"os.arch", os_arch},
                {"java.class.path", class_path},
                {"java.library.path", library_path},
                {"java.home", art_root},
                {"java.vm.name", "Dalvik"},
                {"java.vm.version", "2.1.0"},
                {"java.specification.version", "1.8"},
              };

              for (auto& sp : sysProps) {
                set_unchangeable_prop(sp.key, sp.val);
              }

              for (const std::string& opt_str : g_vm_property_options) {
                const char* opt = opt_str.c_str();
                if (opt == nullptr || strncmp(opt, "-D", 2) != 0) {
                  continue;
                }
                const char* key_start = opt + 2;
                if (*key_start == '\0') {
                  continue;
                }
                const char* eq = strchr(key_start, '=');
                std::string key;
                std::string value;
                if (eq == nullptr) {
                  key.assign(key_start);
                  value.assign("");
                } else {
                  key.assign(key_start, eq - key_start);
                  value.assign(eq + 1);
                }
                if (!key.empty()) {
                  set_unchangeable_prop(key.c_str(), value.c_str());
                }
              }

              jobject rebuiltProps = nullptr;
              bool rebuiltViaSystemApi = false;
              if (setProperties != nullptr) {
                env->CallStaticVoidMethod(systemCls, setProperties, seedProps);
                if (!env->ExceptionCheck()) {
                  rebuiltViaSystemApi = true;
                  rebuiltProps = propsField ? env->GetStaticObjectField(systemCls, propsField) : nullptr;
                  if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    rebuiltProps = nullptr;
                  }
                } else {
                  fprintf(stderr, "[dalvikvm] WARN: System.setProperties threw during repair\n");
                  env->ExceptionClear();
                }
              }
              if (rebuiltProps == nullptr && propsWithDefaultsInit != nullptr) {
                rebuiltProps = env->NewObject(propsWithDefaultsCls, propsWithDefaultsInit, ucProps);
                if (env->ExceptionCheck()) {
                  env->ExceptionClear();
                  rebuiltProps = nullptr;
                }
                if (rebuiltProps != nullptr && propsField) {
                  env->SetStaticObjectField(systemCls, propsField, rebuiltProps);
                  if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    rebuiltProps = nullptr;
                  }
                }
              }
              if (rebuiltProps == nullptr && propsField) {
                rebuiltProps = env->NewObject(propsCls, propsInit);
                if (env->ExceptionCheck()) {
                  env->ExceptionClear();
                  rebuiltProps = nullptr;
                }
                if (rebuiltProps != nullptr) {
                  for (auto& sp : sysProps) {
                    put_prop(rebuiltProps, sp.key, sp.val);
                  }
                  for (const std::string& opt_str : g_vm_property_options) {
                    const char* opt = opt_str.c_str();
                    if (opt == nullptr || strncmp(opt, "-D", 2) != 0) {
                      continue;
                    }
                    const char* key_start = opt + 2;
                    if (*key_start == '\0') {
                      continue;
                    }
                    const char* eq = strchr(key_start, '=');
                    std::string key;
                    std::string value;
                    if (eq == nullptr) {
                      key.assign(key_start);
                      value.assign("");
                    } else {
                      key.assign(key_start, eq - key_start);
                      value.assign(eq + 1);
                    }
                    if (!key.empty()) {
                      put_prop(rebuiltProps, key.c_str(), value.c_str());
                    }
                  }
                  env->SetStaticObjectField(systemCls, propsField, rebuiltProps);
                  if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    rebuiltProps = nullptr;
                  }
                }
              }

              if (ucPropsField && ucProps != nullptr) {
                env->SetStaticObjectField(systemCls, ucPropsField, ucProps);
                if (env->ExceptionCheck()) env->ExceptionClear();
              }

              if (rebuiltProps != nullptr) {
                fprintf(stderr,
                        "[dalvikvm] System.props rebuilt (%s) and VM -D properties preserved\n",
                        rebuiltViaSystemApi ? "System.setProperties" : "manual");
              } else {
                fprintf(stderr, "[dalvikvm] WARN: System.props rebuild failed\n");
              }

              auto log_resolved_prop = [&](const char* key) {
                if (rebuiltProps == nullptr || key == nullptr) {
                  return;
                }
                jstring k = env->NewStringUTF(key);
                if (k == nullptr || env->ExceptionCheck()) {
                  env->ExceptionClear();
                  env->DeleteLocalRef(k);
                  return;
                }
                jstring value = (jstring)env->CallObjectMethod(rebuiltProps, propsGet, k);
                if (env->ExceptionCheck()) {
                  env->ExceptionClear();
                  value = nullptr;
                }
                const char* value_utf = value ? env->GetStringUTFChars(value, nullptr) : nullptr;
                fprintf(stderr, "[dalvikvm]   %s=%s\n", key, value_utf ? value_utf : "(null)");
                if (value && value_utf) env->ReleaseStringUTFChars(value, value_utf);
                env->DeleteLocalRef(value);
                env->DeleteLocalRef(k);
              };
              log_resolved_prop("user.name");
              log_resolved_prop("user.dir");
              log_resolved_prop("user.home");
              log_resolved_prop("java.io.tmpdir");
              log_resolved_prop("java.library.path");
              log_resolved_prop("java.class.path");
              log_resolved_prop("java.home");

              env->DeleteLocalRef(seedProps);
              env->DeleteLocalRef(ucProps);
              env->DeleteLocalRef(rebuiltProps);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
          }

          // Set System.lineSeparator field — used by System.lineSeparator().
          // System.<clinit> normally sets this, but in the standalone build
          // <clinit> may fail before reaching it.
          // Field name: "lineSeparator" (A11+), fallback "lineSep".
          {
            jfieldID lineSepF = env->GetStaticFieldID(systemCls, "lineSeparator", "Ljava/lang/String;");
            if (env->ExceptionCheck()) { env->ExceptionClear(); lineSepF = nullptr; }
            if (!lineSepF) {
              lineSepF = env->GetStaticFieldID(systemCls, "lineSep", "Ljava/lang/String;");
              if (env->ExceptionCheck()) { env->ExceptionClear(); lineSepF = nullptr; }
            }
            if (lineSepF) {
              jobject cur = env->GetStaticObjectField(systemCls, lineSepF);
              if (!cur) {
                jstring nl = env->NewStringUTF("\n");
                env->SetStaticObjectField(systemCls, lineSepF, nl);
                fprintf(stderr, "[dalvikvm] Set System.lineSeparator to \\n\n");
              } else {
                fprintf(stderr, "[dalvikvm] System.lineSeparator already set\n");
              }
            } else {
              fprintf(stderr, "[dalvikvm] WARN: System.lineSeparator field not found\n");
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
          }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        fflush(stderr);
      }

      // ---- Pre-initialize Locale ----
      // Now that System.props has user.language=en, user.region=US,
      // trigger Locale.<clinit> naturally via JNI. If it fails, force-init.
      {
        jobject enUsLocale = nullptr;
        // Try natural init via JNI FindClass (triggers <clinit>)
        jclass localeCls = env->FindClass("java/util/Locale");
        if (env->ExceptionCheck()) {
          fprintf(stderr, "[dalvikvm] Locale.<clinit> failed, falling back to force-init\n");
          env->ExceptionClear();
          localeCls = nullptr;
        }
        if (localeCls) {
          // <clinit> succeeded — get Locale.US
          jfieldID usField = env->GetStaticFieldID(localeCls, "US", "Ljava/util/Locale;");
          if (usField && !env->ExceptionCheck()) {
            enUsLocale = env->GetStaticObjectField(localeCls, usField);
          }
          if (env->ExceptionCheck()) env->ExceptionClear();
          fprintf(stderr, "[dalvikvm] Locale initialized naturally, US=%p\n", enUsLocale);
        }
        if (!localeCls) {
          // Fallback: force-init with ART internals
          ScopedObjectAccess soa(art::Thread::Current());
          art::ClassLinker* cl = art::Runtime::Current()->GetClassLinker();
          art::ObjPtr<art::mirror::Class> locMirror =
              cl->FindSystemClass(soa.Self(), "Ljava/util/Locale;");
          if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
          if (locMirror != nullptr && !locMirror->IsVisiblyInitialized()) {
            art::StackHandleScope<1> hsL(soa.Self());
            art::Handle<art::mirror::Class> hL(hsL.NewHandle(locMirror));
            art::ObjectLock<art::mirror::Class> lockL(soa.Self(), hL);
            art::mirror::Class::SetStatus(hL, art::ClassStatus::kVisiblyInitialized, soa.Self());
            fprintf(stderr, "[dalvikvm] Forced Locale to kVisiblyInitialized\n");
          }
          if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();

        } // end fallback force-init

        // Now that both Locale and NoImagePreloadHolder are forced-initialized,
        // we can safely use JNI to create a Locale and set the field.
        {
          jclass localeCls = env->FindClass("java/util/Locale");
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (localeCls) {
            // Try to get Locale.US (static final field, set in boot image)
            jfieldID usField = env->GetStaticFieldID(localeCls, "US", "Ljava/util/Locale;");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (usField) {
              enUsLocale = env->GetStaticObjectField(localeCls, usField);
              if (enUsLocale) {
                fprintf(stderr, "[dalvikvm] Got Locale.US from static field\n");
              }
            }
            // Fallback: try constructor
            if (!enUsLocale) {
              jmethodID localeInit = env->GetMethodID(localeCls, "<init>",
                  "(Ljava/lang/String;Ljava/lang/String;)V");
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (localeInit) {
                jstring lang = env->NewStringUTF("en");
                jstring country = env->NewStringUTF("US");
                enUsLocale = env->NewObject(localeCls, localeInit, lang, country);
                if (env->ExceptionCheck()) { env->ExceptionClear(); enUsLocale = nullptr; }
              }
            }
          }
        }

        // Locale initialized naturally. Now force-init NoImagePreloadHolder
        // and set its defaultLocale field via JNI (to avoid its <clinit> which
        // calls initDefault() → ICU → NPE).
        if (enUsLocale) {
          // Force NoImagePreloadHolder to kVisiblyInitialized without running <clinit>
          {
            ScopedObjectAccess soa(art::Thread::Current());
            art::ObjPtr<art::mirror::Class> niph =
                art::Runtime::Current()->GetClassLinker()->FindSystemClass(soa.Self(),
                    "Ljava/util/Locale$NoImagePreloadHolder;");
            if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
            if (niph != nullptr && !niph->IsVisiblyInitialized()) {
              art::StackHandleScope<1> hs9(soa.Self());
              art::Handle<art::mirror::Class> h9(hs9.NewHandle(niph));
              art::ObjectLock<art::mirror::Class> lock9(soa.Self(), h9);
              art::mirror::Class::SetStatus(h9, art::ClassStatus::kVisiblyInitialized, soa.Self());
              fprintf(stderr, "[dalvikvm] Forced NoImagePreloadHolder to kVisiblyInitialized\n");
            }
            if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
          }
          // Set defaultLocale via JNI FindClass (class is already kVisiblyInitialized,
          // so FindClass won't re-trigger <clinit>)
          {
            jclass niphCls = env->FindClass("java/util/Locale$NoImagePreloadHolder");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (niphCls) {
              jfieldID dlField = env->GetStaticFieldID(niphCls, "defaultLocale", "Ljava/util/Locale;");
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (dlField) {
                env->SetStaticObjectField(niphCls, dlField, enUsLocale);
                fprintf(stderr, "[dalvikvm] Set NoImagePreloadHolder.defaultLocale via JNI\n");
              }
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
          }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        fflush(stderr);
      }

      // Re-find classes to avoid stale references after heavy class init above
      fdCls = env->FindClass("java/io/FileDescriptor");
      if (env->ExceptionCheck()) { env->ExceptionClear(); fdCls = nullptr; }
      fosCls = env->FindClass("java/io/FileOutputStream");
      if (env->ExceptionCheck()) { env->ExceptionClear(); fosCls = nullptr; }
      psCls = env->FindClass("java/io/PrintStream");
      if (env->ExceptionCheck()) { env->ExceptionClear(); psCls = nullptr; }
      // Get FileDescriptor.out (fd=1) and FileDescriptor.err (fd=2)
      if (!fdCls || !fosCls || !psCls) {
        fprintf(stderr, "[dalvikvm] WARN: class re-find failed, skipping System.out setup\n");
        fflush(stderr);
      } else {
      jfieldID fdOutField = env->GetStaticFieldID(fdCls, "out", "Ljava/io/FileDescriptor;");
      if (env->ExceptionCheck()) { env->ExceptionClear(); fdOutField = nullptr; }
      jfieldID fdErrField = env->GetStaticFieldID(fdCls, "err", "Ljava/io/FileDescriptor;");
      if (env->ExceptionCheck()) { env->ExceptionClear(); fdErrField = nullptr; }

      if (fdOutField && fdErrField) {
        jobject fdOut = env->GetStaticObjectField(fdCls, fdOutField);
        if (env->ExceptionCheck()) { env->ExceptionClear(); fdOut = nullptr; }
        jobject fdErr = env->GetStaticObjectField(fdCls, fdErrField);
        if (env->ExceptionCheck()) { env->ExceptionClear(); fdErr = nullptr; }

        // Create FileOutputStream(FileDescriptor)
        jmethodID fosInit = env->GetMethodID(fosCls, "<init>", "(Ljava/io/FileDescriptor;)V");
        if (env->ExceptionCheck()) { env->ExceptionClear(); fosInit = nullptr; }
        if (env->ExceptionCheck()) env->ExceptionClear();

        if (fosInit && fdOut && fdErr) {
          jobject fosOut = env->NewObject(fosCls, fosInit, fdOut);
          if (env->ExceptionCheck()) { env->ExceptionClear(); fosOut = nullptr; }
          jobject fosErr = env->NewObject(fosCls, fosInit, fdErr);
          if (env->ExceptionCheck()) { env->ExceptionClear(); fosErr = nullptr; }

          // Create PrintStream — try (OutputStream, boolean, Charset) with a
          // pre-resolved charset to avoid Charset.forName() which has boot image
          // vtable corruption in TreeMap collections.
          // Strategy: find sun.nio.cs.UTF_8 class (simple charset, no ICU needed),
          // instantiate it, and pass to PrintStream(OutputStream, boolean, Charset).
          jmethodID psInit = nullptr;
          jobject psOut = nullptr;
          jobject psErr = nullptr;

          // Try to find and instantiate sun.nio.cs.UTF_8 charset directly
          jclass utf8Cls = env->FindClass("sun/nio/cs/UTF_8");
          if (env->ExceptionCheck()) { env->ExceptionClear(); utf8Cls = nullptr; }
          jobject utf8Charset = nullptr;
          if (utf8Cls) {
            jmethodID utf8Init = env->GetMethodID(utf8Cls, "<init>", "()V");
            fprintf(stderr, "[dalvikvm] DBG: utf8Init=%p\n", utf8Init); fflush(stderr);
            if (utf8Init && !env->ExceptionCheck()) {
              utf8Charset = env->NewObject(utf8Cls, utf8Init);
              if (env->ExceptionCheck()) { env->ExceptionClear(); utf8Charset = nullptr; }
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
          }

          // Pre-populate Charset.cache1 with the sun.nio.cs.UTF_8 instance so
          // future Charset.forName("UTF-8") returns it from cache1 immediately
          // without going through lookup2 → charsetForName → CharsetEncoderICU.
          if (utf8Charset) {
            jclass charsetCls2 = env->FindClass("java/nio/charset/Charset");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (charsetCls2) {
              jfieldID cache1F = env->GetStaticFieldID(charsetCls2, "cache1", "Ljava/util/Map$Entry;");
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (cache1F) {
                // Create AbstractMap.SimpleImmutableEntry<String, Charset>("UTF-8", utf8Charset)
                jclass sieCls = env->FindClass("java/util/AbstractMap$SimpleImmutableEntry");
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (sieCls) {
                  jmethodID sieInit = env->GetMethodID(sieCls, "<init>",
                      "(Ljava/lang/Object;Ljava/lang/Object;)V");
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (sieInit) {
                    jstring utf8Name = env->NewStringUTF("UTF-8");
                    jobject entry = env->NewObject(sieCls, sieInit, utf8Name, utf8Charset);
                    if (entry && !env->ExceptionCheck()) {
                      env->SetStaticObjectField(charsetCls2, cache1F, entry);
                      fprintf(stderr, "[dalvikvm] Set Charset.cache1 to UTF-8 (sun.nio.cs.UTF_8)\n");
                    }
                    if (env->ExceptionCheck()) env->ExceptionClear();
                  }
                }
              }
              // Also put UTF-8 into cache2 HashMap
              jfieldID cache2F = env->GetStaticFieldID(charsetCls2, "cache2", "Ljava/util/HashMap;");
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (cache2F) {
                jobject cache2Map = env->GetStaticObjectField(charsetCls2, cache2F);
                if (cache2Map) {
                  jclass hmCls3 = env->FindClass("java/util/HashMap");
                  jmethodID hmPut3 = hmCls3 ? env->GetMethodID(hmCls3, "put",
                      "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;") : nullptr;
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (hmPut3) {
                    jstring utf8Key = env->NewStringUTF("UTF-8");
                    env->CallObjectMethod(cache2Map, hmPut3, utf8Key, utf8Charset);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    // Also add lowercase variant
                    jstring utf8Key2 = env->NewStringUTF("utf-8");
                    env->CallObjectMethod(cache2Map, hmPut3, utf8Key2, utf8Charset);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    fprintf(stderr, "[dalvikvm] Added UTF-8 to Charset.cache2\n");
                  }
                }
              }
              if (env->ExceptionCheck()) env->ExceptionClear();
            }
          }

          // Also set StandardCharsets.UTF_8 field directly
          if (utf8Charset) {
            jclass stdCharsets = env->FindClass("java/nio/charset/StandardCharsets");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (stdCharsets) {
              jfieldID utf8Field = env->GetStaticFieldID(stdCharsets, "UTF_8", "Ljava/nio/charset/Charset;");
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (utf8Field) {
                env->SetStaticObjectField(stdCharsets, utf8Field, utf8Charset);
                fprintf(stderr, "[dalvikvm] Set StandardCharsets.UTF_8\n");
              }
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
          }

          if (utf8Charset) {
            // PrintStream(OutputStream out, boolean autoFlush, Charset charset)
            jmethodID psInitCS = env->GetMethodID(psCls, "<init>",
                "(Ljava/io/OutputStream;ZLjava/nio/charset/Charset;)V");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (psInitCS) {
              psOut = env->NewObject(psCls, psInitCS, fosOut, JNI_FALSE, utf8Charset);
              if (env->ExceptionCheck()) { env->ExceptionClear(); psOut = nullptr; }
              psErr = env->NewObject(psCls, psInitCS, fosErr, JNI_FALSE, utf8Charset);
              if (env->ExceptionCheck()) { env->ExceptionClear(); psErr = nullptr; }
              fprintf(stderr, "[dalvikvm] Created PrintStream with UTF_8 charset\n");
            }
          }

          // Fallback: basic PrintStream(OutputStream)
          if (!psOut) {
            psInit = env->GetMethodID(psCls, "<init>", "(Ljava/io/OutputStream;)V");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (psInit) {
              psOut = env->NewObject(psCls, psInit, fosOut);
              if (env->ExceptionCheck()) { env->ExceptionClear(); psOut = nullptr; }
              psErr = env->NewObject(psCls, psInit, fosErr);
              if (env->ExceptionCheck()) { env->ExceptionClear(); psErr = nullptr; }
            }
          }

          // If PrintStream creation succeeded with charset, use it.
          // Otherwise create a minimal NativePrintStream that writes raw bytes.
          // For the fallback, we override System.out.println by providing a custom
          // native print method via JNI - but that's complex.
          // Instead: if psOut is null, create it with the basic constructor anyway.
          // The AbstractMethodError only happens when println is called, not at construction.
          if (!psOut && psInit) {
            psOut = env->NewObject(psCls, psInit, fosOut);
            if (env->ExceptionCheck()) { env->ExceptionClear(); psOut = nullptr; }
          }
          if (!psErr && psInit) {
            psErr = env->NewObject(psCls, psInit, fosErr);
            if (env->ExceptionCheck()) { env->ExceptionClear(); psErr = nullptr; }
          }

          // Pre-set PrintStream's internal textOut field to bypass getTextOut()
          // which triggers Charset.forName() → CharsetICU.newEncoder() →
          // CharsetEncoderICU.makeReplacement() → HashMap itable dispatch bug (NPE).
          //
          // Use art.io.Utf8Writer — a pure-Java Writer that encodes chars as
          // UTF-8 bytes without touching ICU or Charset at all.  It lives in
          // core-jars/art-patch.jar which must be on the boot classpath.
          if (psOut) {
            jclass uwCls = env->FindClass("art/io/Utf8Writer");
            if (env->ExceptionCheck()) { env->ExceptionClear(); uwCls = nullptr; }
            jclass bwCls = env->FindClass("java/io/BufferedWriter");
            if (env->ExceptionCheck()) { env->ExceptionClear(); bwCls = nullptr; }
            jclass oswCls = env->FindClass("java/io/OutputStreamWriter");
            if (env->ExceptionCheck()) { env->ExceptionClear(); oswCls = nullptr; }
            if (uwCls && bwCls) {
              // Utf8Writer(OutputStream)
              jmethodID uwInit = env->GetMethodID(uwCls, "<init>",
                  "(Ljava/io/OutputStream;)V");
              if (env->ExceptionCheck()) { env->ExceptionClear(); uwInit = nullptr; }
              // Use (Writer, int) constructor to avoid BufferedWriter.defaultCharBufferSize
              // which is 0 because we force-inited BufferedWriter (skipped <clinit>)
              jmethodID bwInit = env->GetMethodID(bwCls, "<init>", "(Ljava/io/Writer;I)V");
              if (env->ExceptionCheck()) { env->ExceptionClear(); bwInit = nullptr; }

              // Create a dummy OutputStreamWriter via AllocObject (no ctor)
              // for charOut.  Its flushBuffer() calls se.flushBuffer().
              // We create a StreamEncoder via AllocObject too, with isOpen=false
              // so flushBuffer() throws IOException (which PrintStream catches).
              jobject dummyOSW = nullptr;
              if (oswCls) {
                dummyOSW = env->AllocObject(oswCls);
                if (env->ExceptionCheck()) { env->ExceptionClear(); dummyOSW = nullptr; }
                if (dummyOSW) {
                  jfieldID seF = env->GetFieldID(oswCls, "se", "Lsun/nio/cs/StreamEncoder;");
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (seF) {
                    jclass seCls = env->FindClass("sun/nio/cs/StreamEncoder");
                    if (env->ExceptionCheck()) { env->ExceptionClear(); seCls = nullptr; }
                    if (seCls) {
                      jobject dummySE = env->AllocObject(seCls);
                      if (env->ExceptionCheck()) { env->ExceptionClear(); dummySE = nullptr; }
                      if (dummySE) {
                        // StreamEncoder.isOpen = false so flushBuffer() throws
                        // IOException "Stream closed" — which PrintStream catches.
                        jfieldID isOpenF = env->GetFieldID(seCls, "isOpen", "Z");
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        if (isOpenF) env->SetBooleanField(dummySE, isOpenF, JNI_FALSE);
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        // Set lock object (synchronized(lock) in flushBuffer)
                        jclass writerCls = env->FindClass("java/io/Writer");
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        if (writerCls) {
                          jfieldID lockF = env->GetFieldID(writerCls, "lock", "Ljava/lang/Object;");
                          if (env->ExceptionCheck()) env->ExceptionClear();
                          if (lockF) {
                            // Use the OutputStream as the lock
                            env->SetObjectField(dummySE, lockF, fosOut);
                          }
                          if (env->ExceptionCheck()) env->ExceptionClear();
                        }
                        env->SetObjectField(dummyOSW, seF, dummySE);
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        fprintf(stderr, "[dalvikvm] Created dummy OutputStreamWriter for charOut\n");
                      }
                    }
                  }
                }
              }
              if (env->ExceptionCheck()) env->ExceptionClear();

              if (uwInit && bwInit) {
                jstring newline = env->NewStringUTF("\n");
                // Get lineSeparator field — try all possible names and types
                jfieldID bwLineSepF = nullptr;
                const char* sep_names[] = {"lineSeparator", "lineSep"};
                const char* sep_types[] = {"Ljava/lang/String;", "[C"};
                for (const char* name : sep_names) {
                  for (const char* type : sep_types) {
                    bwLineSepF = env->GetFieldID(bwCls, name, type);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); bwLineSepF = nullptr; }
                    if (bwLineSepF) {
                      fprintf(stderr, "[dalvikvm] Found BW field %s type %s\n", name, type);
                      break;
                    }
                  }
                  if (bwLineSepF) break;
                }
                if (!bwLineSepF) fprintf(stderr, "[dalvikvm] WARN: no lineSep field found in BufferedWriter\n");
                fflush(stderr);

                jfieldID textOutF = env->GetFieldID(psCls, "textOut", "Ljava/io/BufferedWriter;");
                if (env->ExceptionCheck()) { env->ExceptionClear(); textOutF = nullptr; }
                jfieldID charOutF = env->GetFieldID(psCls, "charOut", "Ljava/io/OutputStreamWriter;");
                if (env->ExceptionCheck()) env->ExceptionClear();

                // Helper lambda for setting up textOut/charOut on a PrintStream
                jmethodID setFdM = env->GetMethodID(uwCls, "setFd", "(I)V");
                if (env->ExceptionCheck()) { env->ExceptionClear(); setFdM = nullptr; }
                fprintf(stderr, "[dalvikvm] setFdM=%p\n", setFdM); fflush(stderr);
                auto setupPrintStream = [&](jobject ps, jobject fos, const char* name, int rawFd) {
                  jobject uw = env->NewObject(uwCls, uwInit, fos);
                  if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
                  if (!uw) return;
                  // Set the raw fd for native write
                  if (setFdM) {
                    env->CallVoidMethod(uw, setFdM, rawFd);
                    if (env->ExceptionCheck()) {
                      fprintf(stderr, "[dalvikvm] setFd(%d) FAILED\n", rawFd);
                      env->ExceptionDescribe();
                      env->ExceptionClear();
                    } else {
                      fprintf(stderr, "[dalvikvm] setFd(%d) OK\n", rawFd);
                    }
                  }
                  jobject bw = env->NewObject(bwCls, bwInit, uw, (jint)8192);
                  if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
                  if (!bw) return;
                  // Fix lineSeparator if field exists
                  if (bwLineSepF) {
                    env->SetObjectField(bw, bwLineSepF, newline);
                  }
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (textOutF) env->SetObjectField(ps, textOutF, bw);
                  if (charOutF && dummyOSW) env->SetObjectField(ps, charOutF, dummyOSW);
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  fprintf(stderr, "[dalvikvm] Pre-set PrintStream.textOut via Utf8Writer (%s)\n", name);
                };

                setupPrintStream(psOut, fosOut, "System.out", 1);
                if (psErr) setupPrintStream(psErr, fosErr, "System.err", 2);
              }
            } else {
              fprintf(stderr, "[dalvikvm] WARN: art.io.Utf8Writer not found — add art-patch.jar to boot classpath\n");
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
          }

          // Set System.out and System.err, reset trouble flag
          if (psOut) {
            jfieldID outField = env->GetStaticFieldID(systemCls, "out", "Ljava/io/PrintStream;");
            if (outField) env->SetStaticObjectField(systemCls, outField, psOut);
            if (env->ExceptionCheck()) env->ExceptionClear();
            // Reset PrintStream.trouble to false (may have been set during setup)
            jfieldID troubleF = env->GetFieldID(psCls, "trouble", "Z");
            if (env->ExceptionCheck()) { env->ExceptionClear(); troubleF = nullptr; }
            if (troubleF) {
              env->SetBooleanField(psOut, troubleF, JNI_FALSE);
              fprintf(stderr, "[dalvikvm] System.out set OK (trouble reset)\n");
            } else {
              fprintf(stderr, "[dalvikvm] System.out set OK\n");
            }
          }
          // Register Utf8Writer.nativeWrite JNI method
          {
            jclass uwRegCls = env->FindClass("art/io/Utf8Writer");
            if (env->ExceptionCheck()) { env->ExceptionClear(); uwRegCls = nullptr; }
            if (uwRegCls) {
              JNINativeMethod nm = {"nativeWrite", "(I[BII)I",
                  (void*)Java_art_io_Utf8Writer_nativeWrite};
              if (env->RegisterNatives(uwRegCls, &nm, 1) == 0) {
                fprintf(stderr, "[dalvikvm] Registered Utf8Writer.nativeWrite\n");
              } else {
                env->ExceptionClear();
              }
            }
          }
          // Patch Thread.clone() to return 'this' instead of throwing.
          // A15's ThreadLocal code path calls Thread.clone() → CloneNotSupportedException.
          // Fix: replace the first DEX instruction with 'return-object p0'.
          {
            ScopedObjectAccess soa(art::Thread::Current());
            art::ObjPtr<art::mirror::Class> threadClass =
                art::Runtime::Current()->GetClassLinker()->FindSystemClass(
                    soa.Self(), "Ljava/lang/Thread;");
            if (threadClass != nullptr) {
              for (art::ArtMethod& m : threadClass->GetDeclaredVirtualMethods(art::kRuntimePointerSize)) {
                if (strcmp(m.GetName(), "clone") == 0 && m.HasCodeItem()) {
                  art::CodeItemDataAccessor accessor(m.DexInstructionData());
                  uint16_t num_regs = accessor.RegistersSize();
                  uint16_t num_ins = accessor.InsSize();
                  uint16_t this_reg = num_regs - num_ins;
                  uint16_t* insns = const_cast<uint16_t*>(accessor.Insns());
                  // Make the page writable to patch the bytecode
                  uintptr_t page = reinterpret_cast<uintptr_t>(insns) & ~0xFFFUL;
                  mprotect(reinterpret_cast<void*>(page), 4096, PROT_READ | PROT_WRITE);
                  insns[0] = 0x0011 | (this_reg << 8);
                  fprintf(stderr, "[dalvikvm] Patched Thread.clone() → return-object v%d\n", this_reg);
                  break;
                }
              }
            }
            // Patch Float.toString(float) and Double.toString(double) to native
            // to bypass FloatingDecimal which uses arrays that can't be allocated.
            auto patchToNative = [&](const char* classDesc, const char* methodName,
                                     const char* shorty, void* nativeFunc) {
              fprintf(stderr, "[dalvikvm] patchToNative: looking for %s.%s\n", classDesc, methodName); fflush(stderr);
              art::StackHandleScope<1> hs(soa.Self());
              art::Handle<art::mirror::Class> cls = hs.NewHandle(
                  art::Runtime::Current()->GetClassLinker()->FindSystemClass(soa.Self(), classDesc));
              if (cls == nullptr) {
                if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
                fprintf(stderr, "[dalvikvm] DBG: class %s not found\n", classDesc);
              }
              if (cls != nullptr) {
                // Prevent GC during method iteration (ArtMethod pointers are raw)
                const char* old_cause = soa.Self()->StartAssertNoThreadSuspension("patchToNative");
                int total = 0, matched = 0;
                auto methods = cls->GetDeclaredMethods(art::kRuntimePointerSize);
                // Match by name only — GetShorty crashes due to GC moving DexCache
                art::ArtMethod* found = nullptr;
                for (art::ArtMethod& m : methods) {
                  total++;
                  if (strcmp(m.GetName(), methodName) == 0 && !m.IsNative()) {
                    found = &m; matched++; break;
                  }
                }
                soa.Self()->EndAssertNoThreadSuspension(old_cause);
                if (found) {
                  found->SetAccessFlags(found->GetAccessFlags() | art::kAccNative);
                  found->SetEntryPointFromJni(nativeFunc);
                  fprintf(stderr, "[dalvikvm] Patched %s.%s → native\n", classDesc, methodName);
                }
                if (matched == 0) {
                  fprintf(stderr, "[dalvikvm] DBG: %s.%s NOT FOUND (%d total methods)\n",
                          classDesc, methodName, total);
                }
              }
              if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
            };
            fprintf(stderr, "[dalvikvm] About to call patchToNative for Float.toString...\n"); fflush(stderr);
            patchToNative("Ljava/lang/Float;", "toString", "LF",
                (void*)Java_java_lang_Float_toStringImpl);
            patchToNative("Ljava/lang/Double;", "toString", "LD",
                (void*)Java_java_lang_Double_toStringImpl);
            // Stub SocProperties/TelephonyProperties to break circular class init
            // that causes StackOverflow during Build.<clinit>.
            auto patchToNativeBySig = [&](const char* classDesc, const char* methodName,
                                          const char* sig, void* nativeFunc) {
              art::StackHandleScope<1> hs2(soa.Self());
              art::Handle<art::mirror::Class> cls2 = hs2.NewHandle(
                  art::Runtime::Current()->GetClassLinker()->FindSystemClass(soa.Self(), classDesc));
              if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
              if (cls2 != nullptr) {
                const char* oc = soa.Self()->StartAssertNoThreadSuspension("patchBySig");
                for (art::ArtMethod& m : cls2->GetDeclaredMethods(art::kRuntimePointerSize)) {
                  if (strcmp(m.GetName(), methodName) == 0 &&
                      m.GetSignature().ToString() == sig && !m.IsNative()) {
                    m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
                    m.SetEntryPointFromJni(nativeFunc);
                    soa.Self()->EndAssertNoThreadSuspension(oc);
                    fprintf(stderr, "[dalvikvm] Patched %s.%s → native\n", classDesc, methodName);
                    oc = nullptr;
                    break;
                  }
                }
                if (oc) soa.Self()->EndAssertNoThreadSuspension(oc);
              }
              if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
            };
            patchToNativeBySig("Landroid/sysprop/SocProperties;", "soc_manufacturer",
                "()Ljava/util/Optional;", (void*)SocProperties_soc_manufacturer);
            patchToNativeBySig("Landroid/sysprop/SocProperties;", "soc_model",
                "()Ljava/util/Optional;", (void*)SocProperties_soc_model);
            patchToNativeBySig("Landroid/sysprop/TelephonyProperties;", "baseband_version",
                "()Ljava/util/List;", (void*)TelephonyProperties_baseband_version);

            // Also patch lastIndexOf(int,int) to handle null 'this'
            // The interpreter has a vreg corruption bug where the receiver becomes null
            static auto lastIdxII = +[](JNIEnv* env, jobject thiz, jint ch, jint fromIndex) -> jint {
              if (!thiz) return -1; // null guard
              jint len = env->GetStringLength((jstring)thiz);
              if (ch < 0x10000) {
                const jchar* chars = env->GetStringChars((jstring)thiz, nullptr);
                if (!chars) return -1;
                jint result = -1;
                for (jint i = (fromIndex < len ? fromIndex : len - 1); i >= 0; i--) {
                  if (chars[i] == (jchar)ch) { result = i; break; }
                }
                env->ReleaseStringChars((jstring)thiz, chars);
                return result;
              }
              jchar hi = (jchar)((ch >> 10) + 0xD7C0);
              jchar lo = (jchar)((ch & 0x3FF) + 0xDC00);
              const jchar* chars = env->GetStringChars((jstring)thiz, nullptr);
              if (!chars) return -1;
              jint result = -1;
              for (jint i = (fromIndex < len - 1 ? fromIndex : len - 2); i >= 0; i--) {
                if (chars[i] == hi && chars[i+1] == lo) { result = i; break; }
              }
              env->ReleaseStringChars((jstring)thiz, chars);
              return result;
            };
            // Patch both lastIndexOf(I) and lastIndexOf(II) via name match
            {
              art::StackHandleScope<1> lihs(soa.Self());
              art::Handle<art::mirror::Class> strCls2 = lihs.NewHandle(
                  art::Runtime::Current()->GetClassLinker()->FindSystemClass(
                      soa.Self(), "Ljava/lang/String;"));
              if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
              if (strCls2 != nullptr) {
                for (art::ArtMethod& m : strCls2->GetDeclaredMethods(art::kRuntimePointerSize)) {
                  if (strcmp(m.GetName(), "lastIndexOf") == 0 && !m.IsNative()) {
                    m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
                    m.SetEntryPointFromJni(reinterpret_cast<void*>(+lastIdxII));
                  }
                }
                fprintf(stderr, "[dalvikvm] Patched ALL String.lastIndexOf → native\n");
              }
              if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
            }

            // Stub LocaleUtils.toLowerString — returns input unchanged
            static auto toLowerStub = +[](JNIEnv* env, jclass, jstring s) -> jstring {
              return s ? s : env->NewStringUTF("");
            };
            patchToNativeBySig("Lsun/util/locale/LocaleUtils;", "toLowerString",
                "(Ljava/lang/String;)Ljava/lang/String;", (void*)+toLowerStub);
            patchToNativeBySig("Lsun/util/locale/LocaleUtils;", "toUpperString",
                "(Ljava/lang/String;)Ljava/lang/String;", (void*)+toLowerStub);

            // Re-run clinits — PHASE 1: Unsafe + ConcurrentHashMap
            // Must fix ConcurrentHashMap.U BEFORE re-initing Locale (which triggers LocaleList)
            {
              const char* phase1[] = {
                "Lsun/misc/Unsafe;",
                "Ljdk/internal/misc/Unsafe;",
                "Ljava/util/concurrent/ConcurrentHashMap;",
                nullptr
              };
              for (int i = 0; phase1[i]; i++) {
                art::StackHandleScope<1> rhs(soa.Self());
                art::Handle<art::mirror::Class> rc = rhs.NewHandle(
                    art::Runtime::Current()->GetClassLinker()->FindSystemClass(
                        soa.Self(), phase1[i]));
                if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
                if (rc != nullptr && rc->IsInitialized()) {
                  rc->SetStatusForPrimitiveOrArray(art::ClassStatus::kVerified);
                  bool ok = art::Runtime::Current()->GetClassLinker()->EnsureInitialized(
                      soa.Self(), rc, true, true);
                  if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
                  fprintf(stderr, "[dalvikvm] Re-init %s: %s\n", phase1[i],
                          ok ? "OK" : "FAILED (tolerated)");
                }
              }
            }

            // Fix ConcurrentHashMap.U + field offsets BEFORE Locale re-init
            {
              PatchConcurrentHashMapUnsafe(env, "early, before Locale re-init");
            }

            // Re-run clinits — PHASE 2: Locale helpers + Configuration
            // SKIP Locale itself — our JNI_OnLoad_framework pre-sets ROOT/US/ENGLISH,
            // and re-init via SetStatusForPrimitiveOrArray leaves internal state inconsistent
            // (causes FATAL "Unexpected change back of class status" later).
            {
              const char* phase2[] = {
                "Lsun/util/locale/LocaleObjectCache;",
                "Lsun/util/locale/BaseLocale;",
                "Lsun/util/locale/BaseLocale$Cache;",
                // "Ljava/util/Locale;",  — DO NOT re-init (causes FATAL later)
                // "Landroid/content/res/Configuration;",  — DO NOT re-init (same FATAL)
                nullptr
              };
              for (int i = 0; phase2[i]; i++) {
                art::StackHandleScope<1> rhs(soa.Self());
                art::Handle<art::mirror::Class> rc = rhs.NewHandle(
                    art::Runtime::Current()->GetClassLinker()->FindSystemClass(
                        soa.Self(), phase2[i]));
                if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
                if (rc != nullptr && rc->IsInitialized()) {
                  rc->SetStatusForPrimitiveOrArray(art::ClassStatus::kVerified);
                  bool ok = art::Runtime::Current()->GetClassLinker()->EnsureInitialized(
                      soa.Self(), rc, true, true);
                  if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
                  fprintf(stderr, "[dalvikvm] Re-init %s: %s\n", phase2[i],
                          ok ? "OK" : "FAILED (tolerated)");
                }
              }
            }

            // Pre-set LocaleList static fields to bypass broken clinit
            {
              jclass llCls = env->FindClass("android/os/LocaleList");
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (llCls) {
                // Create empty LocaleList via AllocObject
                jobject emptyLL = env->AllocObject(llCls);
                if (env->ExceptionCheck()) env->ExceptionClear();
                // Create US LocaleList
                jobject usLL = env->AllocObject(llCls);
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (emptyLL && usLL) {
                  jfieldID emptyF = env->GetStaticFieldID(llCls, "sEmptyLocaleList", "Landroid/os/LocaleList;");
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (emptyF) env->SetStaticObjectField(llCls, emptyF, emptyLL);

                  jfieldID defF = env->GetStaticFieldID(llCls, "sDefaultLocaleList", "Landroid/os/LocaleList;");
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (defF) env->SetStaticObjectField(llCls, defF, usLL);

                  jfieldID adjF = env->GetStaticFieldID(llCls, "sDefaultAdjustedLocaleList", "Landroid/os/LocaleList;");
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (adjF) env->SetStaticObjectField(llCls, adjF, usLL);

                  jfieldID lastF = env->GetStaticFieldID(llCls, "sLastExplicitlySetLocaleList", "Landroid/os/LocaleList;");
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (lastF) env->SetStaticObjectField(llCls, lastF, usLL);

                  // Set mList arrays so instances are usable
                  jfieldID mListF = env->GetFieldID(llCls, "mList", "[Ljava/util/Locale;");
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (mListF) {
                    jclass localeCls = env->FindClass("java/util/Locale");
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    if (localeCls) {
                      // Empty array for sEmptyLocaleList
                      jobjectArray emptyArr = env->NewObjectArray(0, localeCls, nullptr);
                      if (env->ExceptionCheck()) env->ExceptionClear();
                      if (emptyArr) env->SetObjectField(emptyLL, mListF, emptyArr);

                      // [Locale.US] array for default lists
                      jfieldID usLocF = env->GetStaticFieldID(localeCls, "US", "Ljava/util/Locale;");
                      if (env->ExceptionCheck()) env->ExceptionClear();
                      jobject locUS = usLocF ? env->GetStaticObjectField(localeCls, usLocF) : nullptr;
                      if (env->ExceptionCheck()) env->ExceptionClear();
                      if (locUS) {
                        jobjectArray usArr = env->NewObjectArray(1, localeCls, locUS);
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        if (usArr) env->SetObjectField(usLL, mListF, usArr);
                      }
                    }
                  }
                  fprintf(stderr, "[dalvikvm] Pre-set LocaleList static fields + mList\n");
                }
              }
              if (env->ExceptionCheck()) env->ExceptionClear();

              // Mark LocaleList as initialized so its broken clinit never runs
              {
                art::ObjPtr<art::mirror::Class> llMirror =
                    art::Runtime::Current()->GetClassLinker()->FindSystemClass(
                        soa.Self(), "Landroid/os/LocaleList;");
                if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
                if (llMirror != nullptr) {
                  llMirror->SetStatusForPrimitiveOrArray(art::ClassStatus::kInitialized);
                  fprintf(stderr, "[dalvikvm] Force-marked LocaleList as kInitialized\n");
                }
              }
            }

            // Stub UUID.randomUUID() — SecureRandom not available
            {
              art::ObjPtr<art::mirror::Class> uuidCls =
                  art::Runtime::Current()->GetClassLinker()->FindSystemClass(
                      soa.Self(), "Ljava/util/UUID;");
              if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
              if (uuidCls != nullptr) {
                for (art::ArtMethod& m : uuidCls->GetDeclaredMethods(art::kRuntimePointerSize)) {
                  if (strcmp(m.GetName(), "randomUUID") == 0 && !m.IsNative()) {
                    m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
                    m.SetEntryPointFromJni(
                        reinterpret_cast<void*>(Java_java_util_UUID_randomUUID_stub));
                    fprintf(stderr, "[dalvikvm] Patched UUID.randomUUID() → native stub\n");
                    break;
                  }
                }
              }
              if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
            }

            // Stub SecureRandom.nextBytes() — no crypto provider
            {
              art::ObjPtr<art::mirror::Class> srCls =
                  art::Runtime::Current()->GetClassLinker()->FindSystemClass(
                      soa.Self(), "Ljava/security/SecureRandom;");
              if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
              if (srCls != nullptr) {
                for (art::ArtMethod& m : srCls->GetDeclaredMethods(art::kRuntimePointerSize)) {
                  if (strcmp(m.GetName(), "nextBytes") == 0 &&
                      m.GetSignature().ToString() == "([B)V" && !m.IsNative()) {
                    m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
                    m.SetEntryPointFromJni(
                        reinterpret_cast<void*>(Java_java_security_SecureRandom_nextBytes_stub));
                    fprintf(stderr, "[dalvikvm] Patched SecureRandom.nextBytes() → native stub\n");
                    break;
                  }
                }
              }
              if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
            }

            // Replace TextUtils.formatSimple with String.format delegation.
            // The built-in only handles %s/%d and crashes on %x, %08x, etc.
            // Note: can't use patchToNative because GetShorty() returns wrong value
            // for non-boot-image methods. Match by name + signature instead.
            {
              art::ObjPtr<art::mirror::Class> txCls =
                  art::Runtime::Current()->GetClassLinker()->FindSystemClass(
                      soa.Self(), "Landroid/text/TextUtils;");
              if (txCls != nullptr) {
                for (art::ArtMethod& m : txCls->GetDeclaredMethods(art::kRuntimePointerSize)) {
                  if (strcmp(m.GetName(), "formatSimple") == 0 &&
                      m.GetSignature().ToString() ==
                          "(Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/String;" &&
                      !m.IsNative()) {
                    m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
                    m.SetEntryPointFromJni(
                        reinterpret_cast<void*>(Java_android_text_TextUtils_formatSimple));
                    fprintf(stderr, "[dalvikvm] Patched TextUtils.formatSimple → native\n");
                    break;
                  }
                }
              }
              if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
            }
            if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
            // Patch Throwable.printStackTrace() to no-op — prevents infinite loops when
            // JNI ExceptionDescribe calls printStackTrace which triggers more exceptions.
            {
              art::ObjPtr<art::mirror::Class> throwableCls =
                  art::Runtime::Current()->GetClassLinker()->FindSystemClass(
                      soa.Self(), "Ljava/lang/Throwable;");
              if (throwableCls != nullptr) {
                for (art::ArtMethod& m : throwableCls->GetDeclaredMethods(art::kRuntimePointerSize)) {
                  if (strcmp(m.GetName(), "printStackTrace") == 0 &&
                      m.GetSignature().ToString() == "()V" && !m.IsNative()) {
                    m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
                    m.SetEntryPointFromJni(
                        reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
                    fprintf(stderr, "[dalvikvm] Patched Throwable.printStackTrace() → no-op\n");
                    break;
                  }
                }
              }
              if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
            }
            // Patch String.lastIndexOf(int) bytecode — the interpreter has a
            // register corruption bug where length() clobbers the ch parameter.
            // Fix: save ch to v0 before calling length, restore after.
            // Original:   length→v0, v0=v0-1, lastIndexOf(v1,v2,v0)
            // Patched:    length→v0, v0=v0-1, lastIndexOf(v1,v2,v0) [same but with v2 protected]
            // Actually: just replace the whole method with one that calls lastIndexOf(II)
            // using only registers that don't conflict.
            //
            // New bytecode for lastIndexOf(I)I (regs=3, ins=2: v1=this, v2=ch):
            // 0: move v0, v2          ; save ch to v0
            // 1: invoke-virtual {v1}, length()I
            // 4: move-result v2       ; v2 = length (overwrites ch)
            // 5: add-int/lit8 v2, v2, -1  ; v2 = length - 1
            // 7: invoke-virtual {v1, v0, v2}, lastIndexOf(II)I  ; v0=ch(saved), v2=fromIndex
            // a: move-result v0
            // b: return v0
            {
              art::ObjPtr<art::mirror::Class> strCls =
                  art::Runtime::Current()->GetClassLinker()->FindSystemClass(
                      soa.Self(), "Ljava/lang/String;");
              if (strCls != nullptr) {
                for (art::ArtMethod& m : strCls->GetDeclaredMethods(art::kRuntimePointerSize)) {
                  if (strcmp(m.GetName(), "lastIndexOf") == 0 &&
                      m.GetSignature().ToString() == "(I)I" && !m.IsNative()) {
                    // Patch the vtable entry to point to our patched ArtMethod
                    m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
                    m.SetEntryPointFromJni(
                        reinterpret_cast<void*>(Java_java_lang_String_lastIndexOf_native));
                    // entry point already set by VisitPackedArtMethods
                    // Also patch via vtable
                    art::ObjPtr<art::mirror::PointerArray> vtable = strCls->GetVTableDuringLinking();
                    if (vtable != nullptr) {
                      int vtable_count = vtable->GetLength();
                      for (int vi = 0; vi < vtable_count; vi++) {
                        auto* vm = vtable->GetElementPtrSize<art::ArtMethod*>(vi, art::kRuntimePointerSize);
                        if (vm && strcmp(vm->GetName(), "lastIndexOf") == 0 &&
                            vm->GetSignature().ToString() == "(I)I") {
                          vm->SetAccessFlags(vm->GetAccessFlags() | art::kAccNative);
                          vm->SetEntryPointFromJni(
                              reinterpret_cast<void*>(Java_java_lang_String_lastIndexOf_native));
                          // entry point already set
                          fprintf(stderr, "[dalvikvm] Patched String vtable[%d] lastIndexOf(I)\n", vi);
                        }
                      }
                    }
                    fprintf(stderr, "[dalvikvm] Patched String.lastIndexOf(I) → native\n");
                    break;
                  }
                }
              }
              if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
            }
          }

          if (psErr) {
            jfieldID errField = env->GetStaticFieldID(systemCls, "err", "Ljava/io/PrintStream;");
            if (errField) env->SetStaticObjectField(systemCls, errField, psErr);
            if (env->ExceptionCheck()) env->ExceptionClear();
            fprintf(stderr, "[dalvikvm] System.err set OK\n");
          }
        }
      }
      } // end of re-found classes block
    }
    // Null out IoTracker on System.out/err FileOutputStream to avoid BlockGuard NPE
    // The tracker field calls BlockGuard.getThreadPolicy() which crashes with boot image
    {
      jclass fosCls = env->FindClass("java/io/FileOutputStream");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (fosCls) {
        jfieldID trackerF = env->GetFieldID(fosCls, "tracker", "Llibcore/io/IoTracker;");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (trackerF) {
          // Get the FileOutputStream from System.out's underlying stream
          jclass psCls2 = env->FindClass("java/io/PrintStream");
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (psCls2 && systemCls) {
            jfieldID outF2 = env->GetStaticFieldID(systemCls, "out", "Ljava/io/PrintStream;");
            jfieldID errF2 = env->GetStaticFieldID(systemCls, "err", "Ljava/io/PrintStream;");
            if (env->ExceptionCheck()) env->ExceptionClear();
            // PrintStream has a field 'out' (inherited from FilterOutputStream)
            jfieldID filterOut = env->GetFieldID(psCls2, "out", "Ljava/io/OutputStream;");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (filterOut && outF2) {
              jobject ps = env->GetStaticObjectField(systemCls, outF2);
              if (ps) {
                jobject fos = env->GetObjectField(ps, filterOut);
                if (fos && env->IsInstanceOf(fos, fosCls)) {
                  env->SetObjectField(fos, trackerF, nullptr);
                }
              }
              if (env->ExceptionCheck()) env->ExceptionClear();
            }
            if (filterOut && errF2) {
              jobject ps = env->GetStaticObjectField(systemCls, errF2);
              if (ps) {
                jobject fos = env->GetObjectField(ps, filterOut);
                if (fos && env->IsInstanceOf(fos, fosCls)) {
                  env->SetObjectField(fos, trackerF, nullptr);
                }
              }
              if (env->ExceptionCheck()) env->ExceptionClear();
            }
          }
          fprintf(stderr, "[dalvikvm] Nulled IoTracker on System.out/err\n");
        }
      }
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    fflush(stderr);
  }
  // Patch Float.toString(float) and Double.toString(double) to native C.
  // FloatingDecimal uses inner class arrays that can't be allocated by the A15 interpreter.
  {
    ScopedObjectAccess soa(art::Thread::Current());
    auto patchToNative = [&](const char* classDesc, const char* methodName,
                             const char* shorty, void* nativeFunc) {
      art::ObjPtr<art::mirror::Class> cls =
          art::Runtime::Current()->GetClassLinker()->FindSystemClass(soa.Self(), classDesc);
      if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
      if (cls == nullptr) return;
      for (art::ArtMethod& m : cls->GetDeclaredMethods(art::kRuntimePointerSize)) {
        if (strcmp(m.GetName(), methodName) == 0 && !m.IsNative()) {
          (void)shorty;
          m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(nativeFunc);
          fprintf(stderr, "[dalvikvm] Patched %s.%s → native\n", classDesc, methodName);
          break;
        }
      }
      if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
    };
    patchToNative("Ljava/lang/Float;", "toString", "LF",
        (void*)Java_java_lang_Float_toStringImpl);
    patchToNative("Ljava/lang/Double;", "toString", "LD",
        (void*)Java_java_lang_Double_toStringImpl);
    // Patch AbstractStringBuilder.append(float) and append(double) to use
    // String.valueOf instead of FloatingDecimal.appendTo (which uses broken ThreadLocal).
    // We make them native: append(float f) → append(Float.toString(f))
    // But since Float.toString IS now native, it returns a proper string.
    // Simplest: just make FloatingDecimal.appendTo a no-op (return void).
    // The append(float) will still call appendTo but it won't crash.
    // The number won't be formatted but at least no crash.
    // Actually better: patch AbstractStringBuilder.append(float) bytecode to call
    // append(String.valueOf(float)) instead of FloatingDecimal.appendTo.
    // Too complex. Just make appendTo a no-op — the float won't print but no crash.

    // Patch FloatingDecimal.appendTo to return-void immediately
    {
      art::ObjPtr<art::mirror::Class> fdCls =
          art::Runtime::Current()->GetClassLinker()->FindSystemClass(
              soa.Self(), "Ljdk/internal/math/FloatingDecimal;");
      if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
      if (fdCls != nullptr) {
        for (art::ArtMethod& m : fdCls->GetDeclaredMethods(art::kRuntimePointerSize)) {
          if (strcmp(m.GetName(), "appendTo") == 0 && m.HasCodeItem()) {
            art::CodeItemDataAccessor accessor(m.DexInstructionData());
            uint16_t* insns = const_cast<uint16_t*>(accessor.Insns());
            uintptr_t page = reinterpret_cast<uintptr_t>(insns) & ~0xFFFUL;
            mprotect(reinterpret_cast<void*>(page), 4096, PROT_READ | PROT_WRITE);
            insns[0] = 0x000e; // return-void
            fprintf(stderr, "[dalvikvm] Patched FloatingDecimal.appendTo → return-void\n");
            // Don't break — patch ALL overloads (float and double)
          }
        }
        if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
      }
    }
  }

  // Patch StringBuilder.append(boolean) to return-object this (skip boolean value).
  // A15's append(boolean) uses internal arrays that can't be allocated.
  {
    ScopedObjectAccess soa(art::Thread::Current());
    art::ObjPtr<art::mirror::Class> sbCls =
        art::Runtime::Current()->GetClassLinker()->FindSystemClass(
            soa.Self(), "Ljava/lang/StringBuilder;");
    if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
    // Patch append(boolean) using JNI method lookup for exact signature match
    {
      jclass sbJni = env->FindClass("java/lang/StringBuilder");
      jclass asbJni = env->FindClass("java/lang/AbstractStringBuilder");
      if (env->ExceptionCheck()) env->ExceptionClear();
      // Get the exact append(Z) method via JNI
      jmethodID sbAppendZ = sbJni ? env->GetMethodID(sbJni, "append", "(Z)Ljava/lang/StringBuilder;") : nullptr;
      if (env->ExceptionCheck()) env->ExceptionClear();
      jmethodID asbAppendZ = asbJni ? env->GetMethodID(asbJni, "append", "(Z)Ljava/lang/AbstractStringBuilder;") : nullptr;
      if (env->ExceptionCheck()) env->ExceptionClear();
      // Convert jmethodID to ArtMethod* and patch bytecode
      auto patchMethod = [&](jmethodID mid, const char* desc) {
        if (!mid) return;
        art::ArtMethod* am = art::jni::DecodeArtMethod(mid);
        if (am && am->HasCodeItem()) {
          art::CodeItemDataAccessor accessor(am->DexInstructionData());
          uint16_t this_reg = accessor.RegistersSize() - accessor.InsSize();
          uint16_t* insns = const_cast<uint16_t*>(accessor.Insns());
          uintptr_t page = reinterpret_cast<uintptr_t>(insns) & ~0xFFFUL;
          mprotect(reinterpret_cast<void*>(page), 4096, PROT_READ | PROT_WRITE);
          insns[0] = 0x0011 | (this_reg << 8);
          fprintf(stderr, "[dalvikvm] Patched %s.append(Z) → return-object v%d\n", desc, this_reg);
        }
      };
      patchMethod(sbAppendZ, "StringBuilder");
      patchMethod(asbAppendZ, "AbstractStringBuilder");
    }
  }

  // Clear Thread.threadLocals to force fresh ThreadLocalMap creation.
  // The boot image may contain a corrupt ThreadLocalMap from dex2oat's thread
  // that has length-0 table after relocation.
  {
    jclass threadCls = env->FindClass("java/lang/Thread");
    if (env->ExceptionCheck()) { env->ExceptionClear(); threadCls = nullptr; }
    if (threadCls) {
      jmethodID currentThread = env->GetStaticMethodID(threadCls, "currentThread",
          "()Ljava/lang/Thread;");
      if (env->ExceptionCheck()) { env->ExceptionClear(); currentThread = nullptr; }
      jfieldID tlField = env->GetFieldID(threadCls, "threadLocals",
          "Ljava/lang/ThreadLocal$ThreadLocalMap;");
      if (env->ExceptionCheck()) { env->ExceptionClear(); tlField = nullptr; }
      if (currentThread && tlField) {
        jobject ct = env->CallStaticObjectMethod(threadCls, currentThread);
        if (env->ExceptionCheck()) { env->ExceptionClear(); ct = nullptr; }
        if (ct) {
          env->SetObjectField(ct, tlField, nullptr);
          if (env->ExceptionCheck()) env->ExceptionClear();
          fprintf(stderr, "[dalvikvm] Cleared Thread.threadLocals\n");
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Route ThreadLocal.nextHashCode() to a simple native counter before any
  // Looper/BlockGuard path gets a chance to touch it. The previous best-effort
  // ArtMethod mutation was too soft and still allowed Java-side nextHashCode()
  // to run with a null AtomicInteger on standalone boot.
  ForceStandaloneNativeEntryPoint("Ljava/lang/ThreadLocal;",
                                  "nextHashCode",
                                  "()I",
                                  reinterpret_cast<void*>(
                                      Java_java_lang_ThreadLocal_nextHashCode_native));
  if (art::Thread::Current()->IsExceptionPending()) {
    art::Thread::Current()->ClearException();
  }

  // Seed ThreadLocal.nextHashCode with a fresh AtomicInteger.
  // The relocated boot image sometimes leaves this static field null, which
  // makes BlockGuard/Looper/View clinits fail immediately.
  {
    jclass tlCls = env->FindClass("java/lang/ThreadLocal");
    jclass aiCls = env->FindClass("java/util/concurrent/atomic/AtomicInteger");
    if (env->ExceptionCheck()) { env->ExceptionClear(); tlCls = nullptr; aiCls = nullptr; }
    if (tlCls && aiCls) {
      jfieldID nextHashCodeF = env->GetStaticFieldID(
          tlCls, "nextHashCode", "Ljava/util/concurrent/atomic/AtomicInteger;");
      jfieldID valueF = env->GetFieldID(aiCls, "value", "I");
      if (env->ExceptionCheck()) { env->ExceptionClear(); nextHashCodeF = nullptr; valueF = nullptr; }
      if (nextHashCodeF && valueF) {
        jobject nextHashCode = env->GetStaticObjectField(tlCls, nextHashCodeF);
        if (env->ExceptionCheck()) { env->ExceptionClear(); nextHashCode = nullptr; }
        if (nextHashCode == nullptr) {
          jmethodID aiCtor = env->GetMethodID(aiCls, "<init>", "()V");
          if (env->ExceptionCheck()) { env->ExceptionClear(); aiCtor = nullptr; }
          if (aiCtor != nullptr) {
            nextHashCode = env->NewObject(aiCls, aiCtor);
            if (env->ExceptionCheck()) { env->ExceptionClear(); nextHashCode = nullptr; }
          }
          if (nextHashCode == nullptr) {
            nextHashCode = env->AllocObject(aiCls);
            if (env->ExceptionCheck()) { env->ExceptionClear(); nextHashCode = nullptr; }
          }
          if (nextHashCode != nullptr) {
            env->SetIntField(nextHashCode, valueF, 0);
            env->SetStaticObjectField(tlCls, nextHashCodeF, nextHashCode);
            if (env->ExceptionCheck()) env->ExceptionClear();
            fprintf(stderr, "[dalvikvm] Seeded ThreadLocal.nextHashCode\n");
          } else {
            fprintf(stderr, "[dalvikvm] FAILED to seed ThreadLocal.nextHashCode\n");
          }
        } else {
          fprintf(stderr, "[dalvikvm] ThreadLocal.nextHashCode already present\n");
        }
      } else {
        fprintf(stderr, "[dalvikvm] FAILED to resolve ThreadLocal.nextHashCode / AtomicInteger.value\n");
      }
    } else {
      fprintf(stderr, "[dalvikvm] FAILED to load ThreadLocal / AtomicInteger for nextHashCode seed\n");
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Force-initialize BlockGuard class so threadPolicy ThreadLocal is created
  // With verify-only boot image, <clinit> wasn't run during image generation
  {
    ScopedObjectAccess soa(art::Thread::Current());
    art::ObjPtr<art::mirror::Class> bgClass = art::Runtime::Current()->GetClassLinker()->
        FindSystemClass(art::Thread::Current(), "Ldalvik/system/BlockGuard;");
    if (bgClass != nullptr) {
      art::StackHandleScope<1> hs(art::Thread::Current());
      art::Handle<art::mirror::Class> h(hs.NewHandle(bgClass));
      art::Runtime::Current()->GetClassLinker()->EnsureInitialized(
          art::Thread::Current(), h, true, true);
      if (art::Thread::Current()->IsExceptionPending()) {
        art::Thread::Current()->ClearException();
      }
      fprintf(stderr, "[dalvikvm] BlockGuard force-initialized\n");
    }
  }
  if (env->ExceptionCheck()) env->ExceptionClear();

  // Register MessageQueue native methods (needed for Looper/Handler/ActivityThread)
  {
    jclass mqCls = env->FindClass("android/os/MessageQueue");
    if (env->ExceptionCheck()) { env->ExceptionClear(); mqCls = nullptr; }
    if (mqCls) {
      // Stub natives for MessageQueue — minimal epoll-based Looper
      static auto nativeInit = +[](JNIEnv*, jobject) -> jlong {
        int epollFd = epoll_create1(EPOLL_CLOEXEC);
        int eventFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (epollFd >= 0 && eventFd >= 0) {
          struct epoll_event ev = {}; ev.events = EPOLLIN; ev.data.fd = eventFd;
          epoll_ctl(epollFd, EPOLL_CTL_ADD, eventFd, &ev);
        }
        return (jlong)((((int64_t)epollFd) << 32) | (eventFd & 0xFFFFFFFFL));
      };
      static auto nativeDestroy = +[](JNIEnv*, jobject, jlong ptr) {
        close((int)(ptr >> 32)); close((int)(ptr & 0xFFFFFFFFL));
      };
      static auto nativePollOnce = +[](JNIEnv*, jobject, jlong ptr, jint timeoutMs) {
        struct epoll_event events[4];
        epoll_wait((int)(ptr >> 32), events, 4, timeoutMs > 0 ? timeoutMs : 0);
      };
      static auto nativeWake = +[](JNIEnv*, jobject, jlong ptr) {
        uint64_t val = 1; write((int)(ptr & 0xFFFFFFFFL), &val, sizeof(val));
      };
      static auto nativeIsPolling = +[](JNIEnv*, jobject, jlong) -> jboolean { return JNI_FALSE; };
      static auto nativeSetFdEvents = +[](JNIEnv*, jobject, jlong, jint, jint) {};

      JNINativeMethod methods[] = {
        {"nativeInit", "()J", (void*)nativeInit},
        {"nativeDestroy", "(J)V", (void*)nativeDestroy},
        {"nativePollOnce", "(JI)V", (void*)nativePollOnce},
        {"nativeWake", "(J)V", (void*)nativeWake},
        {"nativeIsPolling", "(J)Z", (void*)nativeIsPolling},
        {"nativeSetFileDescriptorEvents", "(JII)V", (void*)nativeSetFdEvents},
      };
      int rc = env->RegisterNatives(mqCls, methods, 6);
      if (env->ExceptionCheck()) env->ExceptionClear();
      fprintf(stderr, "[dalvikvm] MessageQueue natives registered (rc=%d)\n", rc);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Bypass BlockGuardOs by replacing Libcore.os with the underlying Linux
  // instance. BlockGuardOs.write() calls BlockGuard.getThreadPolicy() which
  // uses a ThreadLocal that crashes with length=0 table after boot image relocation.
  {
    jclass libcoreCls = env->FindClass("libcore/io/Libcore");
    if (env->ExceptionCheck()) { env->ExceptionClear(); libcoreCls = nullptr; }
    if (libcoreCls) {
      // Get Libcore.rawOs (the unwrapped Linux instance)
      jfieldID rawOsF = env->GetStaticFieldID(libcoreCls, "rawOs", "Llibcore/io/Os;");
      if (env->ExceptionCheck()) { env->ExceptionClear(); rawOsF = nullptr; }
      jfieldID osF = env->GetStaticFieldID(libcoreCls, "os", "Llibcore/io/Os;");
      if (env->ExceptionCheck()) { env->ExceptionClear(); osF = nullptr; }
      if (rawOsF && osF) {
        jobject rawOs = env->GetStaticObjectField(libcoreCls, rawOsF);
        if (env->ExceptionCheck()) { env->ExceptionClear(); rawOs = nullptr; }
        if (rawOs) {
          env->SetStaticObjectField(libcoreCls, osF, rawOs);
          if (env->ExceptionCheck()) env->ExceptionClear();
          fprintf(stderr, "[dalvikvm] Libcore.os = rawOs (bypass BlockGuard)\n");
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Fix ThreadLocalMap.INITIAL_CAPACITY — it gets corrupted to 0 by boot image relocation
  {
    jclass tlmCls = env->FindClass("java/lang/ThreadLocal$ThreadLocalMap");
    if (env->ExceptionCheck()) { env->ExceptionClear(); tlmCls = nullptr; }
    if (tlmCls) {
      jfieldID icF = env->GetStaticFieldID(tlmCls, "INITIAL_CAPACITY", "I");
      if (env->ExceptionCheck()) { env->ExceptionClear(); icF = nullptr; }
      if (icF) {
        jint val = env->GetStaticIntField(tlmCls, icF);
        fprintf(stderr, "[dalvikvm] ThreadLocalMap.INITIAL_CAPACITY = %d\n", val);
        if (val != 16) {
          env->SetStaticIntField(tlmCls, icF, 16);
          fprintf(stderr, "[dalvikvm] Fixed INITIAL_CAPACITY to 16\n");
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Clear Thread.threadLocals RIGHT before main() to ensure no corrupt
  // ThreadLocalMap from boot image or setup code persists.
  {
    jclass threadCls2 = env->FindClass("java/lang/Thread");
    if (env->ExceptionCheck()) { env->ExceptionClear(); threadCls2 = nullptr; }
    if (threadCls2) {
      jmethodID ct2 = env->GetStaticMethodID(threadCls2, "currentThread", "()Ljava/lang/Thread;");
      jfieldID tlf2 = env->GetFieldID(threadCls2, "threadLocals",
          "Ljava/lang/ThreadLocal$ThreadLocalMap;");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (ct2 && tlf2) {
        jobject t2 = env->CallStaticObjectMethod(threadCls2, ct2);
        if (t2 && !env->ExceptionCheck()) {
          env->SetObjectField(t2, tlf2, nullptr);
          fprintf(stderr, "[dalvikvm] Cleared threadLocals before main()\n");
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Create fresh PrintStream(NativeOutputStream) right before main().
  // This bypasses the broken FileOutputStream/IoTracker/ThreadLocal chain entirely.
  {
    jclass nosCls = env->FindClass("art/io/NativeOutputStream");
    if (env->ExceptionCheck()) { env->ExceptionClear(); nosCls = nullptr; }
    jclass psCls3 = env->FindClass("java/io/PrintStream");
    jclass sysCls3 = env->FindClass("java/lang/System");
    if (nosCls && psCls3 && sysCls3) {
      jmethodID nosInit = env->GetMethodID(nosCls, "<init>", "(I)V");
      jmethodID psInit3 = env->GetMethodID(psCls3, "<init>", "(Ljava/io/OutputStream;)V");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (nosInit && psInit3) {
        // stdout
        jobject nos1 = env->NewObject(nosCls, nosInit, 1);
        if (nos1 && !env->ExceptionCheck()) {
          jobject ps1 = env->NewObject(psCls3, psInit3, nos1);
          if (ps1 && !env->ExceptionCheck()) {
            // Set textOut to Utf8Writer(NativeOutputStream)
            jclass uwCls2 = env->FindClass("art/io/Utf8Writer");
            jclass bwCls2 = env->FindClass("java/io/BufferedWriter");
            if (uwCls2 && bwCls2) {
              jmethodID uwI = env->GetMethodID(uwCls2, "<init>", "(Ljava/io/OutputStream;)V");
              jmethodID bwI = env->GetMethodID(bwCls2, "<init>", "(Ljava/io/Writer;I)V");
              jmethodID setFdM2 = env->GetMethodID(uwCls2, "setFd", "(I)V");
              jfieldID toF = env->GetFieldID(psCls3, "textOut", "Ljava/io/BufferedWriter;");
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (uwI && bwI && toF) {
                jobject uw2 = env->NewObject(uwCls2, uwI, nos1);
                if (uw2 && setFdM2) env->CallVoidMethod(uw2, setFdM2, 1);
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (uw2) {
                  jobject bw2 = env->NewObject(bwCls2, bwI, uw2, (jint)8192);
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (bw2) env->SetObjectField(ps1, toF, bw2);
                }
              }
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            // Set charOut to the same object as textOut (our BufferedWriter).
            // PrintStream.writeln calls charOut.flushBuffer() which we need to succeed.
            {
              jfieldID charOutF3 = env->GetFieldID(psCls3, "charOut", "Ljava/io/OutputStreamWriter;");
              if (env->ExceptionCheck()) { env->ExceptionClear(); charOutF3 = nullptr; }
              jfieldID toF3 = env->GetFieldID(psCls3, "textOut", "Ljava/io/BufferedWriter;");
              if (env->ExceptionCheck()) { env->ExceptionClear(); toF3 = nullptr; }
              if (charOutF3 && toF3) {
                // BufferedWriter IS-NOT-A OutputStreamWriter, but we can assign it
                // because the JNI SetObjectField doesn't type-check.
                // charOut.flushBuffer() → BufferedWriter.flushBuffer() → Utf8Writer.write → nativeWrite
                jobject textOut3 = env->GetObjectField(ps1, toF3);
                if (textOut3) env->SetObjectField(ps1, charOutF3, textOut3);
              }
              if (env->ExceptionCheck()) env->ExceptionClear();
            }
            jfieldID sysOutF = env->GetStaticFieldID(sysCls3, "out", "Ljava/io/PrintStream;");
            if (sysOutF) env->SetStaticObjectField(sysCls3, sysOutF, ps1);
            fprintf(stderr, "[dalvikvm] System.out = PrintStream(NativeOutputStream(1))\n");
          }
        }
        // stderr
        jobject nos2 = env->NewObject(nosCls, nosInit, 2);
        if (nos2 && !env->ExceptionCheck()) {
          jobject ps2 = env->NewObject(psCls3, psInit3, nos2);
          if (ps2 && !env->ExceptionCheck()) {
            jfieldID sysErrF = env->GetStaticFieldID(sysCls3, "err", "Ljava/io/PrintStream;");
            if (sysErrF) env->SetStaticObjectField(sysCls3, sysErrF, ps2);
          }
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Reset trouble on the final System.out
  {
    jclass sysCls = env->FindClass("java/lang/System");
    jclass psCls2 = env->FindClass("java/io/PrintStream");
    if (sysCls && psCls2) {
      jfieldID outF = env->GetStaticFieldID(sysCls, "out", "Ljava/io/PrintStream;");
      jfieldID errF = env->GetStaticFieldID(sysCls, "err", "Ljava/io/PrintStream;");
      jfieldID troubleF2 = env->GetFieldID(psCls2, "trouble", "Z");
      if (outF && troubleF2) {
        jobject sysOut = env->GetStaticObjectField(sysCls, outF);
        if (sysOut) env->SetBooleanField(sysOut, troubleF2, JNI_FALSE);
      }
      if (errF && troubleF2) {
        jobject sysErr = env->GetStaticObjectField(sysCls, errF);
        if (sysErr) env->SetBooleanField(sysErr, troubleF2, JNI_FALSE);
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }
  // Pre-create ThreadLocalMap for the main thread manually via JNI.
  // The interpreter's new-array for Entry[] fails, but JNI NewObjectArray works.
  // So we build the ThreadLocalMap from JNI and attach it to the main thread.
  {
    jclass threadCls3 = env->FindClass("java/lang/Thread");
    jclass tlmCls = env->FindClass("java/lang/ThreadLocal$ThreadLocalMap");
    jclass entryCls = env->FindClass("java/lang/ThreadLocal$ThreadLocalMap$Entry");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (threadCls3 && tlmCls && entryCls) {
      jmethodID ctm = env->GetStaticMethodID(threadCls3, "currentThread", "()Ljava/lang/Thread;");
      jfieldID tlf = env->GetFieldID(threadCls3, "threadLocals", "Ljava/lang/ThreadLocal$ThreadLocalMap;");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (ctm && tlf) {
        jobject ct = env->CallStaticObjectMethod(threadCls3, ctm);
        if (ct && !env->ExceptionCheck()) {
          env->SetObjectField(ct, tlf, nullptr); // clear stale
          // Create ThreadLocalMap via AllocObject (skip constructor)
          jobject tlm = env->AllocObject(tlmCls);
          if (tlm && !env->ExceptionCheck()) {
            // Set table = new Entry[16] via JNI
            jobjectArray table = env->NewObjectArray(16, entryCls, nullptr);
            if (table && !env->ExceptionCheck()) {
              jfieldID tableF = env->GetFieldID(tlmCls, "table",
                  "[Ljava/lang/ThreadLocal$ThreadLocalMap$Entry;");
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (tableF) {
                env->SetObjectField(tlm, tableF, table);
                // Set threshold and size
                jfieldID threshF = env->GetFieldID(tlmCls, "threshold", "I");
                jfieldID sizeF = env->GetFieldID(tlmCls, "size", "I");
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (threshF) env->SetIntField(tlm, threshF, 10); // 2/3 of 16
                if (sizeF) env->SetIntField(tlm, sizeF, 0);
                // Attach to thread
                env->SetObjectField(ct, tlf, tlm);
                fprintf(stderr, "[dalvikvm] Pre-created ThreadLocalMap with Entry[16] table\n");
              }
            }
          }
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Late-patch sysprop methods right before main() — after all class loading
  // so the entries don't get overwritten by ClassLinker.
  {
    art::ScopedObjectAccess soa(art::Thread::Current());
    auto latePatch = [&](const char* classDesc, const char* methodName,
                         const char* sig, void* nativeFunc) {
      art::ObjPtr<art::mirror::Class> cls =
          art::Runtime::Current()->GetClassLinker()->FindSystemClass(soa.Self(), classDesc);
      if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
      if (cls == nullptr) return;
      for (art::ArtMethod& m : cls->GetDeclaredMethods(art::kRuntimePointerSize)) {
        if (strcmp(m.GetName(), methodName) == 0 &&
            m.GetSignature().ToString() == sig) {
          if (!m.IsNative()) m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(nativeFunc);
          fprintf(stderr, "[dalvikvm] Late-patched %s.%s\n", classDesc, methodName);
          break;
        }
      }
      if (soa.Self()->IsExceptionPending()) soa.Self()->ClearException();
    };
    latePatch("Landroid/sysprop/SocProperties;", "soc_manufacturer",
        "()Ljava/util/Optional;", (void*)SocProperties_soc_manufacturer);
    latePatch("Landroid/sysprop/SocProperties;", "soc_model",
        "()Ljava/util/Optional;", (void*)SocProperties_soc_model);
    latePatch("Landroid/sysprop/TelephonyProperties;", "baseband_version",
        "()Ljava/util/List;", (void*)TelephonyProperties_baseband_version);
  }

  // Pre-initialize classes that MCD app needs during onCreate
  {
    // Force-init SecureRandom / UUID (analytics SDKs use UUID.randomUUID())
    const char* preInitClasses[] = {
      "java/security/SecureRandom",
      "java/util/UUID",
      "java/util/Collections",
      "java/util/HashMap",
      "java/util/ArrayList",
      "java/util/HashSet",
      "java/lang/ref/WeakReference",
      "android/util/ArrayMap",
      "android/util/SparseArray",
      "android/os/Handler",
      "android/os/Looper",
      nullptr
    };
    for (int i = 0; preInitClasses[i]; i++) {
      jclass cls = env->FindClass(preInitClasses[i]);
      if (env->ExceptionCheck()) env->ExceptionClear();
      // FindClass triggers class initialization
    }

    // Fix StandardCharsets.UTF_8 and sun.nio.cs.UTF_8.INSTANCE
    {
      // First ensure sun.nio.cs.UTF_8 singleton exists
      jclass utf8Impl = env->FindClass("sun/nio/cs/UTF_8");
      if (env->ExceptionCheck()) env->ExceptionClear();
      jobject utf8Instance = nullptr;
      if (utf8Impl) {
        jfieldID instF = env->GetStaticFieldID(utf8Impl, "INSTANCE", "Lsun/nio/cs/UTF_8;");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (instF) utf8Instance = env->GetStaticObjectField(utf8Impl, instF);
        if (!utf8Instance) {
          utf8Instance = env->AllocObject(utf8Impl);
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (utf8Instance && instF) {
            env->SetStaticObjectField(utf8Impl, instF, utf8Instance);
            fprintf(stderr, "[dalvikvm] Created sun.nio.cs.UTF_8.INSTANCE\n");
          }
        }
      }
      // Set StandardCharsets.UTF_8 using the instance
      jclass scCls = env->FindClass("java/nio/charset/StandardCharsets");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (scCls && utf8Instance) {
        jfieldID utf8F = env->GetStaticFieldID(scCls, "UTF_8", "Ljava/nio/charset/Charset;");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (utf8F && !env->GetStaticObjectField(scCls, utf8F)) {
          env->SetStaticObjectField(scCls, utf8F, utf8Instance);
          fprintf(stderr, "[dalvikvm] Fixed StandardCharsets.UTF_8\n");
        }
      }
      // Also try Charset.forName for other charsets
      jclass csCls = env->FindClass("java/nio/charset/Charset");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (scCls && csCls) {
        jmethodID forName = env->GetStaticMethodID(csCls, "forName",
            "(Ljava/lang/String;)Ljava/nio/charset/Charset;");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (forName) {
          const char* names[] = {"US_ASCII", "ISO_8859_1", nullptr};
          const char* csnames[] = {"US-ASCII", "ISO-8859-1", nullptr};
          for (int i = 0; names[i]; i++) {
            jfieldID f = env->GetStaticFieldID(scCls, names[i], "Ljava/nio/charset/Charset;");
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (f && !env->GetStaticObjectField(scCls, f)) {
              jobject cs = env->CallStaticObjectMethod(csCls, forName, env->NewStringUTF(csnames[i]));
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (cs) env->SetStaticObjectField(scCls, f, cs);
            }
          }
        }
      }
      if (env->ExceptionCheck()) env->ExceptionClear();

      // Fix kotlin.text.Charsets.UTF_8 — clinit fails with NoClassDefFoundError.
      // Must use JNI FindClass (not FindSystemClass) because it's in MCD DEX.
      // FindClass triggers clinit which fails and is tolerated — then we set the field.
      {
        jclass ktCharsets = env->FindClass("kotlin/text/Charsets");
        if (env->ExceptionCheck()) env->ExceptionClear();
        fprintf(stderr, "[dalvikvm] kotlin.text.Charsets FindClass: %p\n", ktCharsets);
        if (ktCharsets) {
          // Get StandardCharsets.UTF_8
          jclass scCls2 = env->FindClass("java/nio/charset/StandardCharsets");
          if (env->ExceptionCheck()) env->ExceptionClear();
          jobject stdUtf8 = nullptr;
          if (scCls2) {
            jfieldID suf = env->GetStaticFieldID(scCls2, "UTF_8", "Ljava/nio/charset/Charset;");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (suf) stdUtf8 = env->GetStaticObjectField(scCls2, suf);
          }
          fprintf(stderr, "[dalvikvm] StandardCharsets.UTF_8 = %p\n", stdUtf8);
          if (!stdUtf8) {
            // StandardCharsets.UTF_8 is null! Create via Charset.forName
            jclass csCls = env->FindClass("java/nio/charset/Charset");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (csCls) {
              jmethodID forName = env->GetStaticMethodID(csCls, "forName",
                  "(Ljava/lang/String;)Ljava/nio/charset/Charset;");
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (forName) {
                stdUtf8 = env->CallStaticObjectMethod(csCls, forName, env->NewStringUTF("UTF-8"));
                if (env->ExceptionCheck()) { env->ExceptionClear(); stdUtf8 = nullptr; }
                fprintf(stderr, "[dalvikvm] Charset.forName(UTF-8) = %p\n", stdUtf8);
              }
            }
            if (!stdUtf8) {
              // Try multiple concrete charset implementations
              const char* implClasses[] = {
                "sun/nio/cs/UTF_8",
                "com/android/icu/charset/CharsetICU",
                "java/nio/charset/Charset",  // abstract, but AllocObject may work
                nullptr
              };
              for (int ci = 0; !stdUtf8 && implClasses[ci]; ci++) {
                jclass implCls = env->FindClass(implClasses[ci]);
                if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                if (implCls) {
                  stdUtf8 = env->AllocObject(implCls);
                  if (env->ExceptionCheck()) { env->ExceptionClear(); stdUtf8 = nullptr; continue; }
                  fprintf(stderr, "[dalvikvm] AllocObject(%s) = %p\n", implClasses[ci], stdUtf8);
                }
              }
              // Set the name field on whatever we created
              if (stdUtf8) {
                jclass charsetCls = env->FindClass("java/nio/charset/Charset");
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (charsetCls) {
                  jfieldID nameF = env->GetFieldID(charsetCls, "name", "Ljava/lang/String;");
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (nameF) env->SetObjectField(stdUtf8, nameF, env->NewStringUTF("UTF-8"));
                }
                fprintf(stderr, "[dalvikvm] Created UTF-8 charset with name field set\n");
              } else {
                fprintf(stderr, "[dalvikvm] WARN: Could not create any UTF-8 charset instance\n");
              }
            }
            // Set StandardCharsets.UTF_8
            if (stdUtf8 && scCls2) {
              jfieldID suf2 = env->GetStaticFieldID(scCls2, "UTF_8", "Ljava/nio/charset/Charset;");
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (suf2) {
                env->SetStaticObjectField(scCls2, suf2, stdUtf8);
                fprintf(stderr, "[dalvikvm] Set StandardCharsets.UTF_8\n");
              }
            }
          }
          if (stdUtf8) {
            jfieldID ktUtf8F = env->GetStaticFieldID(ktCharsets, "UTF_8", "Ljava/nio/charset/Charset;");
            if (env->ExceptionCheck()) env->ExceptionClear();
            fprintf(stderr, "[dalvikvm] kotlin.text.Charsets.UTF_8 field = %p\n", (void*)ktUtf8F);
            if (ktUtf8F) {
              jobject cur = env->GetStaticObjectField(ktCharsets, ktUtf8F);
              fprintf(stderr, "[dalvikvm] kotlin.text.Charsets.UTF_8 current = %p\n", cur);
              env->SetStaticObjectField(ktCharsets, ktUtf8F, stdUtf8);
              fprintf(stderr, "[dalvikvm] Fixed kotlin.text.Charsets.UTF_8\n");
            }
          }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
      }
    }

    // Fix android.os.Build static fields (clinit fails, leaves all null)
    {
      jclass buildCls = env->FindClass("android/os/Build");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (buildCls) {
        struct { const char* name; const char* val; } fields[] = {
          {"BOARD", "westlake"}, {"BOOTLOADER", "unknown"}, {"BRAND", "Westlake"},
          {"DEVICE", "westlake_vm"}, {"DISPLAY", "Westlake-1.0"}, {"FINGERPRINT", "westlake/vm/1.0"},
          {"HARDWARE", "westlake"}, {"HOST", "localhost"}, {"ID", "WLK.1"},
          {"MANUFACTURER", "Westlake"}, {"MODEL", "Westlake VM"}, {"PRODUCT", "westlake"},
          {"TAGS", "release-keys"}, {"TYPE", "userdebug"}, {"USER", "westlake"},
          {nullptr, nullptr}
        };
        for (int i = 0; fields[i].name; i++) {
          jfieldID f = env->GetStaticFieldID(buildCls, fields[i].name, "Ljava/lang/String;");
          if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
          if (f) env->SetStaticObjectField(buildCls, f, env->NewStringUTF(fields[i].val));
        }
        // VERSION fields
        jclass verCls = env->FindClass("android/os/Build$VERSION");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (verCls) {
          struct { const char* name; const char* type; const char* sval; int ival; } vfields[] = {
            {"RELEASE", "Ljava/lang/String;", "15", 0},
            {"SDK_INT", "I", nullptr, 35},
            {"CODENAME", "Ljava/lang/String;", "REL", 0},
            {"BASE_OS", "Ljava/lang/String;", "", 0},
            {"SECURITY_PATCH", "Ljava/lang/String;", "2025-01-01", 0},
            {nullptr, nullptr, nullptr, 0}
          };
          for (int i = 0; vfields[i].name; i++) {
            jfieldID f = env->GetStaticFieldID(verCls, vfields[i].name, vfields[i].type);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (f) {
              if (vfields[i].sval) env->SetStaticObjectField(verCls, f, env->NewStringUTF(vfields[i].sval));
              else env->SetStaticIntField(verCls, f, vfields[i].ival);
            }
          }
        }
        fprintf(stderr, "[dalvikvm] Set Build + Build.VERSION fields\n");
      }
      if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // Also ensure Looper.sMainLooper is set (many Android APIs need it)
    jclass looperCls = env->FindClass("android/os/Looper");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (looperCls) {
      // Call Looper.prepareMainLooper() to set sMainLooper
      jmethodID prepMain = env->GetStaticMethodID(looperCls, "prepareMainLooper", "()V");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (prepMain) {
        env->CallStaticVoidMethod(looperCls, prepMain);
        if (env->ExceptionCheck()) env->ExceptionClear();
        fprintf(stderr, "[dalvikvm] Looper.prepareMainLooper() called\n");
      }
    }
  }

  // Leave Resources.obtainStyledAttributes in Java.
  // The shim implementation already returns an empty TypedArray when host
  // theming is unavailable; forcing these methods native/null only breaks
  // View construction later.
  {
    fprintf(stderr, "[dalvikvm] Leaving Resources.obtainStyledAttributes/obtainAttributes unpatched\n");
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Leave Context.obtainStyledAttributes in Java for the same reason.
  {
    fprintf(stderr, "[dalvikvm] Leaving Context.obtainStyledAttributes unpatched\n");
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Patch Resources$Theme methods to no-ops (ThemeImpl is null without real AssetManager)
  {
    jclass themeCls = env->FindClass("android/content/res/Resources$Theme");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (themeCls) {
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(themeCls);
      if (mirror != nullptr) {
        int patched = 0;
        for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
          if (m.IsNative() || m.IsAbstract() || m.IsConstructor()) continue;
          const char* name = m.GetName();
          std::string sig = m.GetSignature().ToString();
          // Patch void methods to no-op
          if (sig.find(")V") != std::string::npos) {
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
            patched++;
          }
          // Patch boolean resolveAttribute → return false
          else if (strcmp(name, "resolveAttribute") == 0 && sig.find(")Z") != std::string::npos) {
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_false));
            patched++;
          }
        }
        fprintf(stderr, "[dalvikvm] Patched %d Resources$Theme methods → no-op\n", patched);
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Patch Fragment system — only void/boolean methods (preserve getters)
  {
    const char* fragClasses[] = {
      "android/app/FragmentController",
      "android/app/FragmentManagerImpl",
      nullptr
    };
    int patched = 0;
    for (int ci = 0; fragClasses[ci]; ci++) {
      jclass cls = env->FindClass(fragClasses[ci]);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (!cls) continue;
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(cls);
      if (mirror == nullptr) continue;
      for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
        if (m.IsNative() || m.IsAbstract() || m.IsConstructor()) continue;
        std::string sig = m.GetSignature().ToString();
        const char* name = m.GetName();
        // Only no-op void dispatch methods and specific NPE-causing methods
        if (sig.find(")V") != std::string::npos) {
          m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
          patched++;
        } else if (sig.find(")Z") != std::string::npos) {
          m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_false));
          patched++;
        }
        // retainNonConfig → null (specific method, not all object-returning)
        else if (strcmp(name, "retainNonConfig") == 0 || strcmp(name, "retainNestedNonConfig") == 0) {
          m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_null));
          patched++;
        }
      }
    }
    // Also patch abstract methods on Fragment base classes
    const char* abstractFragClasses[] = {
      "android/app/FragmentManager",
      "android/app/FragmentTransaction",
      nullptr
    };
    for (int aci = 0; abstractFragClasses[aci]; aci++) {
    jclass fmBaseCls = env->FindClass(abstractFragClasses[aci]);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (fmBaseCls) {
      art::ScopedObjectAccess soa2(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> fmMirror = soa2.Decode<art::mirror::Class>(fmBaseCls);
      if (fmMirror != nullptr) {
        for (art::ArtMethod& m : fmMirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
          if (!m.IsAbstract()) continue;
          std::string sig = m.GetSignature().ToString();
          // Make abstract void methods concrete no-ops
          if (sig.find(")V") != std::string::npos) {
            m.SetAccessFlags((m.GetAccessFlags() & ~art::kAccAbstract) | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
            patched++;
          } else if (sig.find(")L") != std::string::npos) {
            m.SetAccessFlags((m.GetAccessFlags() & ~art::kAccAbstract) | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_null));
            patched++;
          } else if (sig.find(")Z") != std::string::npos) {
            m.SetAccessFlags((m.GetAccessFlags() & ~art::kAccAbstract) | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_false));
            patched++;
          }
        }
      }
    }
    } // end for abstractFragClasses
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr, "[dalvikvm] Patched %d Fragment system methods\n", patched);
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Fix StatLogger: create instance and set on WindowManagerGlobal.sStatLogger
  // The clinit fails but we need a valid instance for addView to work
  {
    jclass slCls = env->FindClass("com/android/internal/util/StatLogger");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (slCls) {
      // Patch methods to return 0/no-op
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(slCls);
      if (mirror != nullptr) {
        static auto ret0Long = +[](JNIEnv*, jobject) -> jlong { return 0; };
        for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
          if (m.IsNative() || m.IsAbstract() || m.IsConstructor()) continue;
          std::string sig = m.GetSignature().ToString();
          if (sig.find(")J") != std::string::npos) {
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(+ret0Long));
          } else if (sig.find(")V") != std::string::npos) {
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
          }
        }
      }
      // Create instance and set on WindowManagerGlobal
      jobject sl = env->AllocObject(slCls);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (sl) {
        jclass wmgCls = env->FindClass("android/view/WindowManagerGlobal");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (wmgCls) {
          jfieldID slF = env->GetStaticFieldID(wmgCls, "sStatLogger", "Lcom/android/internal/util/StatLogger;");
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (slF) env->SetStaticObjectField(wmgCls, slF, sl);
        }
        fprintf(stderr, "[dalvikvm] Created StatLogger + set on WindowManagerGlobal\n");
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Patch BinderProxy.unlinkToDeath/linkToDeath → no-op (null mObituaries list)
  {
    jclass bpCls = env->FindClass("android/os/BinderProxy");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (bpCls) {
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(bpCls);
      if (mirror != nullptr) {
        for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
          const char* n = m.GetName();
          if ((strcmp(n, "unlinkToDeath") == 0 || strcmp(n, "linkToDeath") == 0) && !m.IsNative()) {
            std::string sig = m.GetSignature().ToString();
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            if (sig.find(")Z") != std::string::npos)
              m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_true));
            else
              m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
          }
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Re-register Parcel natives in final fixup — entry points were cleared
  // by VisitPackedArtMethods and @CriticalNative methods can't be resolved
  // via FindCodeForNativeMethod (JNI name lookup doesn't work for them)
  {
    void* librt = dlopen("libandroid_runtime.so", RTLD_NOW);
    if (librt) {
      typedef int (*RegFn)(JNIEnv*);
      RegFn regParcel = (RegFn)dlsym(librt, "_ZN7android26register_android_os_ParcelEP7_JNIEnv");
      if (regParcel) {
        env->PushLocalFrame(128);
        regParcel(env);
        env->PopLocalFrame(nullptr);
        if (env->ExceptionCheck()) env->ExceptionClear();
        fprintf(stderr, "[dalvikvm] FINAL: Re-registered Parcel natives\n");
      }
      // Also re-register Binder for good measure
      RegFn regBinder = (RegFn)dlsym(librt, "_Z26register_android_os_BinderP7_JNIEnv");
      if (regBinder) {
        env->PushLocalFrame(128);
        regBinder(env);
        env->PopLocalFrame(nullptr);
        if (env->ExceptionCheck()) env->ExceptionClear();
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Patch LoadedApk.updateApplicationInfo → no-op (avoids null context chain NPEs)
  {
    jclass lapCls = env->FindClass("android/app/LoadedApk");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (lapCls) {
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(lapCls);
      if (mirror != nullptr) {
        for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
          if (strcmp(m.GetName(), "updateApplicationInfo") == 0 && !m.IsNative()) {
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
            fprintf(stderr, "[dalvikvm] Patched LoadedApk.updateApplicationInfo → no-op\n");
            break;
          }
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Fix ViewConfiguration — create with default display density
  {
    jclass vcCls = env->FindClass("android/view/ViewConfiguration");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (vcCls) {
      // Patch getDisplayDensity to return 320 (xxhdpi)
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(vcCls);
      if (mirror != nullptr) {
        for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
          if (strcmp(m.GetName(), "getDisplayDensity") == 0 && !m.IsNative()) {
            // Patch to return 320 (xxhdpi density)
            static auto retDensity = +[](JNIEnv*, jclass, jobject) -> jint { return 320; };
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative | art::kAccStatic);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(+retDensity));
            fprintf(stderr, "[dalvikvm] Patched ViewConfiguration.getDisplayDensity → 320\n");
            break;
          }
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Bypass AndroidX lifecycle + fragment components (need full system init)
  {
    const char* axClasses[] = {
      "androidx/savedstate/SavedStateRegistryController",
      "androidx/activity/contextaware/ContextAwareHelper",
      "androidx/lifecycle/LifecycleRegistry",
      "androidx/lifecycle/ReportFragment",
      "androidx/fragment/app/FragmentController",
      "androidx/fragment/app/FragmentManagerImpl",
      "androidx/fragment/app/FragmentHostCallback",
      nullptr
    };
    for (int ci = 0; axClasses[ci]; ci++) {
      jclass cls = env->FindClass(axClasses[ci]);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (!cls) continue;
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(cls);
      if (mirror == nullptr) continue;
      for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
        if (m.IsNative() || m.IsAbstract() || m.IsConstructor()) continue;
        std::string sig = m.GetSignature().ToString();
        if (sig.find(")V") != std::string::npos) {
          m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
        } else if (sig.find(")Z") != std::string::npos) {
          m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_false));
        } else if (sig.find(")L") != std::string::npos) {
          m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_null));
        }
      }
    }
    fprintf(stderr, "[dalvikvm] Patched AndroidX lifecycle components → no-op\n");
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Bypass NewRelic tracing (another analytics SDK)
  {
    const char* nrClasses[] = {
      "com/newrelic/agent/android/tracing/TraceMachine",
      "com/newrelic/agent/android/tracing/Trace",
      "com/newrelic/agent/android/NewRelic",
      nullptr
    };
    int totalPatched = 0;
    for (int ci = 0; nrClasses[ci]; ci++) {
      jclass cls = env->FindClass(nrClasses[ci]);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (!cls) continue;
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(cls);
      if (mirror == nullptr) continue;
      for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
        if (m.IsNative() || m.IsAbstract() || m.IsConstructor()) continue;
        std::string sig = m.GetSignature().ToString();
        if (sig.find(")V") != std::string::npos) {
          m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
          totalPatched++;
        }
      }
    }
    if (totalPatched > 0)
      fprintf(stderr, "[dalvikvm] Patched %d NewRelic methods → no-op\n", totalPatched);
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Fix StrictMode.sVmPolicy (null causes NPE in View constructors)
  {
    jclass smCls = env->FindClass("android/os/StrictMode");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (smCls) {
      jclass vpCls = env->FindClass("android/os/StrictMode$VmPolicy");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (vpCls) {
        jfieldID vpF = env->GetStaticFieldID(smCls, "sVmPolicy", "Landroid/os/StrictMode$VmPolicy;");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (vpF && !env->GetStaticObjectField(smCls, vpF)) {
          jobject vp = env->AllocObject(vpCls);
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (vp) {
            env->SetStaticObjectField(smCls, vpF, vp);
            fprintf(stderr, "[dalvikvm] Set StrictMode.sVmPolicy\n");
          }
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Stub Trace methods (tracing not available in standalone mode)
  {
    jclass traceCls = env->FindClass("android/os/Trace");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (traceCls) {
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(traceCls);
      if (mirror != nullptr) {
        for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
          const char* name = m.GetName();
          if (!m.IsNative()) continue; // only patch already-native methods
          std::string sig = m.GetSignature().ToString();
          if (sig.find(")Z") != std::string::npos) {
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_false));
          } else if (sig.find(")V") != std::string::npos) {
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
          }
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Patch AppCompatActivity.onCreate to call Activity.onCreate directly (skip delegate)
  {
    jclass acaCls = env->FindClass("androidx/appcompat/app/AppCompatActivity");
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr, "[dalvikvm] AppCompatActivity FindClass: %p\n", acaCls);
    if (acaCls) {
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(acaCls);
      if (mirror != nullptr) {
        int count = 0;
        for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
          if (strcmp(m.GetName(), "onCreate") == 0) {
            fprintf(stderr, "[dalvikvm]   found: %s sig=%s native=%d\n",
                    m.GetName(), m.GetSignature().ToString().c_str(), m.IsNative());
            if (m.GetSignature().ToString() == "(Landroid/os/Bundle;)V" && !m.IsNative()) {
              m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
              m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
              fprintf(stderr, "[dalvikvm] Patched AppCompatActivity.onCreate → no-op\n");
            }
          }
          count++;
        }
        fprintf(stderr, "[dalvikvm] AppCompatActivity: %d methods total\n", count);
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Patch AppCompatDelegateImpl void methods to prevent infinite recursion
  // (setTheme → delegate.Q → setTheme → ...)
  {
    jclass adCls = env->FindClass("androidx/appcompat/app/AppCompatDelegateImpl");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (adCls) {
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(adCls);
      if (mirror != nullptr) {
        int patched = 0;
        for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
          if (m.IsNative() || m.IsAbstract() || m.IsConstructor()) continue;
          std::string sig = m.GetSignature().ToString();
          const char* name = m.GetName();
          // Patch void methods that take int (like Q, setTheme)
          if (sig == "(I)V" || (sig.find(")V") != std::string::npos && strlen(name) <= 2)) {
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
            patched++;
          }
        }
        fprintf(stderr, "[dalvikvm] Patched %d AppCompatDelegateImpl short void methods\n", patched);
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Patch Activity methods that need Window/WindowController (our mock Window is incomplete)
  {
    jclass actCls = env->FindClass("android/app/Activity");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (actCls) {
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(actCls);
      if (mirror != nullptr) {
        struct { const char* name; const char* sig; void* fn; } patches[] = {
          {"isTaskRoot", "()Z", (void*)Java_noop_return_true},
          {"isFinishing", "()Z", (void*)Java_noop_return_false},
          {"isDestroyed", "()Z", (void*)Java_noop_return_false},
          {"isChangingConfigurations", "()Z", (void*)Java_noop_return_false},
          {"onApplyThemeResource",
           "(Landroid/content/res/Resources$Theme;IZ)V",
           (void*)Java_java_lang_Throwable_printStackTrace_noop},
          {"setTheme", "(I)V", (void*)Java_java_lang_Throwable_printStackTrace_noop},
          // Prevent Fragment cleanup from hitting abstract methods
          {"onDestroy", "()V", (void*)Java_java_lang_Throwable_printStackTrace_noop},
          {nullptr, nullptr, nullptr}
        };
        for (int i = 0; patches[i].name; i++) {
          for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
            if (strcmp(m.GetName(), patches[i].name) == 0 &&
                m.GetSignature().ToString() == patches[i].sig && !m.IsNative()) {
              m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
              m.SetEntryPointFromJni(reinterpret_cast<void*>(patches[i].fn));
              break;
            }
          }
        }
        fprintf(stderr, "[dalvikvm] Patched Activity boolean methods\n");
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Patch SplashScreen to no-ops (needs fully working Resources/Theme which we don't have)
  {
    const char* splashClasses[] = {
      "androidx/core/splashscreen/SplashScreen",
      "androidx/core/splashscreen/SplashScreen$Companion",
      "androidx/core/splashscreen/SplashScreen$Impl",
      "androidx/core/splashscreen/SplashScreen$Impl31",
      nullptr
    };
    int totalPatched = 0;
    for (int ci = 0; splashClasses[ci]; ci++) {
      jclass cls = env->FindClass(splashClasses[ci]);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (!cls) continue;
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(cls);
      if (mirror == nullptr) continue;
      for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
        if (!m.IsNative() && !m.IsAbstract() && !m.IsConstructor()) {
          std::string sig = m.GetSignature().ToString();
          if (sig.find(")V") != std::string::npos) {
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(
                reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
            totalPatched++;
          } else if (sig.find(")L") != std::string::npos) {
            // Object-returning method → return null
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(
                reinterpret_cast<void*>(Java_noop_return_null));
            totalPatched++;
          }
        }
      }
    }
    fprintf(stderr, "[dalvikvm] Patched %d SplashScreen methods → no-op\n", totalPatched);
  }

  // Patch Phrase SDK (localization) to no-ops (Severity enum has null values)
  {
    const char* phraseClasses[] = {
      "com/phrase/android/sdk/PhraseContextWrapper",
      "com/phrase/android/sdk/PhraseLog",
      "com/phrase/android/sdk/Severity",
      nullptr
    };
    int totalPatched = 0;
    for (int ci = 0; phraseClasses[ci]; ci++) {
      jclass cls = env->FindClass(phraseClasses[ci]);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (!cls) continue;
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(cls);
      if (mirror == nullptr) continue;
      for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
        if (m.IsNative() || m.IsAbstract() || m.IsConstructor()) continue;
        std::string sig = m.GetSignature().ToString();
        if (sig.find(")V") != std::string::npos) {
          m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
          totalPatched++;
        } else if (sig.find(")L") != std::string::npos) {
          m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_null));
          totalPatched++;
        } else if (sig.find(")Z") != std::string::npos) {
          m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_false));
          totalPatched++;
        }
      }
    }
    if (totalPatched > 0)
      fprintf(stderr, "[dalvikvm] Patched %d Phrase SDK methods → no-op\n", totalPatched);
    // Special: Phrase.d(Activity, Delegate) → return delegate (pass-through)
    jclass phraseCls = env->FindClass("com/phrase/android/sdk/Phrase");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (phraseCls) {
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> pm = soa.Decode<art::mirror::Class>(phraseCls);
      if (pm != nullptr) {
        for (art::ArtMethod& m : pm->GetDeclaredMethods(art::kRuntimePointerSize)) {
          if (m.IsNative() || m.IsAbstract() || m.IsConstructor()) continue;
          const char* name = m.GetName();
          std::string sig = m.GetSignature().ToString();
          if (sig.find(")V") != std::string::npos) {
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
            totalPatched++;
          } else if (sig.find("AppCompatDelegate") != std::string::npos &&
                     sig.find(")L") != std::string::npos) {
            // d(Activity, Delegate) → return delegate
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative | art::kAccStatic);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_arg2));
            fprintf(stderr, "[dalvikvm] Patched Phrase.%s → pass-through delegate\n", name);
            totalPatched++;
          } else if (sig.find(")L") != std::string::npos) {
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_null));
            totalPatched++;
          } else if (sig.find(")Z") != std::string::npos) {
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_false));
            totalPatched++;
          }
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // McDonald's splash boot still trips Hilt/account-profile work that Westlake
  // does not need. We already queue HomeDashboardActivity directly, so bypass
  // the DI wrapper and the splash-only home navigation path.
  {
    struct SplashPatch {
      const char* class_name;
      const char* method_name;
      const char* signature;
      void* entry;
    } patches[] = {
      {"com/mcdonalds/mcdcoreapp/common/activity/Hilt_SplashActivity",
       "onCreate",
       "(Landroid/os/Bundle;)V",
       (void*)Java_java_lang_Throwable_printStackTrace_noop},
      {"com/mcdonalds/mcdcoreapp/common/activity/SplashActivity",
       "launchHome",
       "()V",
       (void*)Java_java_lang_Throwable_printStackTrace_noop},
      {nullptr, nullptr, nullptr, nullptr},
    };
    int patched = 0;
    for (int pi = 0; patches[pi].class_name != nullptr; ++pi) {
      jclass patchCls = env->FindClass(patches[pi].class_name);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (!patchCls) {
        continue;
      }
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(patchCls);
      if (mirror == nullptr) {
        continue;
      }
      for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
        if (m.IsNative() || m.IsAbstract() || m.IsConstructor()) {
          continue;
        }
        if (strcmp(m.GetName(), patches[pi].method_name) == 0 &&
            m.GetSignature().ToString() == patches[pi].signature) {
          m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
          m.SetEntryPointFromJni(reinterpret_cast<void*>(patches[pi].entry));
          patched++;
          fprintf(stderr, "[dalvikvm] Patched %s.%s%s\n",
                  patches[pi].class_name,
                  patches[pi].method_name,
                  patches[pi].signature);
          break;
        }
      }
    }
    fprintf(stderr, "[dalvikvm] Splash bootstrap patches applied=%d\n", patched);
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Patch MCD analytics methods to no-ops (they cause NPE from null config strings)
  // PerfAnalyticsInteractor.h() and PerfAnalyticsInteractor.u() are performance tracking —
  // not needed for UI rendering. Patching them lets onCreate reach super.onCreate().
  {
    jclass perfCls = env->FindClass("com/mcdonalds/mcdcoreapp/performanalytics/PerfAnalyticsInteractor");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (perfCls) {
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> cls = soa.Decode<art::mirror::Class>(perfCls);
      if (cls != nullptr) {
        int patched = 0;
        for (art::ArtMethod& m : cls->GetDeclaredMethods(art::kRuntimePointerSize)) {
          const char* name = m.GetName();
          // Patch all non-static void methods to no-op (analytics tracking)
          if (!m.IsNative() && !m.IsStatic() && !m.IsConstructor()) {
            std::string sig = m.GetSignature().ToString();
            if (sig.find(")V") != std::string::npos) {
              // void method — make it return immediately via native no-op
              m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
              m.SetEntryPointFromJni(
                  reinterpret_cast<void*>(Java_java_lang_Throwable_printStackTrace_noop));
              patched++;
            }
          }
        }
        fprintf(stderr, "[dalvikvm] Patched %d PerfAnalyticsInteractor void methods → no-op\n", patched);
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // AppCoreUtils static init currently trips a broken HashSet(Arrays.asList(...))
  // path in standalone ART. Seed a non-null whitelist and bypass the private
  // validator so SplashActivity.getExtraIntentData() can continue.
  PatchMcDonaldsAppCoreUtils(env);

  // FINAL FIXUP: re-set ConcurrentHashMap.U right before main()
  // Earlier setting might have been overwritten by class loading/init during pre-init phase.
  {
    PatchConcurrentHashMapUnsafe(env, "final before main");
  }

  // Final fixup: set StatLogger on WindowManagerGlobal (clinit may have reset it)
  {
    jclass slCls = env->FindClass("com/android/internal/util/StatLogger");
    if (env->ExceptionCheck()) env->ExceptionClear();
    jclass wmgCls = env->FindClass("android/view/WindowManagerGlobal");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (slCls && wmgCls) {
      jfieldID slF = env->GetStaticFieldID(wmgCls, "sStatLogger", "Lcom/android/internal/util/StatLogger;");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (slF) {
        jobject sl = env->AllocObject(slCls);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (sl) env->SetStaticObjectField(wmgCls, slF, sl);
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    // Patch BinderProxy$ProxyMap.get to return null (avoids null WeakReference NPE)
  // The null comes from ProxyMap's WeakReference entries not being initialized
  {
    jclass pmCls = env->FindClass("android/os/BinderProxy$ProxyMap");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (pmCls) {
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(pmCls);
      if (mirror != nullptr) {
        for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
          if (strcmp(m.GetName(), "get") == 0 && !m.IsNative()) {
            // Return null — forces BinderProxy.getInstance to create a new proxy
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(Java_noop_return_null));
            break;
          }
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // Stub NativeAllocationRegistry completely (GC cleanup not needed in standalone)
  {
    jclass narCls = env->FindClass("libcore/util/NativeAllocationRegistry");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (narCls) {
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(narCls);
      if (mirror != nullptr) {
        static auto noopJJ = +[](JNIEnv*, jclass, jlong, jlong) {};
        // Stub all methods — native and non-native
        for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
          if (m.IsAbstract() || m.IsConstructor()) continue;
          std::string sig = m.GetSignature().ToString();
          if (strcmp(m.GetName(), "applyFreeFunction") == 0 && m.IsNative()) {
            m.SetEntryPointFromJni(reinterpret_cast<void*>(+noopJJ));
          }
          // registerNativeAllocation → return no-op Runnable (not null!)
          if (strcmp(m.GetName(), "registerNativeAllocation") == 0 && !m.IsNative()) {
            // Return a Runnable that does nothing
            static auto retNoopRunnable = +[](JNIEnv* e, jobject, jobject) -> jobject {
              // Create a no-op Runnable via Proxy
              // Simpler: just return the object itself (it has run() which won't be called)
              // Actually simplest: allocate a minimal Runnable
              jclass runnableCls = e->FindClass("java/lang/Thread");
              if (e->ExceptionCheck()) e->ExceptionClear();
              if (runnableCls) {
                jobject r = e->AllocObject(runnableCls);
                if (e->ExceptionCheck()) e->ExceptionClear();
                return r;
              }
              return nullptr;
            };
            m.SetAccessFlags(m.GetAccessFlags() | art::kAccNative);
            m.SetEntryPointFromJni(reinterpret_cast<void*>(+retNoopRunnable));
          }
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }
  // Also fix SystemClock.elapsedRealtime if not registered
    jclass scCls = env->FindClass("android/os/SystemClock");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (scCls) {
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::ObjPtr<art::mirror::Class> mirror = soa.Decode<art::mirror::Class>(scCls);
      if (mirror != nullptr) {
        static auto elapsed = +[](JNIEnv*, jclass) -> jlong {
          struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
          return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
        };
        for (art::ArtMethod& m : mirror->GetDeclaredMethods(art::kRuntimePointerSize)) {
          if (strcmp(m.GetName(), "elapsedRealtime") == 0 && m.IsNative()) {
            m.SetEntryPointFromJni(reinterpret_cast<void*>(+elapsed));
          }
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  // FINAL: Fix AppOpsManager ThreadLocals (MUST be after all clinit tolerance)
  {
    jclass aomCls = env->FindClass("android/app/AppOpsManager");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (aomCls) {
      jclass tlCls = env->FindClass("java/lang/ThreadLocal");
      if (env->ExceptionCheck()) env->ExceptionClear();
      jmethodID tlInit = tlCls ? env->GetMethodID(tlCls, "<init>", "()V") : nullptr;
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (tlInit) {
        // Find and set ALL ThreadLocal static fields via JNI
        jclass clsCls = env->FindClass("java/lang/Class");
        jmethodID getDeclF = clsCls ? env->GetMethodID(clsCls, "getDeclaredFields", "()[Ljava/lang/reflect/Field;") : nullptr;
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (getDeclF) {
          jobjectArray fields = (jobjectArray)env->CallObjectMethod(aomCls, getDeclF);
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (fields) {
            jint len = env->GetArrayLength(fields);
            for (int i = 0; i < len; i++) {
              jobject field = env->GetObjectArrayElement(fields, i);
              if (!field) continue;
              jclass fieldCls = env->GetObjectClass(field);
              jmethodID getType = env->GetMethodID(fieldCls, "getType", "()Ljava/lang/Class;");
              jclass fType = (jclass)env->CallObjectMethod(field, getType);
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (fType && env->IsAssignableFrom(fType, tlCls)) {
                jmethodID setAcc = env->GetMethodID(fieldCls, "setAccessible", "(Z)V");
                env->CallVoidMethod(field, setAcc, JNI_TRUE);
                if (env->ExceptionCheck()) env->ExceptionClear();
                jmethodID getF = env->GetMethodID(fieldCls, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
                jobject val = env->CallObjectMethod(field, getF, (jobject)nullptr);
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (!val) {
                  jobject tl = env->NewObject(tlCls, tlInit);
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (tl) {
                    jmethodID setF = env->GetMethodID(fieldCls, "set", "(Ljava/lang/Object;Ljava/lang/Object;)V");
                    env->CallVoidMethod(field, setF, (jobject)nullptr, tl);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                  }
                }
              }
            }
          }
        }
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  }

  // [ARM64-OHOS 2026-07-08] Install a REAL System.out / System.err here, on the
  // big-stack app thread AFTER the runtime is fully up (constructing PrintStream
  // during Runtime::Start hangs; in-app it works — the runtime's cache2 + buffer
  // static fixes make it succeed). Without this, System.out is the broken
  // force-init'd PrintStream and System.out.println NPEs.
  {
    jclass system_class = env->FindClass("java/lang/System");
    jclass fd_class = env->FindClass("java/io/FileDescriptor");
    jclass fos_class = env->FindClass("java/io/FileOutputStream");
    jclass ps_class = env->FindClass("java/io/PrintStream");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (system_class && fd_class && fos_class && ps_class) {
      jmethodID fos_init = env->GetMethodID(fos_class, "<init>", "(Ljava/io/FileDescriptor;)V");
      jmethodID ps_init = env->GetMethodID(ps_class, "<init>",
          "(Ljava/io/OutputStream;ZLjava/lang/String;)V");
      jstring utf8 = env->NewStringUTF("UTF-8");
      const char* fds[] = {"out", "err", nullptr};
      for (int i = 0; fds[i] && fos_init && ps_init; i++) {
        jfieldID fd_f = env->GetStaticFieldID(fd_class, fds[i], "Ljava/io/FileDescriptor;");
        jobject fd_o = fd_f ? env->GetStaticObjectField(fd_class, fd_f) : nullptr;
        if (!fd_o) { if (env->ExceptionCheck()) env->ExceptionClear(); continue; }
        jobject fos = env->NewObject(fos_class, fos_init, fd_o);
        if (env->ExceptionCheck() || !fos) { env->ExceptionClear(); continue; }
        jobject ps = env->NewObject(ps_class, ps_init, fos, JNI_TRUE, utf8);
        if (env->ExceptionCheck() || !ps) { env->ExceptionClear(); continue; }
        jfieldID sys_f = env->GetStaticFieldID(system_class, fds[i], "Ljava/io/PrintStream;");
        if (sys_f) env->SetStaticObjectField(system_class, sys_f, ps);
        env->DeleteLocalRef(ps); env->DeleteLocalRef(fos); env->DeleteLocalRef(fd_o);
      }
      if (utf8) env->DeleteLocalRef(utf8);
      fprintf(stderr, "[dalvikvm] Installed real System.out/err (PrintStream/UTF-8)\n"); fflush(stderr);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  fprintf(stderr, "[dalvikvm] Calling main()...\n");
  fflush(stderr);
  struct timespec ts_start, ts_end;
  clock_gettime(CLOCK_MONOTONIC, &ts_start);
  env->CallStaticVoidMethod(klass.get(), method, args.get());
  clock_gettime(CLOCK_MONOTONIC, &ts_end);
  long elapsed_ms = (ts_end.tv_sec - ts_start.tv_sec) * 1000L +
                    (ts_end.tv_nsec - ts_start.tv_nsec) / 1000000L;
  fprintf(stderr, "[dalvikvm] main() returned (elapsed: %ld ms)\n", elapsed_ms);
  fflush(stderr);

  // Check whether there was an uncaught exception.  In standalone builds the normal
  // uncaught-exception handler may not be wired up, so print it ourselves.
  // Use both ExceptionOccurred and ExceptionCheck for robustness.
  jthrowable exc = env->ExceptionOccurred();
  if (exc != nullptr) {
    fprintf(stderr, "[dalvikvm] Exception occurred after main()\n");
    // Try to get exception class name
    jclass exc_class = env->GetObjectClass(exc);
    if (exc_class != nullptr) {
      jmethodID getName = env->GetMethodID(env->FindClass("java/lang/Class"), "getName", "()Ljava/lang/String;");
      if (getName != nullptr) {
        env->ExceptionClear(); // Clear to call methods
        jstring name = (jstring) env->CallObjectMethod(exc_class, getName);
        if (name != nullptr) {
          const char* nameChars = env->GetStringUTFChars(name, nullptr);
          if (nameChars) {
            fprintf(stderr, "[dalvikvm] Exception class: %s\n", nameChars);
            env->ReleaseStringUTFChars(name, nameChars);
          }
        }
        // Try getMessage()
        jmethodID getMsg = env->GetMethodID(exc_class, "getMessage", "()Ljava/lang/String;");
        if (getMsg != nullptr) {
          jstring msg = (jstring) env->CallObjectMethod(exc, getMsg);
          if (msg != nullptr) {
            const char* msgChars = env->GetStringUTFChars(msg, nullptr);
            if (msgChars) {
              fprintf(stderr, "[dalvikvm] Exception message: %s\n", msgChars);
              env->ReleaseStringUTFChars(msg, msgChars);
            }
          }
        }
      }
    }
    env->ExceptionClear();
    PrintThrowableRaw(env, exc);
    fflush(stderr);
    return EXIT_FAILURE;
  }
  if (env->ExceptionCheck()) {
    fprintf(stderr, "[dalvikvm] ExceptionCheck true after main()\n");
    jthrowable pending = env->ExceptionOccurred();
    env->ExceptionClear();
    PrintThrowableRaw(env, pending);
    fflush(stderr);
    return EXIT_FAILURE;
  }
  fprintf(stderr, "[dalvikvm] main() completed successfully\n");

  // Try calling various result methods on the class
  {
    const char* methods_to_try[] = {"getResult", "compute", "computeFib"};
    for (int i = 0; i < 3; i++) {
      jmethodID mid = env->GetStaticMethodID(klass.get(), methods_to_try[i], "()I");
      if (mid != nullptr) {
        jint val = env->CallStaticIntMethod(klass.get(), mid);
        fprintf(stderr, "[BENCH] %s() = %d\n", methods_to_try[i], (int)val);
        if (env->ExceptionCheck()) {
          env->ExceptionDescribe();
          env->ExceptionClear();
        }
      } else {
        env->ExceptionClear();
      }
    }
  }

  // After main() returns, try to read benchmark results from static fields
  // This allows benchmarks to store results without needing working I/O
  {
    const char* bench_fields[] = {"fibResult", "methodResult", "loopResult", "allocResult", "fieldResult"};
    const char* bench_names[] = {"FIB40", "METHOD_10M", "LOOP_100M", "ALLOC_1M", "FIELD_10M"};
    for (int i = 0; i < 5; i++) {
      jfieldID fid = env->GetStaticFieldID(klass.get(), bench_fields[i], "J");
      if (fid != nullptr) {
        jlong val = env->GetStaticLongField(klass.get(), fid);
        if (val >= 0) {
          fprintf(stderr, "[BENCH] %s = %lld ms\n", bench_names[i], (long long)val);
        }
      } else {
        env->ExceptionClear();
      }
    }
    // Also try fibAnswer
    jfieldID fid = env->GetStaticFieldID(klass.get(), "fibAnswer", "I");
    if (fid != nullptr) {
      jint val = env->GetStaticIntField(klass.get(), fid);
      if (val >= 0) {
        fprintf(stderr, "[BENCH] fibAnswer = %d\n", (int)val);
      }
    } else {
      env->ExceptionClear();
    }
  }

  return EXIT_SUCCESS;
}

// Parse arguments.  Most of it just gets passed through to the runtime.
// The JNI spec defines a handful of standard arguments.
static int dalvikvm(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);

  // Install a crash handler to get backtrace before ART's handler
#if !defined(__MUSL__)
  {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = [](int sig, siginfo_t* info, void* ctx) {
      fprintf(stderr, "\n[dalvikvm] CRASH: signal=%d addr=%p\n",
              sig, info->si_addr);
      fflush(stderr);
      _exit(128 + sig);
    };
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
  }
#endif  // !__MUSL__

  // Skip over argv[0].
  argv++;
  argc--;

  // If we're adding any additional stuff, e.g. function hook specifiers,
  // add them to the count here.
  //
  // We're over-allocating, because this includes the options to the runtime
  // plus the options to the program.
  int option_count = argc + 2; // extra slots for -Xss and -Xms
  std::unique_ptr<JavaVMOption[]> options(new JavaVMOption[option_count]());

  // Copy options over.  Everything up to the name of the class starts
  // with a '-' (the function hook stuff is strictly internal).
  //
  // [Do we need to catch & handle "-jar" here?]
  bool need_extra = false;
  const char* lib = nullptr;
  const char* what = nullptr;
  int curr_opt, arg_idx;
  bool capture_class_path_value = false;
  g_vm_class_path.clear();
  for (curr_opt = arg_idx = 0; arg_idx < argc; arg_idx++) {
    if (argv[arg_idx][0] != '-' && !need_extra) {
      break;
    }
    if (capture_class_path_value) {
      g_vm_class_path.assign(argv[arg_idx]);
      capture_class_path_value = false;
    }
    if (strncmp(argv[arg_idx], "-XXlib:", strlen("-XXlib:")) == 0) {
      lib = argv[arg_idx] + strlen("-XXlib:");
      continue;
    }

    options[curr_opt++].optionString = argv[arg_idx];

    // Some options require an additional argument.
    need_extra = false;
    if (strcmp(argv[arg_idx], "-classpath") == 0 || strcmp(argv[arg_idx], "-cp") == 0) {
      need_extra = true;
      what = argv[arg_idx];
      capture_class_path_value = true;
    }
  }

  if (need_extra) {
    fprintf(stderr, "%s must be followed by an additional argument giving a value\n", what);
    return EXIT_FAILURE;
  }

  // Inject a large thread stack for interpreted real-app startup. The C++
  // interpreter currently consumes much more native stack per Java frame than
  // ART's compiled paths, and McD's Gson/reflection startup exceeds 64MB.
  options[curr_opt++].optionString = const_cast<char*>("-XX:mainThreadStackSize=268435456");
  fprintf(stderr, "[dalvikvm] Injected mainThreadStackSize=256MB\n");

  if (curr_opt > option_count) {
    fprintf(stderr, "curr_opt(%d) > option_count(%d)\n", curr_opt, option_count);
    abort();
    return EXIT_FAILURE;
  }

  g_vm_property_options.clear();
  if (g_vm_class_path.empty()) {
    const char* env_class_path = getenv("CLASSPATH");
    if (env_class_path != nullptr) {
      g_vm_class_path.assign(env_class_path);
    }
  } else {
    setenv("CLASSPATH", g_vm_class_path.c_str(), 1);
    fprintf(stderr, "[dalvikvm] Captured -classpath=%s\n", g_vm_class_path.c_str());
  }
  for (int opt_idx = 0; opt_idx < curr_opt; ++opt_idx) {
    const char* opt = options[opt_idx].optionString;
    if (opt != nullptr && strncmp(opt, "-D", 2) == 0) {
      g_vm_property_options.emplace_back(opt);
    }
  }

  // Find the JNI_CreateJavaVM implementation.
  JniInvocation jni_invocation;
  if (!jni_invocation.Init(lib)) {
    fprintf(stderr, "Failed to initialize JNI invocation API from %s\n", lib);
    return EXIT_FAILURE;
  }

  JavaVMInitArgs init_args;
  init_args.version = JNI_VERSION_1_6;
  init_args.options = options.get();
  init_args.nOptions = curr_opt;
  init_args.ignoreUnrecognized = JNI_FALSE;

  // Start the runtime. The current thread becomes the main thread.
  JavaVM* vm = nullptr;
  JNIEnv* env = nullptr;
  if (JNI_CreateJavaVM(&vm, &env, &init_args) != JNI_OK) {
    fprintf(stderr, "Failed to initialize runtime (check log for details)\n");
    return EXIT_FAILURE;
  }

  // Clear any pending exceptions from runtime init
  if (env->ExceptionCheck()) env->ExceptionClear();

  // Make sure they provided a class name. We do this after
  // JNI_CreateJavaVM so that things like "-help" have the opportunity
  // to emit a usage statement.
  if (arg_idx == argc) {
    fprintf(stderr, "Class name required\n");
    return EXIT_FAILURE;
  }

  // Run InvokeMain on a NEW thread with a large stack.
  struct MainArgs { JNIEnv* env; char** argv; int argc; int rc; JavaVM* vm; };
  MainArgs margs = {env, &argv[arg_idx], argc - arg_idx, 0, vm};
  pthread_t main_thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 256 * 1024 * 1024);
  fprintf(stderr, "[dalvikvm] Spawning main thread with 256MB stack\n");
  int pt_rc = pthread_create(&main_thread, &attr, [](void* arg) -> void* {
    MainArgs* ma = (MainArgs*)arg;
    // Attach this thread to the VM
    JNIEnv* env2 = nullptr;
    JavaVMAttachArgs attach_args = {JNI_VERSION_1_6, "main-256mb", nullptr};
    ma->vm->AttachCurrentThread(&env2, &attach_args);
    ma->rc = InvokeMain(env2, ma->argv);
    return nullptr;
  }, &margs);
  pthread_attr_destroy(&attr);
  if (pt_rc == 0) {
    pthread_join(main_thread, nullptr);
  } else {
    fprintf(stderr, "[dalvikvm] pthread_create failed (%d), using original thread\n", pt_rc);
    margs.rc = InvokeMain(env, &argv[arg_idx]);
  }
  int rc = margs.rc;

  // In standalone builds, VM shutdown (DestroyJavaVM) crashes because thread groups
  // and daemon threads aren't fully initialized. Just exit directly.
  // Flush stderr so ExceptionDescribe output is not lost by _exit().
  fflush(stderr);
  fflush(stdout);
  _exit(rc);
}

}  // namespace art

// TODO(b/141622862): stop leaks
extern "C" const char *__asan_default_options() {
    return "detect_leaks=0";
}

// SIGBUS handler to log the faulting address and program counter
static void westlake_sigbus_handler(int sig, siginfo_t* info, void* ucontext_raw) {
  ucontext_t* uc = reinterpret_cast<ucontext_t*>(ucontext_raw);
  void* fault_addr = info->si_addr;
  void* pc = nullptr;
  void* lr = nullptr;
#if defined(__aarch64__)
  pc = reinterpret_cast<void*>(uc->uc_mcontext.pc);
  lr = reinterpret_cast<void*>(uc->uc_mcontext.regs[30]);
#endif
  char buf[256];
  int n = snprintf(buf, sizeof(buf),
      "\n[WESTLAKE_SIGBUS] sig=%d addr=%p pc=%p lr=%p\n"
      "[WESTLAKE_SIGBUS] x0=%p x1=%p x19=%p\n",
      sig, fault_addr, pc, lr,
#if defined(__aarch64__)
      reinterpret_cast<void*>(uc->uc_mcontext.regs[0]),
      reinterpret_cast<void*>(uc->uc_mcontext.regs[1]),
      reinterpret_cast<void*>(uc->uc_mcontext.regs[19])
#else
      nullptr, nullptr, nullptr
#endif
  );
  write(STDERR_FILENO, buf, n);

  // Try to get the current ART method from the Thread
  art::Thread* self = art::Thread::Current();
  if (self != nullptr) {
    const art::ManagedStack* stack = self->GetManagedStack();
    if (stack != nullptr) {
      art::ShadowFrame* frame = stack->GetTopShadowFrame();
      if (frame != nullptr) {
        art::ArtMethod* method = frame->GetMethod();
        if (method != nullptr) {
          const char* shorty = method->GetShorty();
          n = snprintf(buf, sizeof(buf),
              "[WESTLAKE_SIGBUS] current_method=%p native=%d entry=%p jni=%p shorty=%s\n",
              method, method->IsNative(),
              method->GetEntryPointFromQuickCompiledCode(),
              method->GetEntryPointFromJni(),
              shorty ? shorty : "?");
          write(STDERR_FILENO, buf, n);
          std::string pretty = method->PrettyMethod();
          n = snprintf(buf, sizeof(buf), "[WESTLAKE_SIGBUS] method: %s\n", pretty.c_str());
          write(STDERR_FILENO, buf, n);
        }
      }
    }
  }

  // Re-raise to get default crash handler
  signal(sig, SIG_DFL);
  raise(sig);
}

struct WestlakeDalvikMainArgs {
  int argc;
  char** argv;
  int result;
};

static void* WestlakeDalvikMainThread(void* opaque) {
  WestlakeDalvikMainArgs* args = reinterpret_cast<WestlakeDalvikMainArgs*>(opaque);
  args->result = art::dalvikvm(args->argc, args->argv);
  return nullptr;
}

int main(int argc, char** argv) {
  // Install SIGBUS handler to diagnose stale entry points
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = westlake_sigbus_handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGBUS, &sa, nullptr);

  // Westlake currently executes all app bytecode in ART's C++ interpreter.
  // Deep real-app startup paths, especially Gson/reflection/Kotlin stacks,
  // exhaust Android's inherited process-main stack before reaching UI. Run
  // the VM on an explicit pthread stack so this behavior is portable to OHOS
  // and does not depend on the launcher process rlimit.
  static constexpr size_t kWestlakeDalvikStackSize = 256 * 1024 * 1024;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  int attr_result = pthread_attr_setstacksize(&attr, kWestlakeDalvikStackSize);
  WestlakeDalvikMainArgs args = {argc, argv, 1};
  pthread_t thread;
  int create_result = attr_result == 0
      ? pthread_create(&thread, &attr, WestlakeDalvikMainThread, &args)
      : attr_result;
  pthread_attr_destroy(&attr);
  if (create_result == 0) {
    pthread_join(thread, nullptr);
    art::FastExit(args.result);
  }

  fprintf(stderr,
          "[dalvikvm] large-stack VM thread unavailable (%s); using process main stack\n",
          strerror(create_result));

  // Do not allow static destructors to be called, since it's conceivable that
  // daemons may still awaken (literally).
  art::FastExit(art::dalvikvm(argc, argv));
}
