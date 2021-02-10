#include "Thread.h"
#include "Blockable.h"
#include "Scheduler.h"
#include "Task.h"
#include "IdleWorker.h"

#include "TimerBlocker.h"

#include "mem/StackPool.h"
#include "mem/SlabAllocator.h"

#include <arch/critical.h>

#include <platform.h>
#include <string.h>

using namespace sched;

static char gAllocBuf[sizeof(mem::SlabAllocator<Thread>)] __attribute__((aligned(64)));
static mem::SlabAllocator<Thread> *gThreadAllocator = nullptr;

/// Thread id for the next thread
uint32_t Thread::nextTid = 1;

/**
 * Initializes the task struct allocator.
 */
void Thread::initAllocator() {
    gThreadAllocator = reinterpret_cast<mem::SlabAllocator<Thread> *>(&gAllocBuf);
    new(gThreadAllocator) mem::SlabAllocator<Thread>();
}

/**
 * Allocates a new thread.
 */
Thread *Thread::kernelThread(Task *parent, void (*entry)(uintptr_t), const uintptr_t param) {
    if(!gThreadAllocator) initAllocator();
    auto thread = gThreadAllocator->alloc(parent, (uintptr_t) entry, param, true);

    return thread;
}

/**
 * Frees a previously allocated thread.
 *
 * @note The thread MUST NOT be scheduled or running.
 */
void Thread::free(Thread *ptr) {
    gThreadAllocator->free(ptr);
}



/**
 * Allocates a new thread.
 */
Thread::Thread(Task *_parent, const uintptr_t pc, const uintptr_t param, const bool _kernel) : 
    task(_parent), kernelMode(_kernel) {
    this->tid = nextTid++;

    // get a kernel stack
    this->stack = mem::StackPool::get();
    REQUIRE(this->stack, "failed to get stack for thread %p", this);

    // then initialize thread state
    arch::InitThreadState(this, pc, param);

    // and add to task
    if(_parent) {
        _parent->addThread(this);
    }
}

/**
 * Destroys all resources associated with this thread.
 */
Thread::~Thread() {
    // release kernel stack
    mem::StackPool::release(this->stack);
}

/**
 * Returns the currently executing thread.
 */
Thread *Thread::current() {
    return Scheduler::get()->runningThread();
}

/**
 * Updates internal tracking structures when the thread is context switched out.
 */
void Thread::switchFrom() {
    if(this->lastSwitchedTo) {
        const auto ran = platform_timer_now() - this->lastSwitchedTo;
        __atomic_fetch_add(&this->cpuTime, ran, __ATOMIC_RELEASE);
    }
}

/**
 * Performs a context switch to this thread.
 *
 * If we're currently executing on a thread, its state is saved, and the function will return when
 * that thread is switched back in. Otherwise, the function never returns.
 */
void Thread::switchTo() {
    auto current = Scheduler::get()->runningThread();

    if(current) {
        current->switchFrom();
    }

    this->lastSwitchedTo = platform_timer_now();

    //log("switching to %s (from %s)", this->name, current ? (current->name) : "<null>");
    Scheduler::get()->setRunningThread(this);
    arch::RestoreThreadState(current, this);
}

/**
 * Call into architecture code to return to user mode.
 */
void Thread::returnToUser(const uintptr_t pc, const uintptr_t stack) {
    arch::ReturnToUser(pc, stack);
}


/**
 * Copies the given name string to the thread's name field.
 */
void Thread::setName(const char *newName) {
    RW_LOCK_WRITE_GUARD(this->lock);
    strncpy(this->name, newName, kNameLength);
}

/**
 * Call into the scheduler to yield the rest of this thread's CPU time. We'll get put back at the
 * end of the runnable queue.
 */
void Thread::yield() {
    Scheduler::get()->yield();
}

/**
 * Terminates the calling thread.
 *
 * The thread will be marked as a zombie (so it won't be scheduled anymore) and submitted to the
 * scheduler to actually be deallocated at a later time.
 */
void Thread::terminate() {
    auto thread = current();
    REQUIRE(thread, "cannot terminate null thread!");

    thread->setState(State::Zombie);

    Scheduler::get()->idle->queueDestroyThread(thread);
    Scheduler::get()->switchToRunnable();

    // we should not get here
    panic("failed to terminmate thread");
}



/**
 * Sleeps the calling thread for the given number of nanoseconds.
 *
 * @note The actual sleep time may be less or more than what is provided; it's merely taken as a
 * "best effort" hint to the actual sleep time.
 */
void Thread::sleep(const uint64_t nanos) {
    auto thread = current();
    int err;

    // create the timer blockable
    TimerBlocker block(nanos);

    // wait on it
    err = thread->blockOn(&block);

    if(err) {
        log("sleep failed: %d state %d", err, (int) thread->state);
    }
}

/**
 * Blocks the thread on the given object.
 *
 * @return 0 if the block completed, or an error code if the block was interrupted (because the
 * thread was woken for another reason, for example.)
 *
 * Note that we raise the IRQL, but do not ever lower it; this is because when we get switched back
 * in, the IRQL will be lowered again to the Passive level, i.e. what most threads that are running
 * will be using.
 */
int Thread::blockOn(Blockable *b) {
    REQUIRE(b, "invalid blockable %p", b);
    REQUIRE_IRQL_LEQ(platform::Irql::Scheduler);

    // raise IRQL to scheduler level (to prevent being preempted)
    platform_raise_irql(platform::Irql::Scheduler);

    // update thread state
    RW_LOCK_WRITE(&this->lock);

    REQUIRE(this->state == State::Runnable, "Cannot %s thread %p with state: %d (blockable %p)",
            "block", this, (int) this->state, b);
    REQUIRE(!this->blockingOn, "cannot block thread %p (object %p) while already blocking (%p)",
            this, b, this->blockingOn);

    this->blockingOn = b;
    this->setState(State::Blocked);

    RW_UNLOCK_WRITE(&this->lock);

    // yield the rest of the CPU time
    Scheduler::get()->yield();

    // get state from the blockable
    bool signaled = b->isSignalled();
    b->reset();

    // return whether the thread woke correctly or nah
    return (signaled ? 0 : -1);
}

/**
 * Prepares any objects that we're blocking on.
 */
void Thread::prepareBlocks() {
    REQUIRE(this->blockingOn, "no blocking objects");
    this->blockingOn->willBlockOn(this);
}

/**
 * Unblocks the thread.
 */
void Thread::unblock(Blockable *b) {
    REQUIRE_IRQL_LEQ(platform::Irql::Scheduler);
    REQUIRE(this->state == State::Blocked, "Cannot %s thread %p with state: %d (blockable %p)",
            "unblock", this, (int) this->state, b);

    DECLARE_CRITICAL();
    CRITICAL_ENTER();

    // clear the blockable list
    RW_LOCK_WRITE(&this->lock);
    REQUIRE(b == this->blockingOn, "thread not blocking on %p! (is %p)", b, this->blockingOn);

    // finish setting state
    this->blockingOn->didUnblock();
    this->blockingOn = nullptr;

    this->setState(State::Runnable);

    RW_UNLOCK_WRITE(&this->lock);
    CRITICAL_EXIT();

    // queue in scheduler
    Scheduler::get()->markThreadAsRunnable(this, true);
}
