// SkOpts::hash compat: OHOS render_service_base uses SkOpts::hash which skia m133
// moved to SkChecksum::Hash. Provide it forwarding to m133's SkChecksum.
#ifndef SKOPTS_HASH_COMPAT_H
#define SKOPTS_HASH_COMPAT_H
#include "src/core/SkChecksum.h"
namespace SkOpts {
  inline uint32_t hash(const void* data, size_t bytes, uint32_t seed = 0) {
    return SkChecksum::Hash32(data, bytes, seed);
  }
}
#endif
