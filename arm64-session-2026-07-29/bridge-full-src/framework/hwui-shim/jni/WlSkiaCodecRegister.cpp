// WESTLAKE §611g — register skia image decoders into libskia_canvaskit's SkCodec registry.
//
// This libhwui links skia dynamically (SKIA_DLL, board libskia_canvaskit.z.so). Skia m1xx
// moved codec construction to an explicit registry (SkCodecs::Register); nothing in this
// process ever registered a decoder, so SkCodec::MakeFromStream returned null for EVERY
// raster image and BitmapFactory produced "requires a valid src" for every png consumer
// (§553 text-select handles, §611 cab_background / sym_def_app_icon / abc_textfield_*).
// The board skia EXPORTS the per-format Decoder pieces and Register itself — this TU just
// wires them together at load time. AVIF is not exported on this board; not registered.
#include "include/codec/SkPngDecoder.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/codec/SkWebpDecoder.h"
#include "include/codec/SkGifDecoder.h"
#include "include/codec/SkBmpDecoder.h"
#include "include/codec/SkIcoDecoder.h"
#include "include/codec/SkWbmpDecoder.h"

#include <stdio.h>

__attribute__((constructor)) static void wl_register_skia_codecs() {
    SkCodecs::Register(SkPngDecoder::Decoder());
    SkCodecs::Register(SkJpegDecoder::Decoder());
    SkCodecs::Register(SkWebpDecoder::Decoder());
    SkCodecs::Register(SkGifDecoder::Decoder());
    SkCodecs::Register(SkBmpDecoder::Decoder());
    SkCodecs::Register(SkIcoDecoder::Decoder());
    SkCodecs::Register(SkWbmpDecoder::Decoder());
    fprintf(stderr, "[WESTLAKE-611g] skia codecs registered (png jpeg webp gif bmp ico wbmp)\n");
    fflush(stderr);
}
