/*
 * Implementation of the Global Interpreter Lock (GIL).
 */
/*全局解释器锁（Global Interpreter Lock，GIL）的接口*/

#include <stdlib.h>
#include <errno.h>

#include "pycore_atomic.h"


/*
   Notes about the implementation:

   - The GIL is just a boolean variable (locked) whose access is protected
     by a mutex (gil_mutex), and whose changes are signalled by a condition
     variable (gil_cond). gil_mutex is taken for short periods of time,
     and therefore mostly uncontended.
    GIL只是一个布尔变量（locked），其访问受mutex（gil_mutex）保护，其更改由条件变量（gil_cond）发出信号。
    gilèu mutex的使用时间很短，因此大多数情况下是无竞争的

   - In the GIL-holding thread, the main loop (PyEval_EvalFrameEx) must be
     able to release the GIL on demand by another thread. A volatile boolean
     variable (gil_drop_request) is used for that purpose, which is checked
     at every turn of the eval loop. That variable is set after a wait of
     `interval` microseconds on `gil_cond` has timed out.
     在GIL holding线程中，主循环（PyEval_EvalFrameEx）必须能够根据另一个线程的请求释放GIL。
     一个可变的布尔变量（gil_drop_request）用于此目的，它在eval循环的每一个回合都被检查。该变量
     是在“gil_cond”上等待“interval”微秒超时后设置的。

      [Actually, another volatile boolean variable (eval_breaker) is used
       which ORs several conditions into one. Volatile booleans are
       sufficient as inter-thread signalling means since Python is run
       on cache-coherent architectures only.]
    实际上，使用了另一个可变布尔变量（eval_breaker），它将多个条件合并为一个条件。
    由于Python只在缓存相关的体系结构上运行，所以Volatile booleans就足以作为线程间信号传递的手段

   - A thread wanting to take the GIL will first let pass a given amount of
     time (`interval` microseconds) before setting gil_drop_request. This
     encourages a defined switching period, but doesn't enforce it since
     opcodes can take an arbitrary time to execute.
     想要获取GIL的线程在设置gil_drop_request请求之前，将首先让给定的时间量（`interval`微秒）通过。
     这会鼓励定义一个切换周期，但不会强制执行，因为操作码可能需要任意时间来执行


     The `interval` value is available for the user to read and modify
     using the Python API `sys.{get,set}switchinterval()`.
    “interval”值可供用户使用, 通过python api`sys.{get，set}switchinterval（）可以读取和修改

   - When a thread releases the GIL and gil_drop_request is set, that thread
     ensures that another GIL-awaiting thread gets scheduled.
     It does so by waiting on a condition variable (switch_cond) until
     the value of last_holder is changed to something else than its
     own thread state pointer, indicating that another thread was able to
     take the GIL.
     当一个线程释放GIL并设置gil_drop_request时，该线程确保另一个等待GIL的线程被调度。
     它通过等待条件变量（switch_cond）直到last_holder的值被更改为自己线程状态指针以外的值，
     一旦last_holder被更改为其他值，表示另一个线程已经获得了GIL.

     This is meant to prohibit the latency-adverse behaviour on multi-core
     machines where one thread would speculatively release the GIL, but still
     run and end up being the first to re-acquire it, making the "timeslices"
     much longer than expected.
     这是为了禁止多核处理器上的延迟不良行为，一个线程可以推测地释放GIL，但仍然运行并最终成为
     第一个重新获得它，使“时间片”比预期的要长得多。
     (Note: this mechanism is enabled with FORCE_SWITCHING above)
*/

#include "condvar.h"

