#include "audio_codec.h"

#include <opus/opus.h>

#include <algorithm>
#include <stdexcept>

namespace media {
    OpusCodec::OpusCodec() {
        int error = OPUS_OK;
        encoder = opus_encoder_create(static_cast<opus_int32>(sampleRate), 1,
                                      OPUS_APPLICATION_AUDIO, &error);
        if (encoder == nullptr || error != OPUS_OK) {
            throw std::runtime_error("failed to create Opus encoder");
        }

        decoder = opus_decoder_create(static_cast<opus_int32>(sampleRate), 1,
                                      &error);
        if (decoder == nullptr || error != OPUS_OK) {
            opus_encoder_destroy(encoder);
            encoder = nullptr;
            throw std::runtime_error("failed to create Opus decoder");
        }

        opus_encoder_ctl(encoder, OPUS_SET_BITRATE(32'000));
        opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(5));
        opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(5));
    }

    OpusCodec::~OpusCodec() {
        opus_decoder_destroy(decoder);
        opus_encoder_destroy(encoder);
    }

    std::vector<std::uint8_t> OpusCodec::encode(
        std::span<const std::int16_t> samples) {
        if (samples.size() != frameSamples || encoder == nullptr) {
            return {};
        }

        std::vector<std::uint8_t> packet(maximumAudioPacketBytes);
        const int encodedBytes = opus_encode(
            encoder, samples.data(), static_cast<int>(frameSamples),
            packet.data(), static_cast<opus_int32>(packet.size()));
        if (encodedBytes < 0) {
            return {};
        }
        packet.resize(static_cast<std::size_t>(encodedBytes));
        return packet;
    }

    std::optional<std::vector<std::int16_t>> OpusCodec::decode(
        std::span<const std::uint8_t> packet) {
        if (packet.empty() || packet.size() > maximumAudioPacketBytes ||
            decoder == nullptr) {
            return std::nullopt;
        }

        std::vector<std::int16_t> samples(frameSamples);
        const int decodedSamples = opus_decode(
            decoder, packet.data(), static_cast<opus_int32>(packet.size()),
            samples.data(), static_cast<int>(frameSamples), 0);
        if (decodedSamples < 0) {
            return std::nullopt;
        }
        samples.resize(static_cast<std::size_t>(decodedSamples));
        return samples;
    }
}
