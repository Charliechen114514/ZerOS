#pragma once

#include "ZerOS/base/helpful_macros.hpp"

namespace ZerOS::irq {
template <typename IsCriticalSection>
concept CriticalSection = requires(IsCriticalSection& t) {
    t.lock();
    t.unlock();
};

template <CriticalSection Lockable> class CriticalGuard {
  public:
    [[nodiscard]] explicit CriticalGuard(Lockable& target) : lockable_(target) { lockable_.lock(); }
    ~CriticalGuard() { lockable_.unlock(); }

  private:
    DISABLE_COPY_MOVE(CriticalGuard);

  private:
    Lockable& lockable_;
};

} // namespace ZerOS::irq