#define MUTEX_INIT(mut) \
    if (PyMUTEX_INIT(&(mut))) { \
        Py_FatalError("PyMUTEX_INIT(" #mut ") failed"); };
#define MUTEX_FINI(mut) \
    if (PyMUTEX_FINI(&(mut))) { \
        Py_FatalError("PyMUTEX_FINI(" #mut ") failed"); };
#define MUTEX_LOCK(mut) \
    if (PyMUTEX_LOCK(&(mut))) { \
        Py_FatalError("PyMUTEX_LOCK(" #mut ") failed"); };
#define MUTEX_UNLOCK(mut) \
    if (PyMUTEX_UNLOCK(&(mut))) { \
        Py_FatalError("PyMUTEX_UNLOCK(" #mut ") failed"); };

#define COND_INIT(cond) \
    if (PyCOND_INIT(&(cond))) { \
        Py_FatalError("PyCOND_INIT(" #cond ") failed"); };
#define COND_FINI(cond) \
    if (PyCOND_FINI(&(cond))) { \
        Py_FatalError("PyCOND_FINI(" #cond ") failed"); };
#define COND_SIGNAL(cond) \
    if (PyCOND_SIGNAL(&(cond))) { \
        Py_FatalError("PyCOND_SIGNAL(" #cond ") failed"); };
#define COND_WAIT(cond, mut) \
    if (PyCOND_WAIT(&(cond), &(mut))) { \
        Py_FatalError("PyCOND_WAIT(" #cond ") failed"); };
#define COND_TIMED_WAIT(cond, mut, microseconds, timeout_result) \
    { \
        int r = PyCOND_TIMEDWAIT(&(cond), &(mut), (microseconds)); \
        if (r < 0) \
            Py_FatalError("PyCOND_WAIT(" #cond ") failed"); \
        if (r) /* 1 == timeout, 2 == impl. can't say, so assume timeout */ \
            timeout_result = 1; \
        else \
            timeout_result = 0; \
    } \


#define DEFAULT_INTERVAL 5000

static void _gil_initialize(struct _gil_runtime_state *gil)
{
    /*
    GIL 初始化，状态未-1，表示未创建状态
    */
    _Py_atomic_int uninitialized = {-1};
    gil->locked = uninitialized;
    gil->interval = DEFAULT_INTERVAL;
}

static int gil_created(struct _gil_runtime_state *gil)
{
    /*
    判断GIL是否已创建，原子读操作
    */
    return (_Py_atomic_load_explicit(&gil->locked, _Py_memory_order_acquire) >= 0);
}

static void create_gil(struct _gil_runtime_state *gil)
{
    /*
    创建GIL，原子写操作
    */
    MUTEX_INIT(gil->mutex);
#ifdef FORCE_SWITCHING
    MUTEX_INIT(gil->switch_mutex);
#endif
    COND_INIT(gil->cond);
#ifdef FORCE_SWITCHING
    COND_INIT(gil->switch_cond);
#endif
    _Py_atomic_store_relaxed(&gil->last_holder, 0);
    _Py_ANNOTATE_RWLOCK_CREATE(&gil->locked);
    _Py_atomic_store_explicit(&gil->locked, 0, _Py_memory_order_release);
}

static void destroy_gil(struct _gil_runtime_state *gil)
{
    /*
    销毁GIL，原子写操作
    */
    /* some pthread-like implementations tie the mutex to the cond
     * and must have the cond destroyed first.
     */
    COND_FINI(gil->cond);
    MUTEX_FINI(gil->mutex);
#ifdef FORCE_SWITCHING
    COND_FINI(gil->switch_cond);
    MUTEX_FINI(gil->switch_mutex);
#endif
    _Py_atomic_store_explicit(&gil->locked, -1,
                              _Py_memory_order_release);
    _Py_ANNOTATE_RWLOCK_DESTROY(&gil->locked);
}

static void recreate_gil(struct _gil_runtime_state *gil)
{   /*
    销毁-->重新创建
    */
    _Py_ANNOTATE_RWLOCK_DESTROY(&gil->locked);
    /* XXX should we destroy the old OS resources here? */
    create_gil(gil);
}

static void
drop_gil(struct _ceval_runtime_state *ceval, struct _ceval_state *ceval2,
         PyThreadState *tstate)
{
#ifdef EXPERIMENTAL_ISOLATED_SUBINTERPRETERS
    struct _gil_runtime_state *gil = &ceval2->gil;
#else
    struct _gil_runtime_state *gil = &ceval->gil;
#endif
    if (!_Py_atomic_load_relaxed(&gil->locked)) {
        Py_FatalError("drop_gil: GIL is not locked");
    }

    /* tstate is allowed to be NULL (early interpreter init) */
    if (tstate != NULL) {
        /* Sub-interpreter support: threads might have been switched
           under our feet using PyThreadState_Swap(). Fix the GIL last
           holder variable so that our heuristics work. */
        _Py_atomic_store_relaxed(&gil->last_holder, (uintptr_t)tstate);
    }

    MUTEX_LOCK(gil->mutex);
    _Py_ANNOTATE_RWLOCK_RELEASED(&gil->locked, /*is_write=*/1);
    _Py_atomic_store_relaxed(&gil->locked, 0);
    COND_SIGNAL(gil->cond);
    MUTEX_UNLOCK(gil->mutex);

#ifdef FORCE_SWITCHING
    if (_Py_atomic_load_relaxed(&ceval2->gil_drop_request) && tstate != NULL) {
        MUTEX_LOCK(gil->switch_mutex);
        /* Not switched yet => wait */
        if (((PyThreadState*)_Py_atomic_load_relaxed(&gil->last_holder)) == tstate)
        {
            assert(is_tstate_valid(tstate));
            RESET_GIL_DROP_REQUEST(tstate->interp);
            /* NOTE: if COND_WAIT does not atomically start waiting when
               releasing the mutex, another thread can run through, take
               the GIL and drop it again, and reset the condition
               before we even had a chance to wait for it. */
            COND_WAIT(gil->switch_cond, gil->switch_mutex);
        }
        MUTEX_UNLOCK(gil->switch_mutex);
    }
#endif
}


/* Check if a Python thread must exit immediately, rather than taking the GIL
   if Py_Finalize() has been called.

   When this function is called by a daemon thread after Py_Finalize() has been
   called, the GIL does no longer exist.

   tstate must be non-NULL. */
static inline int
tstate_must_exit(PyThreadState *tstate)
{
    /* bpo-39877: Access _PyRuntime directly rather than using
       tstate->interp->runtime to support calls from Python daemon threads.
       After Py_Finalize() has been called, tstate can be a dangling pointer:
       point to PyThreadState freed memory. */
    PyThreadState *finalizing = _PyRuntimeState_GetFinalizing(&_PyRuntime);
    return (finalizing != NULL && finalizing != tstate);
}


/* Take the GIL.

   The function saves errno at entry and restores its value at exit.

   tstate must be non-NULL. */
static void
take_gil(PyThreadState *tstate)
{
    /*
    获取GIL
    1. 获取mutex互斥锁
    2. 检查是否有线程持有GIL（locked为1）
    3. 如果GIL被占有的话，设置cond信号量等待。
    4. 等待的时候线程就会挂起，释放mutex互斥锁。
    5. cond信号被唤醒时，mutex互斥锁会自动加锁。
    while语句避免了意外唤醒时条件不满足，大多数情况下都是条件满足被唤醒。
    */
    int err = errno;

    assert(tstate != NULL);

    if (tstate_must_exit(tstate)) {
        /* bpo-39877: If Py_Finalize() has been called and tstate is not the
           thread which called Py_Finalize(), exit immediately the thread.

           This code path can be reached by a daemon thread after Py_Finalize()
           completes. In this case, tstate is a dangling pointer: points to
           PyThreadState freed memory. */
        PyThread_exit_thread();
    }

    assert(is_tstate_valid(tstate));
    PyInterpreterState *interp = tstate->interp;
    struct _ceval_runtime_state *ceval = &interp->runtime->ceval;
    struct _ceval_state *ceval2 = &interp->ceval;
#ifdef EXPERIMENTAL_ISOLATED_SUBINTERPRETERS
    struct _gil_runtime_state *gil = &ceval2->gil;
#else
    struct _gil_runtime_state *gil = &ceval->gil;
#endif

    /* Check that _PyEval_InitThreads() was called to create the lock */
    assert(gil_created(gil));

    MUTEX_LOCK(gil->mutex);

    if (!_Py_atomic_load_relaxed(&gil->locked)) {
        goto _ready;
    }

    while (_Py_atomic_load_relaxed(&gil->locked)) {
        unsigned long saved_switchnum = gil->switch_number;

        unsigned long interval = (gil->interval >= 1 ? gil->interval : 1);
        int timed_out = 0;
        COND_TIMED_WAIT(gil->cond, gil->mutex, interval, timed_out);

        /*
        如果等待超时并且这期间没有发生线程切换，就通过SET_GIL_DROP_REQUEST请求持有GIL的线程释放GIL。
        反之获得GIL就将locked置为1，并将当前线程状态PyThreadState对象保存到last_holder，switch_number切换次数加1。
        */
        /* If we timed out and no switch occurred in the meantime, it is time
           to ask the GIL-holding thread to drop it. */
        if (timed_out &&
            _Py_atomic_load_relaxed(&gil->locked) &&
            gil->switch_number == saved_switchnum)
        {
            if (tstate_must_exit(tstate)) {
                MUTEX_UNLOCK(gil->mutex);
                PyThread_exit_thread();
            }
            assert(is_tstate_valid(tstate));

            SET_GIL_DROP_REQUEST(interp);
        }
    }

_ready:
#ifdef FORCE_SWITCHING
    /* This mutex must be taken before modifying gil->last_holder:
       see drop_gil(). */
    MUTEX_LOCK(gil->switch_mutex);
#endif
    /* We now hold the GIL */
    _Py_atomic_store_relaxed(&gil->locked, 1);
    _Py_ANNOTATE_RWLOCK_ACQUIRED(&gil->locked, /*is_write=*/1);

    if (tstate != (PyThreadState*)_Py_atomic_load_relaxed(&gil->last_holder)) {
        _Py_atomic_store_relaxed(&gil->last_holder, (uintptr_t)tstate);
        ++gil->switch_number;
    }

#ifdef FORCE_SWITCHING
    COND_SIGNAL(gil->switch_cond);
    MUTEX_UNLOCK(gil->switch_mutex);
#endif

    if (tstate_must_exit(tstate)) {
        /* bpo-36475: If Py_Finalize() has been called and tstate is not
           the thread which called Py_Finalize(), exit immediately the
           thread.

           This code path can be reached by a daemon thread which was waiting
           in take_gil() while the main thread called
           wait_for_thread_shutdown() from Py_Finalize(). */
        MUTEX_UNLOCK(gil->mutex);
        drop_gil(ceval, ceval2, tstate);
        PyThread_exit_thread();
    }
    assert(is_tstate_valid(tstate));

    if (_Py_atomic_load_relaxed(&ceval2->gil_drop_request)) {
        RESET_GIL_DROP_REQUEST(interp);
    }
    else {
        /* bpo-40010: eval_breaker should be recomputed to be set to 1 if there
           is a pending signal: signal received by another thread which cannot
           handle signals.

           Note: RESET_GIL_DROP_REQUEST() calls COMPUTE_EVAL_BREAKER(). */
        COMPUTE_EVAL_BREAKER(interp, ceval, ceval2);
    }

    /* Don't access tstate if the thread must exit */
    if (tstate->async_exc != NULL) {
        _PyEval_SignalAsyncExc(tstate->interp);
    }

    MUTEX_UNLOCK(gil->mutex);

    errno = err;
}

{
#ifdef EXPERIMENTAL_ISOLATED_SUBINTERPRETERS
    PyInterpreterState *interp = PyInterpreterState_Get();
    struct _gil_runtime_state *gil = &interp->ceval.gil;
#else
    struct _gil_runtime_state *gil = &_PyRuntime.ceval.gil;
#endif
    gil->interval = microseconds;
}

unsigned long _PyEval_GetSwitchInterval()
{
#ifdef EXPERIMENTAL_ISOLATED_SUBINTERPRETERS
    PyInterpreterState *interp = PyInterpreterState_Get();
    struct _gil_runtime_state *gil = &interp->ceval.gil;
#else
    struct _gil_runtime_state *gil = &_PyRuntime.ceval.gil;
#endif
    return gil->interval;
}
