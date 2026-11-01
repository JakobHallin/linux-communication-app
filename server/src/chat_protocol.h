#ifndef INCLUDE_CHAT_PROTOCOL_H
#define INCLUDE_CHAT_PROTOCOL_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace http {
    bool isValidDisplayName(std::string_view name);
    std::optional<std::uint16_t> parseVoicePort(std::string_view frame);

    class ChatFrameParser {
    public:
        explicit ChatFrameParser(std::size_t maximumFrameSize = 1024);
        std::vector<std::string> push(std::string_view bytes);

    private:
        std::size_t maximumFrameSize;
        std::string pending;
    };
} // namespace http

#endif // INCLUDE_CHAT_PROTOCOL_H
