#ifndef AOSP_BRIDGE_COMPAT_H
#define AOSP_BRIDGE_COMPAT_H
#include <cstdint>
// AOSP-11 vs newer-AOSP shims used by the bridge (arm64 port)
namespace android { using ToolType = int32_t; }
using ToolType = int32_t;
#endif
