#ifndef INCLUDE_CHAT_PROTOCOL_H
#define INCLUDE_CHAT_PROTOCOL_H

#include <string>
#include <string_view>
#include <vector>

namespace http {
    class ChatFrameParser {
    public:
        explicit ChatFrameParser(std::size_t maximumFrameSize = 1024);
        std::vector<std::string> push(std::string_view bytes);
    };
} // namespace http

#endif // INCLUDE_CHAT_PROTOCOL_H
