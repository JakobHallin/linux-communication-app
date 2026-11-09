#include "screen_frame_encoder.h"

#include "mjpeg_frame.h"

#include <cstdio>
#include <csetjmp>
#include <cstdlib>
#include <utility>

#include <jpeglib.h>

namespace media {
    namespace {
        struct JpegErrorManager {
            jpeg_error_mgr base{};
            std::jmp_buf jump{};
        };

        void jpegErrorExit(j_common_ptr common) {
            auto* errors = reinterpret_cast<JpegErrorManager*>(common->err);
            longjmp(errors->jump, 1);
        }
    }

    std::optional<VideoFrame> makeMjpegVideoFrameFromBgra(
        std::uint32_t sessionId,
        std::uint32_t senderId,
        std::uint32_t sequence,
        const BgraFrame& frame,
        int quality) {
        const auto minimumStride = static_cast<std::int32_t>(frame.width) * 4;
        if (frame.width == 0 || frame.height == 0 || frame.stride < minimumStride ||
            frame.pixels.size() < static_cast<std::size_t>(frame.stride) * frame.height ||
            quality < 1 || quality > 100) {
            return std::nullopt;
        }

        jpeg_compress_struct compressor{};
        JpegErrorManager errors{};
        compressor.err = jpeg_std_error(&errors.base);
        errors.base.error_exit = jpegErrorExit;

        unsigned char* encoded = nullptr;
        unsigned long encodedSize = 0;
        if (setjmp(errors.jump) != 0) {
            jpeg_destroy_compress(&compressor);
            std::free(encoded);
            return std::nullopt;
        }

        jpeg_create_compress(&compressor);
        jpeg_mem_dest(&compressor, &encoded, &encodedSize);
        compressor.image_width = frame.width;
        compressor.image_height = frame.height;
        compressor.input_components = 3;
        compressor.in_color_space = JCS_RGB;
        jpeg_set_defaults(&compressor);
        jpeg_set_quality(&compressor, quality, TRUE);
        jpeg_start_compress(&compressor, TRUE);

        std::vector<JSAMPLE> row(static_cast<std::size_t>(frame.width) * 3);
        while (compressor.next_scanline < compressor.image_height) {
            const auto* source = frame.pixels.data() +
                static_cast<std::size_t>(compressor.next_scanline) * frame.stride;
            for (std::uint16_t x = 0; x < frame.width; ++x) {
                row[x * 3] = source[x * 4 + 2];
                row[x * 3 + 1] = source[x * 4 + 1];
                row[x * 3 + 2] = source[x * 4];
            }
            JSAMPROW rows[] = {row.data()};
            jpeg_write_scanlines(&compressor, rows, 1);
        }
        jpeg_finish_compress(&compressor);

        std::vector<std::uint8_t> payload(encoded, encoded + encodedSize);
        std::free(encoded);
        jpeg_destroy_compress(&compressor);
        return makeMjpegVideoFrame(sessionId, senderId, sequence,
                                   frame.width, frame.height, std::move(payload));
    }
}
