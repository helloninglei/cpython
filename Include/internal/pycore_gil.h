#ifndef Py_INTERNAL_GIL_H
#define Py_INTERNAL_GIL_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_atomic.h"    /* _Py_atomic_address */
#include "pycore_condvar.h"   /* PyCOND_T */

#ifndef Py_HAVE_CONDVAR
#  error You need either a POSIX-compatible or a Windows system!
#endif

/* Enable if you want to force the switching of threads at least
   every `interval`. */
#undef FORCE_SWITCHING
#define FORCE_SWITCHING

struct _gil_runtime_state {
    /*
    GIL 运行时状态
        -1: 未初始化
         1: 已被线程获取
         0: 未被线程获取

    cond和mutex用来保护locked:
        cond:条件机制
        mutex:互斥锁

    _Py_atomic_address:
        atomic_uintptr_t是一个原子类型指针，当一个线程获得GIL之后，_value将指向线程状态PyThreadState对象。
    */
    /* microseconds (the Python API uses seconds, though) */
    unsigned long interval;
    /* Last PyThreadState holding / having held the GIL. This helps us
       know whether anyone else was scheduled after we dropped the GIL. */
    _Py_atomic_address last_holder;
    /* Whether the GIL is already taken (-1 if uninitialized). This is
       atomic because it can be read without any lock taken in ceval.c. */
    _Py_atomic_int locked;
    /* Number of GIL switches since the beginning. */
    unsigned long switch_number;
    /* This condition variable allows one or several threads to wait
       until the GIL is released. In addition, the mutex also protects
       the above variables. */
    PyCOND_T cond;
    PyMUTEX_T mutex;
#ifdef FORCE_SWITCHING
    /* This condition variable helps the GIL-releasing thread wait for
       a GIL-awaiting thread to be scheduled and take the GIL. */
    PyCOND_T switch_cond;
    PyMUTEX_T switch_mutex;
#endif
};

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_GIL_H */
