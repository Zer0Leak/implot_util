#pragma once

#include <functional>

template <class F> class ScopeFail {
  public:
    explicit ScopeFail(F &&f) noexcept(std::is_nothrow_constructible_v<F, F &&>) : f_(std::forward<F>(f)) {}

    ScopeFail(const ScopeFail &) = delete;
    ScopeFail &operator=(const ScopeFail &) = delete;

    ScopeFail(ScopeFail &&other) noexcept(std::is_nothrow_move_constructible_v<F>)
        : f_(std::move(other.f_)), active_(other.active_), uncaught_(other.uncaught_) {
        other.release();
    }

    ~ScopeFail() noexcept(noexcept(std::declval<F &>()())) {
        if (active_ && std::uncaught_exceptions() > uncaught_) {
            f_();
        }
    }

    void release() noexcept { active_ = false; }

  private:
    F f_;
    bool active_ = true;
    int uncaught_ = std::uncaught_exceptions();
};

// CTAD helper
template <class F> ScopeFail(F) -> ScopeFail<F>;