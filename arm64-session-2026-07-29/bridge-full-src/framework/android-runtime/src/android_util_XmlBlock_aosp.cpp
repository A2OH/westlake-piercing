#include <cstdlib>
/* //device/libs/android_runtime/android_util_XmlBlock.cpp
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define LOG_TAG "XmlBlock"

#include "jni.h"
#include <nativehelper/JNIHelp.h>
#include <core_jni_helpers.h>
#include <androidfw/AssetManager.h>
#include <androidfw/ResourceTypes.h>
#include <utils/Log.h>
#include <utils/misc.h>

#include <stdio.h>

namespace android {
constexpr int kNullDocument = UNEXPECTED_NULL;
// The reason not to ResXMLParser::BAD_DOCUMENT which is -1 is that other places use the same value.
constexpr int kBadDocument = BAD_VALUE;

// ----------------------------------------------------------------------------

static jlong android_content_XmlBlock_nativeCreate(JNIEnv* env, jobject clazz,
                                               jbyteArray bArray,
                                               jint off, jint len)
{
    if (bArray == NULL) {
        jniThrowNullPointerException(env, NULL);
        return 0;
    }

    jsize bLen = env->GetArrayLength(bArray);
    if (off < 0 || off >= bLen || len < 0 || len > bLen || (off+len) > bLen) {
        jniThrowException(env, "java/lang/IndexOutOfBoundsException", NULL);
        return 0;
    }

    jbyte* b = env->GetByteArrayElements(bArray, NULL);
    ResXMLTree* osb = new ResXMLTree();
    osb->setTo(b+off, len, true);
    env->ReleaseByteArrayElements(bArray, b, 0);

    if (osb->getError() != NO_ERROR) {
        jniThrowException(env, "java/lang/IllegalArgumentException", NULL);
        return 0;
    }

    return reinterpret_cast<jlong>(osb);
}

static jlong android_content_XmlBlock_nativeGetStringBlock(JNIEnv* env, jobject clazz,
                                                       jlong token)
{
    ResXMLTree* osb = reinterpret_cast<ResXMLTree*>(token);
    if (osb == NULL) {
        jniThrowNullPointerException(env, NULL);
        return 0;
    }

    return reinterpret_cast<jlong>(&osb->getStrings());
}


// ===========================================================================
// WESTLAKE (arm64 board, 2026-07-21) — parse-state TLS fallback.
//
// PROVEN defect: this runtime loses the `jlong` RETURN of the static native
// `XmlBlock.nativeCreateParseState` (shorty `(JI)J`).  Arguments arrive perfectly
// (verified: res_id == the real drawable id, tree pointer valid) but Java receives 0,
// even when the native returns a forced non-zero sentinel.  Consequence:
// `XmlBlock$Parser.mParseState == 0`, so `next()` short-circuits to END_DOCUMENT without
// ever calling `nativeNext`, and every compiled-XML inflate fails with
// "XmlPullParserException: No start tag found" -> no drawable -> no frame.
// (A static `(J)J` native such as ApkAssets.nativeGetStringBlock returns fine, so the
// defect is specific to this shorty — see the memory notes for the ART-side fix.)
//
// WORKAROUND until ART is fixed: remember the parser we just created in a thread-local
// LIFO and, whenever a parse-state native is handed token==0, use the most recent one.
// XML inflation is strictly nested per thread, so a stack gives the right parser.
// ===========================================================================
#include <vector>
static thread_local std::vector<ResXMLParser*> g_westlake_parser_stack;

// WESTLAKE (2026-07-22): TLS-FIRST.  `XmlBlock$Parser.mParseState` cannot be trusted on
// this runtime -- the Java-side wide field read does not return what
// `nativeCreateParseState` handed back (see the boot-image/dex field-offset notes), so the
// token arriving here is meaningless (observed: 0, or a stale 1).  The thread-local stack is
// authoritative: XML inflation is strictly nested per thread, so the most recently created
// parser is the one the caller means.  Only fall back to the raw token if the stack is empty.
// WESTLAKE §216 (2026-07-22) TOKEN-FIRST -- REVERSES the TLS-first rule above.
// The "strictly nested per thread, so the newest parser is the one meant" assumption is FALSE and
// was silently truncating layouts: while inflating a child (e.g. BottomNavigationView) the framework
// creates NESTED parsers for its <selector>/drawable resources, which are pushed on this stack. The
// OUTER layout's next() then received the nested (already finished) parser and got END_DOCUMENT, so
// rInflate's child loop ended early and every remaining sibling was silently dropped.
// Measured on home_fragment (res_id 0x7f0c0043): the stream went
//   START <ConstraintLayout> / START+END <FragmentContainerView> / START <BottomNavigationView>
//   -> END_DOCUMENT, never emitting the 3rd child (playback_controller) -> §207's
//   "Missing required view with ID: .../playback_controller".
// The token is now RELIABLE (createParseState logs parser=0x7f01753950 for res_id=0x7f0c0043 and
// nativeNext receives exactly that value), so trust it whenever it is a plausible pointer and keep
// the TLS stack only as a fallback for the old broken-token case.
static inline ResXMLParser* WestlakeParserFromToken(jlong token) {
    const uintptr_t t = static_cast<uintptr_t>(token);
    if (t > 0x10000u && (t & 3u) == 0u) {
        return reinterpret_cast<ResXMLParser*>(token);
    }
    if (!g_westlake_parser_stack.empty()) {
        return g_westlake_parser_stack.back();
    }
    return nullptr;
}

static jlong android_content_XmlBlock_nativeCreateParseState(JNIEnv* env, jobject clazz,
                                                          jlong token, jint res_id)
{
    ResXMLTree* osb = reinterpret_cast<ResXMLTree*>(token);
    if (osb == NULL) {
        jniThrowNullPointerException(env, NULL);
        return 0;
    }

    ResXMLParser* st = new ResXMLParser(*osb);
    if (st == NULL) {
        jniThrowException(env, "java/lang/OutOfMemoryError", NULL);
        return 0;
    }

    st->setSourceResourceId(res_id);
    st->restart();
    g_westlake_parser_stack.push_back(st);   // see WestlakeParserFromToken

    fprintf(stderr, "[WESTLAKE-XMLP] createParseState tree=%p parser=%p firstEvent=%d res_id=0x%x ret=0x%llx\n",
            (void*)osb, (void*)st, (int)st->getEventType(), (unsigned)res_id,
            (unsigned long long)reinterpret_cast<jlong>(st)); fflush(stderr);
    return reinterpret_cast<jlong>(st);
}

static jint android_content_XmlBlock_nativeNext(CRITICAL_JNI_PARAMS_COMMA jlong token) {
    // WESTLAKE diag — the counter is PROCESS-WIDE and the child parses many XMLs
    // (manifest/themes) during bind before any drawable is inflated, so a small cap hides
    // exactly the calls that matter.  Keep it generous.
    static int westlake_next_calls = 0;
    const bool westlake_log = (++westlake_next_calls <= 4000);  // §215b: 400 was cut mid-layout
    if (westlake_log) {
        fprintf(stderr, "[WESTLAKE-XMLP] nativeNext#%d token=0x%llx\n",
                westlake_next_calls, (unsigned long long)token); fflush(stderr);
    }
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return ResXMLParser::END_DOCUMENT;
    }

    do {
        ResXMLParser::event_code_t code = st->next();
        // WESTLAKE §215: log the RETURNED EVENT, not just the call. §214 showed rInflate's child
        // loop ends one element early (home_fragment's 3rd child is never processed), so the
        // question is whether this parser emits END_TAG/END_DOCUMENT too soon. Also print the
        // element name on START/END_TAG so the event stream can be matched to the layout.
        jint wl_ret = -1;
        switch (code) {
            case ResXMLParser::START_TAG:      wl_ret = 2; break;
            case ResXMLParser::END_TAG:        wl_ret = 3; break;
            case ResXMLParser::TEXT:           wl_ret = 4; break;
            case ResXMLParser::START_DOCUMENT: wl_ret = 0; break;
            case ResXMLParser::END_DOCUMENT:   wl_ret = 1; break;
            case ResXMLParser::BAD_DOCUMENT:   goto bad;
            default: break;
        }
        if (wl_ret >= 0) {
            if (westlake_log) {
                const char* wl_ev = (wl_ret == 2) ? "START_TAG"
                                  : (wl_ret == 3) ? "END_TAG"
                                  : (wl_ret == 4) ? "TEXT"
                                  : (wl_ret == 0) ? "START_DOC" : "END_DOC";
                size_t wl_len = 0;
                const char16_t* wl_nm16 =
                    (wl_ret == 2 || wl_ret == 3) ? st->getElementName(&wl_len) : NULL;
                char wl_nm[96];
                wl_nm[0] = '\0';
                if (wl_nm16 != NULL) {
                    size_t wl_i = 0;
                    for (; wl_i < wl_len && wl_i < sizeof(wl_nm) - 1; ++wl_i) {
                        char16_t c16 = wl_nm16[wl_i];
                        wl_nm[wl_i] = (c16 >= 0x20 && c16 < 0x7f) ? (char)c16 : '?';
                    }
                    wl_nm[wl_i] = '\0';
                }
                // §215b: include the token -- nested resource parses (e.g. <selector> drawables
                // pulled in while inflating a child) interleave with the layout's own stream, and
                // without the token they look like part of the layout.
                fprintf(stderr, "[WESTLAKE-XMLEV] #%d tok=0x%llx %s <%s>\n",
                        westlake_next_calls, (unsigned long long)token, wl_ev, wl_nm);
                fflush(stderr);
            }
            return wl_ret;
        }
    } while (true);

bad:
    return kBadDocument;
}

static jint android_content_XmlBlock_nativeGetNamespace(CRITICAL_JNI_PARAMS_COMMA jlong token) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return -1;
    }

    return static_cast<jint>(st->getElementNamespaceID());
}

static jint android_content_XmlBlock_nativeGetName(CRITICAL_JNI_PARAMS_COMMA jlong token) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return -1;
    }

    return static_cast<jint>(st->getElementNameID());
}

static jint android_content_XmlBlock_nativeGetText(CRITICAL_JNI_PARAMS_COMMA jlong token) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return -1;
    }

    return static_cast<jint>(st->getTextID());
}

static jint android_content_XmlBlock_nativeGetLineNumber(CRITICAL_JNI_PARAMS_COMMA jlong token) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return kNullDocument;
    }

    return static_cast<jint>(st->getLineNumber());
}

static jint android_content_XmlBlock_nativeGetAttributeCount(
        CRITICAL_JNI_PARAMS_COMMA jlong token) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return kNullDocument;
    }

    return static_cast<jint>(st->getAttributeCount());
}

static jint android_content_XmlBlock_nativeGetAttributeNamespace(
        CRITICAL_JNI_PARAMS_COMMA jlong token, jint idx) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return kNullDocument;
    }

    return static_cast<jint>(st->getAttributeNamespaceID(idx));
}

static jint android_content_XmlBlock_nativeGetAttributeName(CRITICAL_JNI_PARAMS_COMMA jlong token,
                                                            jint idx) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return kNullDocument;
    }

    return static_cast<jint>(st->getAttributeNameID(idx));
}

static jint android_content_XmlBlock_nativeGetAttributeResource(
        CRITICAL_JNI_PARAMS_COMMA jlong token, jint idx) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return kNullDocument;
    }

    return static_cast<jint>(st->getAttributeNameResID(idx));
}

static jint android_content_XmlBlock_nativeGetAttributeDataType(
        CRITICAL_JNI_PARAMS_COMMA jlong token, jint idx) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return kNullDocument;
    }

    return static_cast<jint>(st->getAttributeDataType(idx));
}

static jint android_content_XmlBlock_nativeGetAttributeData(CRITICAL_JNI_PARAMS_COMMA jlong token,
                                                            jint idx) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return kNullDocument;
    }

    return static_cast<jint>(st->getAttributeData(idx));
}

static jint android_content_XmlBlock_nativeGetAttributeStringValue(
        CRITICAL_JNI_PARAMS_COMMA jlong token, jint idx) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return kNullDocument;
    }

    return static_cast<jint>(st->getAttributeValueStringID(idx));
}

static jint android_content_XmlBlock_nativeGetAttributeIndex(JNIEnv* env, jobject clazz,
                                                             jlong token,
                                                             jstring ns, jstring name)
{
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL || name == NULL) {
        jniThrowNullPointerException(env, NULL);
        return 0;
    }

    const char16_t* ns16 = NULL;
    jsize nsLen = 0;
    if (ns) {
        ns16 = reinterpret_cast<const char16_t*>(env->GetStringChars(ns, NULL));
        nsLen = env->GetStringLength(ns);
    }

    const char16_t* name16 = reinterpret_cast<const char16_t*>(
        env->GetStringChars(name, NULL));
    jsize nameLen = env->GetStringLength(name);

    jint idx = static_cast<jint>(st->indexOfAttribute(ns16, nsLen, name16, nameLen));

    if (ns) {
        env->ReleaseStringChars(ns, reinterpret_cast<const jchar*>(ns16));
    }
    env->ReleaseStringChars(name, reinterpret_cast<const jchar*>(name16));

    return idx;
}

static jint android_content_XmlBlock_nativeGetIdAttribute(CRITICAL_JNI_PARAMS_COMMA jlong token) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return kNullDocument;
    }

    ssize_t idx = st->indexOfID();
    return idx >= 0 ? static_cast<jint>(st->getAttributeValueStringID(idx)) : -1;
}

static jint android_content_XmlBlock_nativeGetClassAttribute(
        CRITICAL_JNI_PARAMS_COMMA jlong token) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return kNullDocument;
    }

    ssize_t idx = st->indexOfClass();
    return idx >= 0 ? static_cast<jint>(st->getAttributeValueStringID(idx)) : -1;
}

static jint android_content_XmlBlock_nativeGetStyleAttribute(
        CRITICAL_JNI_PARAMS_COMMA jlong token) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return kNullDocument;
    }

    ssize_t idx = st->indexOfStyle();
    if (idx < 0) {
        return 0;
    }

    Res_value value;
    if (st->getAttributeValue(idx, &value) < 0) {
        return 0;
    }

    return value.dataType == value.TYPE_REFERENCE
        || value.dataType == value.TYPE_ATTRIBUTE
        ? value.data : 0;
}

static jint android_content_XmlBlock_nativeGetSourceResId(CRITICAL_JNI_PARAMS_COMMA jlong token) {
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        return 0;
    } else {
        return st->getSourceResourceId();
    }
}

static void android_content_XmlBlock_nativeDestroyParseState(JNIEnv* env, jobject clazz,
                                                          jlong token)
{
    ResXMLParser* st = WestlakeParserFromToken(token);
    if (st == NULL) {
        jniThrowNullPointerException(env, NULL);
        return;
    }

    // Keep the TLS stack in sync -- otherwise it would hold a dangling pointer.
    if (!g_westlake_parser_stack.empty() && g_westlake_parser_stack.back() == st) {
        g_westlake_parser_stack.pop_back();
    }
    fprintf(stderr, "[WESTLAKE-XMLP] destroyParseState parser=%p token=0x%llx depth=%zu\n",
            (void*)st, (unsigned long long)token, g_westlake_parser_stack.size()); fflush(stderr);
    delete st;
}

static void android_content_XmlBlock_nativeDestroy(JNIEnv* env, jobject clazz,
                                                   jlong token)
{
    ResXMLTree* osb = reinterpret_cast<ResXMLTree*>(token);
    if (osb == NULL) {
        jniThrowNullPointerException(env, NULL);
        return;
    }

    delete osb;
}

// ----------------------------------------------------------------------------

/*
 * JNI registration.
 */
