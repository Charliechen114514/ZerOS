#pragma once

#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/base/traits.hpp"
#include <type_traits>
namespace ZerOS::base {
template <typename Stuff> struct SelfNode {
  private:
    // Only the list may walk the chain: a stray hand wiring nodes behind
    // our back is how ready queues die quietly
    template <typename, typename> friend struct SelfList;
    Stuff* next_{};
};

template <typename Stuff>
concept SelfLinked = std::is_base_of_v<SelfNode<Stuff>, Stuff>;

template <typename Stuff, typename Less = ZerOS::traits::AlwaysFalse> struct SelfList {
    // the walk needs the node: stuff without a SelfNode simply has no road in
    static_assert(SelfLinked<Stuff>, "SelfList walks SelfLinked stuff only");

    SelfList() = default;
    SelfList(const SelfList&) = delete;
    SelfList& operator=(const SelfList&) = delete;
    [[nodiscard]] BorrowedPtr<Stuff> head() const { return stuff; }

    // sorted insert (Less comparator) — walks to the right spot
    void insert(BorrowedPtr<Stuff> new_stuff) {
        Stuff** slot = &stuff; // Lets walk!

        while (*slot && !less_(*new_stuff, **slot)) {
            slot = &(*slot)->next_;
        }

        new_stuff->next_ = *slot;
        *slot = new_stuff.get();
        if (new_stuff->next_ == nullptr) {
            tail_ = new_stuff.get(); // landed at the end
        }
    }

    // O(1) append — no walk, tail is right there. Use when order doesn't
    // matter (round-robin queues); the sorted insert above is for deadlines.
    void push_back(BorrowedPtr<Stuff> new_stuff) {
        new_stuff->next_ = nullptr;
        if (tail_ == nullptr) {
            stuff = new_stuff.get(); // list was empty
        } else {
            tail_->next_ = new_stuff.get();
        }
        tail_ = new_stuff.get();
    }

    bool remove(BorrowedPtr<Stuff> w) {
        Stuff** slot = &stuff;
        Stuff* prev = nullptr;
        while (*slot != nullptr) {
            if (*slot == w) {
                *slot = (*slot)->next_;
                if (w.get() == tail_) {
                    tail_ = prev; // removed the last node
                }
                w->next_ = nullptr;
                return true;
            }
            prev = *slot;
            slot = &(*slot)->next_;
        }
        return false;
    }

    void push_front(BorrowedPtr<Stuff> new_stuff) {
        new_stuff->next_ = stuff;
        stuff = new_stuff.get();
        if (tail_ == nullptr) {
            tail_ = new_stuff.get(); // list was empty
        }
    }

    [[nodiscard]] BorrowedPtr<Stuff> pop_head() {
        Stuff* out = stuff;
        if (out != nullptr) {
            stuff = out->next_;
            if (stuff == nullptr) {
                tail_ = nullptr; // drained
            }
            out->next_ = nullptr;
        }
        return out;
    }
    [[nodiscard]] bool empty() const { return stuff == nullptr; }

  private:
    Stuff* stuff{nullptr};
    Stuff* tail_{nullptr}; // last node — O(1) push_back, no more walking
    // if we dont set anything, place it null
    // so a self hold list is same as Stuff*
    [[no_unique_address]] Less less_{};
};

namespace zeros_impl {
struct Probe : SelfNode<Probe> {};
} // namespace zeros_impl

static_assert(SelfLinked<zeros_impl::Probe>);
static_assert(std::is_trivially_destructible_v<SelfList<zeros_impl::Probe>>);
static_assert(sizeof(SelfList<zeros_impl::Probe>) == 2 * sizeof(zeros_impl::Probe*),
              "SelfList = head + tail (tail buys O(1) push_back)");
} // namespace ZerOS::base
