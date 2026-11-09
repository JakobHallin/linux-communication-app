#pragma once

#include <cstdlib>
#include <iostream>

namespace test {
    inline void check(bool passed, const char* expression,
                      const char* file, int line) {
        if (!passed) {
            std::cerr << file << ':' << line << ": check failed: "
                      << expression << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
}

// Unlike assert(), checks and their side effects run in Release builds too.
#define TEST_CHECK(...) \
    ::test::check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__, __FILE__, __LINE__)
