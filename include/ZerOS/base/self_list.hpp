#pragma once

#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/base/traits.hpp"
#include <type_traits>
namespace ZerOS::base {
template <typename Stuff> struct SelfNode {
    Stuff* next_{};
};

template <typename Stuff>
concept SelfLinked = std::is_base_of_v<SelfNode<Stuff>, Stuff>;

template <SelfLinked Stuff, typename Less = ZerOS::traits::AlwaysFalse> struct SelfList {
    SelfList() = default;
    SelfList(const SelfList&) = delete;
    SelfList& operator=(const SelfList&) = delete;
    [[nodiscard]] BorrowedPtr<Stuff> head() const { return stuff; }

    void insert(BorrowedPtr<Stuff> new_stuff) {
        Stuff** slot = &stuff; // Lets walk!

        while (*slot && !less_(*new_stuff, **slot)) {
            slot = &(*slot)->next_;
        }

        new_stuff->next_ = *slot;
        *slot = new_stuff.get();

        // OK, quit here
    }
    bool remove(BorrowedPtr<Stuff> w) {
        Stuff** slot = &stuff;
        while (*slot != nullptr) {
            if (*slot == w) {
                *slot = (*slot)->next_;
                w->next_ = nullptr;
                return true;
            }
            slot = &(*slot)->next_;
        }
        return false;
    }

    [[nodiscard]] BorrowedPtr<Stuff> pop_head() {
        Stuff* out = stuff;
        if (out != nullptr) {
            stuff = out->next_;
            out->next_ = nullptr;
        }
        return out;
    }
    [[nodiscard]] bool empty() const { return stuff == nullptr; }

  private:
    Stuff* stuff{nullptr};
    // if we dont set anything, place it null
    // so a self hold list is same as Stuff*
    [[no_unique_address]] Less less_{};
};

namespace zeros_impl {
struct Probe : SelfNode<Probe> {};
} // namespace zeros_impl

static_assert(SelfLinked<zeros_impl::Probe>);
static_assert(std::is_trivially_destructible_v<SelfList<zeros_impl::Probe>>);
static_assert(sizeof(SelfList<zeros_impl::Probe>) == sizeof(zeros_impl::Probe*),
              "SelfList is not Zero cost");
} // namespace ZerOS::base
