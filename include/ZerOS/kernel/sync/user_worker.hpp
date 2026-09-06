/**
 * @file user_worker.hpp
 * @author CharlieChen114514
 * @brief   What is A worker, tasks forces us to
 *          write something like for(;;) to prevent us not leek
 *          out to the outware, Works dont
 *
 * @version 0.1
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once
#include "ZerOS/base/check.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"
#include "ZerOS/kernel/sync/msg_queue.hpp"
#include "ZerOS/kernel/system_concept.hpp"

namespace ZerOS::sync {
/**
 * @brief Just A work, which execute as a plain functions
 *
 */
struct Work {
    using WorkFunc = void (*)(void*);
    using WorkFuncArgs = void*;
    WorkFunc work_func;
    WorkFuncArgs args;

    static void InvokeSelf(Work& w) { w.work_func(w.args); }
};

template <ZerOS::system::SystemContext System, std::size_t Depth = 8, std::size_t StackWords = 64>
struct WorkBase {
    // defer
    bool defer(Work::WorkFunc func, Work::WorkFuncArgs arg) noexcept {
        ZerOS::debug::Check(task_, "defer before setup");
        return queue_.post(Work{func, arg}) == QueueError::Ok;
    }

    sched::TCB& setup(sched::TaskPriority_t prio, const char* name) noexcept {
        task_ = &task::named(name).prio(prio).stack(stack_).entry(&Loop, this).spawn_into(box_);
        return *task_; // the pen has spoken: from here on, defer may feed it
    }

  private:
    static void Loop(void* self) {
        auto& work_base_itself = *static_cast<WorkBase*>(self);
        for (;;) // Why different? we write the loop here
        {
            auto user_work = work_base_itself.queue_.receive_one();
            if (user_work) {
                Work::InvokeSelf(*user_work);
            }
        }
    };

    sched::TCB* task_{};
    QueueBase<System, Work, Depth> queue_{};
    sched::TCBStorage box_{};
    // Local stack storage
    alignas(8) std::uint32_t stack_[StackWords]{};
};

} // namespace ZerOS::sync