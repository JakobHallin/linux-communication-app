#include "chat_protocol.h"

#include <cctype>
#include <charconv>
#include <stdexcept>

namespace http {
    bool isValidDisplayName(std::string_view name) {
        if (name.empty() || name.size() > 32) {
            return false;
        }
        for (unsigned char character : name) {
            if (!(std::isalnum(character) || character == '_' || character == '-')) {
                return false;
            }
        }
        return true;
    }

    std::optional<std::uint16_t> parseVoicePort(std::string_view frame) {
        constexpr std::string_view prefix = "VOICE_PORT ";
        if (!frame.starts_with(prefix) || frame.size() == prefix.size()) {
            return std::nullopt;
        }

        const auto digits = frame.substr(prefix.size());
        unsigned int port = 0;
        const auto result = std::from_chars(digits.data(), digits.data() + digits.size(),
                                            port);
        if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size() ||
            port == 0 || port > 65'535) {
            return std::nullopt;
        }
        return static_cast<std::uint16_t>(port);
    }

    ChatFrameParser::ChatFrameParser(std::size_t maximumFrameSize)
        : maximumFrameSize(maximumFrameSize) {}

    std::vector<std::string> ChatFrameParser::push(std::string_view bytes) {
        pending.append(bytes);
        std::vector<std::string> frames;

        std::size_t newline = pending.find('\n');
        while (newline != std::string::npos) {
            std::string frame = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (!frame.empty() && frame.back() == '\r') {
                frame.pop_back();
            }
            if (frame.size() > maximumFrameSize) {
                throw std::length_error("chat frame exceeds maximum size");
            }
            frames.push_back(std::move(frame));
            newline = pending.find('\n');
        }

        if (pending.size() > maximumFrameSize) {
            throw std::length_error("chat frame exceeds maximum size");
        }
        return frames;
    }
} // namespace http
