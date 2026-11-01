#include "chat_protocol.h"

#include <stdexcept>

namespace http {
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
