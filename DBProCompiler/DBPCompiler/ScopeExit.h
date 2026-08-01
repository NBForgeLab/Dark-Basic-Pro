#pragma once

#include <type_traits>
#include <utility>

template <typename Function>
class ScopeExit final {
public:
    explicit ScopeExit(Function function) noexcept(
        std::is_nothrow_move_constructible_v<Function>)
        : function_(std::move(function)) {
        static_assert(
            std::is_nothrow_invocable_v<Function&>,
            "Scope-exit callbacks must not throw");
    }

    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ScopeExit(ScopeExit&&) = delete;
    ScopeExit& operator=(ScopeExit&&) = delete;

    ~ScopeExit() noexcept {
        function_();
    }

private:
    Function function_;
};

template <typename Function>
[[nodiscard]] auto MakeScopeExit(Function&& function) noexcept(
    std::is_nothrow_constructible_v<std::decay_t<Function>, Function&&>) {
    return ScopeExit<std::decay_t<Function>>{
        std::forward<Function>(function)};
}
