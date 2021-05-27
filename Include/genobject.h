
/* Generator object interface */

#ifndef Py_LIMITED_API
#ifndef Py_GENOBJECT_H
#define Py_GENOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#include "pystate.h"   /* _PyErr_StackItem */
#include "abstract.h" /* PySendResult */

/* _PyGenObject_HEAD defines the initial segment of generator
   and coroutine objects. */
/*
生成器,协程的基础。生成器的语法规则比较特殊，长得像个函数，但是调用之后却返回一个生成器对象。所以他的结构体定义也比较特殊，是一个宏。
其中最重要的 是 prefix_frame, ## 是连接符。它指向了一个 PyFrameObject 对象，就是该生成器的上下文。

    _frame: 指向生成器上下文的指针
    _code: 生成器的字节码
*/
#define _PyGenObject_HEAD(prefix)                                           \
    PyObject_HEAD                                                           \
    /* Note: gi_frame can be NULL if the generator is "finished" */         \
    PyFrameObject *prefix##_frame;                                          \
    /* The code object backing the generator */                             \
    PyObject *prefix##_code;                                                \
    /* List of weak reference. */                                           \
    PyObject *prefix##_weakreflist;                                         \
    /* Name of the generator. */                                            \
    PyObject *prefix##_name;                                                \
    /* Qualified name of the generator. */                                  \
    PyObject *prefix##_qualname;                                            \
    _PyErr_StackItem prefix##_exc_state;

typedef struct {
    /* The gi_ prefix is intended to remind of generator-iterator. */
    _PyGenObject_HEAD(gi)
} PyGenObject;

PyAPI_DATA(PyTypeObject) PyGen_Type;

#define PyGen_Check(op) PyObject_TypeCheck(op, &PyGen_Type)
#define PyGen_CheckExact(op) Py_IS_TYPE(op, &PyGen_Type)

PyAPI_FUNC(PyObject *) PyGen_New(PyFrameObject *);
PyAPI_FUNC(PyObject *) PyGen_NewWithQualName(PyFrameObject *,
    PyObject *name, PyObject *qualname);
PyAPI_FUNC(int) _PyGen_SetStopIterationValue(PyObject *);
PyAPI_FUNC(int) _PyGen_FetchStopIterationValue(PyObject **);
PyObject *_PyGen_yf(PyGenObject *);
PyAPI_FUNC(void) _PyGen_Finalize(PyObject *self);

#ifndef Py_LIMITED_API
typedef struct {
    _PyGenObject_HEAD(cr)
    PyObject *cr_origin;
} PyCoroObject;

PyAPI_DATA(PyTypeObject) PyCoro_Type;
PyAPI_DATA(PyTypeObject) _PyCoroWrapper_Type;

#define PyCoro_CheckExact(op) Py_IS_TYPE(op, &PyCoro_Type)
PyObject *_PyCoro_GetAwaitableIter(PyObject *o);
PyAPI_FUNC(PyObject *) PyCoro_New(PyFrameObject *,
    PyObject *name, PyObject *qualname);

/* Asynchronous Generators */

typedef struct {
    _PyGenObject_HEAD(ag)
    PyObject *ag_finalizer;

    /* Flag is set to 1 when hooks set up by sys.set_asyncgen_hooks
       were called on the generator, to avoid calling them more
       than once. */
    int ag_hooks_inited;

    /* Flag is set to 1 when aclose() is called for the first time, or
       when a StopAsyncIteration exception is raised. */
    int ag_closed;

    int ag_running_async;
} PyAsyncGenObject;

PyAPI_DATA(PyTypeObject) PyAsyncGen_Type;
PyAPI_DATA(PyTypeObject) _PyAsyncGenASend_Type;
PyAPI_DATA(PyTypeObject) _PyAsyncGenWrappedValue_Type;
PyAPI_DATA(PyTypeObject) _PyAsyncGenAThrow_Type;

PyAPI_FUNC(PyObject *) PyAsyncGen_New(PyFrameObject *,
    PyObject *name, PyObject *qualname);

#define PyAsyncGen_CheckExact(op) Py_IS_TYPE(op, &PyAsyncGen_Type)

PyObject *_PyAsyncGenValueWrapperNew(PyObject *);

#endif

#undef _PyGenObject_HEAD

#ifdef __cplusplus
}
#endif
#endif /* !Py_GENOBJECT_H */
#endif /* Py_LIMITED_API */
