#pragma once

namespace miniredis::resp::detail {

template <typename... Fs>
struct Overloaded : Fs... {
    using Fs::operator()...;
};

template <typename... Fs>
Overloaded(Fs...) -> Overloaded<Fs...>;

}  // namespace miniredis::resp::detail
