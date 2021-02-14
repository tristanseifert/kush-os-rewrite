#ifndef KERN_SCHED_THREAD_H
#define KERN_SCHED_THREAD_H

#include <stddef.h>
#include <stdint.h>

#include <runtime/Queue.h>
#include <runtime/Vector.h>
#include <handle/Manager.h>

#include <arch/ThreadState.h>
#include <arch/rwlock.h>

namespace sched {
class Blockable;
struct Task;

/**
 * Threads are the smallest units of execution in the kernel. They are the unit of work that the
 * scheduler concerns itself with.
 *
 * Each thread can either be ready to run, blocked, or paused. When the scheduler decides to run
 * the thread, its saved CPU state is loaded and the task executed. When the task returns to the
 * kernel (either through a syscall that blocks/context switches, or via its time quantum expiring)
 * its state is saved again.
 *
 * Depending on the nature of the thread's return to the kernel, it will be added back to the run
 * queue if it's ready to run again and not blocked. (This implies threads cannot change from
 * runnable to blocked if they're not currently executing.)
 */
struct Thread {
    friend class Scheduler;
    friend class Blockable;

    /// Length of thread names
    constexpr static const size_t kNameLength = 32;

    private:
        /// Info on a DPC to execute
        struct DpcInfo {
            void (*handler)(Thread *, void *);
            void *context = nullptr;
        };

    public:
        enum class State {
            /// Thread can become runnable at any time, but only via an explicit API call
            Paused                      = 0,
            /// Thread requests to be scheduled as soon as possible
            Runnable                    = 1,
            /// Thread is waiting on some event to occur
            Blocked                     = 2,
            /// About to be destroyed; do not schedule or access.
            Zombie                      = 3,
        };

    public:
        /// Global thread id
        uint32_t tid = 0;
        /// task that owns us
        Task *task = nullptr;

        /// handle to the thread
        Handle handle;

        /// descriptive thread name, if desired
        char name[kNameLength] = {0};

        /// current thread state
        State state = State::Paused;
        /**
         * Determines whether the thread is run in the context of the kernel, or as an user-mode
         * thread. Platforms may treat userspace and kernel threads differently (from the
         * perspective of stacks, for example) but are not required to do so.
         *
         * The scheduler itself makes no core differentiation between user and kernel mode threads;
         * the only difference is that kernel threads may not make system calls.
         */
        bool kernelMode = false;

        /**
         * Flag set when the scheduler has assigned the thread to a processor and it is executing;
         * it will be cleared immediately after the thread is switched out.
         *
         * This flag is the responsibility of the arch context switching code.
         */
        bool isActive = false;
        /// when set, this thread should kill itself when switched out
        bool needsToDie = false;
        /// Timestamp at which the thread was switched to
        uint64_t lastSwitchedTo = 0;

        /**
         * Priority of the thread; this should be a value between -100 and 100, with negative
         * values having the lowest priority. The scheduler internally converts this to whatever
         * priority system it uses.
         */
        int16_t priority = 0;
        /// Priority boost for this thread; incremented any time it's not scheduled
        int16_t priorityBoost = 0;

        // Number of ticks that the thread's quantum is in length. Ticks are usually 1ms.
        uint16_t quantumTicks = 10;
        // Ticks left in the thread's current quantum
        uint16_t quantum = 0;

        /**
         * Number of nanoseconds this thread has been executing on the CPU.
         */
        uint64_t cpuTime __attribute__((aligned(8))) = 0;

        /**
         * Notification value and mask for the thread.
         *
         * Notifications are an asynchronous signalling mechanism that can be used to signal a
         * thread that a paritcular event occurred, without any additional auxiliary information;
         * this makes it ideal for things like interrupt handlers.
         *
         * Each thread defines a notification mask, which indicates on which bits (set) of the
         * notification set the thread is interested in; when the notification mask is updated, it
         * is compared against the mask, and if any bits are set, the thread can be unblocked.
         */
        uintptr_t notifications = 0;
        uintptr_t notificationMask = 0;

        /**
         * Objects this thread is currently blocking on
         */
        Blockable *blockingOn = nullptr;

        /**
         * The thread can be accessed read-only by multiple processes, but the scheduler will
         * always require write access, in order to be able to save processor state later.
         */
        DECLARE_RWLOCK(lock);

        /// DPCs queued for the thread
        rt::Queue<DpcInfo> dpcs;
        /// When set, there are DPCs pending.
        bool dpcsPending = false;

        /// bottom of the kernel stack of this thread
        void *stack = nullptr;

        /// architecture-specific thread state
        arch::ThreadState regs __attribute__((aligned(16)));

    public:
        /// Allocates a new kernel space thread
        static Thread *kernelThread(Task *task, void (*entry)(uintptr_t), const uintptr_t param = 0);
        /// Releases a previously allocated thread struct
        static void free(Thread *);

        Thread(Task *task, const uintptr_t pc, const uintptr_t param,
                const bool kernelThread = true);
        ~Thread();

        /// Context switches to this thread.
        void switchTo();
        /// Returns to user mode, with the specified program counter and stack.
        void returnToUser(const uintptr_t pc, const uintptr_t stack, const uintptr_t arg = 0) __attribute__((noreturn));

        /// Sets the thread's name.
        void setName(const char *name, const size_t length = 0);
        /// Sets the thread's state.
        void setState(State newState) {
            if(newState == State::Runnable) {
                REQUIRE(!this->blockingOn, "cannot be runnable while blocking");
            }

            __atomic_store(&this->state, &newState, __ATOMIC_RELEASE);
        }

        /// Blocks the thread on the given object.
        int blockOn(Blockable *b);

        /// Adds a DPC to the thread's queue.
        int addDpc(void (*handler)(Thread *, void *), void *context = nullptr);
        /// Drains the DPC queue.
        void runDpcs();

        /// Terminates this thread.
        void terminate();

        /// Returns a handle to the currently executing thread.
        static Thread *current();
        /// Blocks the current thread for the given number of nanoseconds.
        static void sleep(const uint64_t nanos);
        /// Give up the rest of this thread's CPU time
        static void yield();
        /// Terminates the calling thread
        static void die() __attribute__((noreturn));

    private:
        /// next thread id
        static uint32_t nextTid;

    private:
        static void initAllocator();

    private:
        void unblock(Blockable *b);
        /// Called when this thread is switching out
        void switchFrom();

        /// Invoked by the scheduler when a thread that is blocked yields
        void prepareBlocks();

        /// Called on context switch out to complete termination.
        void deferredTerminate();
};
}

#endif