static const JNINativeMethod gXmlBlockMethods[] = {
    /* name, signature, funcPtr */
    { "nativeCreate",               "([BII)J",
            (void*) android_content_XmlBlock_nativeCreate },
    { "nativeGetStringBlock",       "(J)J",
            (void*) android_content_XmlBlock_nativeGetStringBlock },
    { "nativeCreateParseState",     "(JI)J",
            (void*) android_content_XmlBlock_nativeCreateParseState },
    { "nativeDestroyParseState",    "(J)V",
            (void*) android_content_XmlBlock_nativeDestroyParseState },
    { "nativeDestroy",              "(J)V",
            (void*) android_content_XmlBlock_nativeDestroy },

    // ------------------- @FastNative ----------------------

    { "nativeNext",                 "(J)I",
            (void*) android_content_XmlBlock_nativeNext },
    { "nativeGetNamespace",         "(J)I",
            (void*) android_content_XmlBlock_nativeGetNamespace },
    { "nativeGetName",              "(J)I",
            (void*) android_content_XmlBlock_nativeGetName },
    { "nativeGetText",              "(J)I",
            (void*) android_content_XmlBlock_nativeGetText },
    { "nativeGetLineNumber",        "(J)I",
            (void*) android_content_XmlBlock_nativeGetLineNumber },
    { "nativeGetAttributeCount",    "(J)I",
            (void*) android_content_XmlBlock_nativeGetAttributeCount },
    { "nativeGetAttributeNamespace","(JI)I",
            (void*) android_content_XmlBlock_nativeGetAttributeNamespace },
    { "nativeGetAttributeName",     "(JI)I",
            (void*) android_content_XmlBlock_nativeGetAttributeName },
    { "nativeGetAttributeResource", "(JI)I",
            (void*) android_content_XmlBlock_nativeGetAttributeResource },
    { "nativeGetAttributeDataType", "(JI)I",
            (void*) android_content_XmlBlock_nativeGetAttributeDataType },
    { "nativeGetAttributeData",    "(JI)I",
            (void*) android_content_XmlBlock_nativeGetAttributeData },
    { "nativeGetAttributeStringValue", "(JI)I",
            (void*) android_content_XmlBlock_nativeGetAttributeStringValue },
    { "nativeGetAttributeIndex",    "(JLjava/lang/String;Ljava/lang/String;)I",
            (void*) android_content_XmlBlock_nativeGetAttributeIndex },
    { "nativeGetIdAttribute",      "(J)I",
            (void*) android_content_XmlBlock_nativeGetIdAttribute },
    { "nativeGetClassAttribute",   "(J)I",
            (void*) android_content_XmlBlock_nativeGetClassAttribute },
    { "nativeGetStyleAttribute",   "(J)I",
            (void*) android_content_XmlBlock_nativeGetStyleAttribute },
    { "nativeGetSourceResId",      "(J)I",
            (void*) android_content_XmlBlock_nativeGetSourceResId},
};

