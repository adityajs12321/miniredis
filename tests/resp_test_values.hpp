#pragma once

#include <random>
#include <string>
#include <vector>

#include "miniredis/resp/resp_value.hpp"

namespace miniredis::resp::testing {

inline int nextInt(std::mt19937_64& random, int bound) {
    return std::uniform_int_distribution<int>(0, bound - 1)(random);
}

inline std::string randomLine(std::mt19937_64& random) {
    std::string s;
    const int n = nextInt(random, 16);
    for (int i = 0; i < n; ++i) {
        s += static_cast<char>(' ' + nextInt(random, 95));
    }
    return s;
}

inline RespValue randomValue(std::mt19937_64& random, int maxDepth) {
    switch (nextInt(random, maxDepth > 0 ? 7 : 5)) {
        case 0:
            return SimpleString(randomLine(random));
        case 1:
            return SimpleError(randomLine(random));
        case 2:
            return Integer{static_cast<std::int64_t>(random())};
        case 3: {
            std::string data(static_cast<std::size_t>(nextInt(random, 32)), '\0');
            for (char& c : data) {
                c = static_cast<char>(nextInt(random, 256));
            }
            return BulkString::of(std::move(data));
        }
        case 4:
            return BulkString::null();
        case 5:
            return Array::null();
        default: {
            const int n = nextInt(random, 5);
            std::vector<RespValue> elements;
            elements.reserve(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) {
                elements.push_back(randomValue(random, maxDepth - 1));
            }
            return Array{std::move(elements)};
        }
    }
}

}  // namespace miniredis::resp::testing