int register_android_content_XmlBlock(JNIEnv* env)
{
    return RegisterMethodsOrDie(env,
            "android/content/res/XmlBlock", gXmlBlockMethods, NELEM(gXmlBlockMethods));
}

}; // namespace android

// ===========================================================================
// WESTLAKE (2026-07-22) -- EXPORT the @CriticalNative entry points.
//
// PROVEN on this board: `RegisterNatives` is NOT honoured for @CriticalNative methods -- ART
// resolves them by dlsym on the mangled `Java_<class>_<method>` symbol instead.  Evidence:
// `nativeCreateParseState` / `nativeDestroyParseState` (plain natives, same table) both run and
// log, while `nativeNext` (@CriticalNative, registered from that same table, no
// UnsatisfiedLinkError) was NEVER entered -- so `XmlBlock$Parser.next()` fell straight through and
// every compiled-XML drawable failed with "No start tag found".  This is the same shape as the
// four @CriticalNative font release-funcs that had to be exported earlier in this port.
//
// Exporting these makes the dlsym path find them.  @CriticalNative takes no JNIEnv*/jclass, which
// is exactly the internal signature (CRITICAL_JNI_PARAMS_COMMA expands to nothing here).
// ===========================================================================
extern "C" {
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeNext(jlong token) {
    return android::android_content_XmlBlock_nativeNext(token);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetNamespace(jlong token) {
    return android::android_content_XmlBlock_nativeGetNamespace(token);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetName(jlong token) {
    return android::android_content_XmlBlock_nativeGetName(token);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetText(jlong token) {
    return android::android_content_XmlBlock_nativeGetText(token);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetLineNumber(jlong token) {
    return android::android_content_XmlBlock_nativeGetLineNumber(token);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetAttributeCount(jlong token) {
    return android::android_content_XmlBlock_nativeGetAttributeCount(token);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetIdAttribute(jlong token) {
    return android::android_content_XmlBlock_nativeGetIdAttribute(token);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetClassAttribute(jlong token) {
    return android::android_content_XmlBlock_nativeGetClassAttribute(token);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetStyleAttribute(jlong token) {
    return android::android_content_XmlBlock_nativeGetStyleAttribute(token);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetSourceResId(jlong token) {
    return android::android_content_XmlBlock_nativeGetSourceResId(token);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetAttributeNamespace(jlong token, jint idx) {
    return android::android_content_XmlBlock_nativeGetAttributeNamespace(token, idx);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetAttributeName(jlong token, jint idx) {
    return android::android_content_XmlBlock_nativeGetAttributeName(token, idx);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetAttributeResource(jlong token, jint idx) {
    return android::android_content_XmlBlock_nativeGetAttributeResource(token, idx);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetAttributeDataType(jlong token, jint idx) {
    return android::android_content_XmlBlock_nativeGetAttributeDataType(token, idx);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetAttributeData(jlong token, jint idx) {
    return android::android_content_XmlBlock_nativeGetAttributeData(token, idx);
}
JNIEXPORT jint Java_android_content_res_XmlBlock_nativeGetAttributeStringValue(jlong token, jint idx) {
    return android::android_content_XmlBlock_nativeGetAttributeStringValue(token, idx);
}
}  // extern "C"

