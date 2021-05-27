/* Frame object interface */

#ifndef Py_CPYTHON_FRAMEOBJECT_H
#  error "this header file must not be included directly"
#endif

/* These values are chosen so that the inline functions below all
 * compare f_state to zero.
 */
enum _framestate {
    FRAME_CREATED = -2,
    FRAME_SUSPENDED = -1,
    FRAME_EXECUTING = 0,
    FRAME_RETURNED = 1,
    FRAME_UNWINDING = 2,
    FRAME_RAISED = 3,
    FRAME_CLEARED = 4
};

typedef signed char PyFrameState;

struct _frame {
    /*
    栈帧对象frame底层实现 源码分析(https://blog.csdn.net/qq_31720329/article/details/86757892)
    f_back: 上一个栈帧
    f_code: 代码段对象
    f_builtins: 内建变量表(字典对象)
    f_globals: 全局变量表(字典对象)
    f_locals: 局部变量表
    f_valuestack: 栈底位置
    f_gen: 专为生成器设计的指针
    f_lasti: 运行的上一个字节码位置
    f_lineno: 运行字节码对应的python源代码的行数
    */
    PyObject_VAR_HEAD
    struct _frame *f_back;      /* previous frame, or NULL */
    PyCodeObject *f_code;       /* code segment */
    PyObject *f_builtins;       /* builtin symbol table (PyDictObject) */
    PyObject *f_globals;        /* global symbol table (PyDictObject) */
    PyObject *f_locals;         /* local symbol table (any mapping) */
    PyObject **f_valuestack;    /* points after the last local */
    PyObject *f_trace;          /* Trace function */
    /* Borrowed reference to a generator, or NULL */
    PyObject *f_gen;
    int f_stackdepth;           /* Depth of value stack */
    int f_lasti;                /* Last instruction if called */
    int f_lineno;               /* Current line number. Only valid if non-zero */
    PyFrameState f_state;       /* What state the frame is in */
    char f_trace_lines;         /* Emit per-line trace events? */
    char f_trace_opcodes;       /* Emit per-opcode trace events? */
    PyObject *f_localsplus[1];  /* locals+stack, dynamically sized */
};

static inline int _PyFrame_IsRunnable(struct _frame *f) {
    return f->f_state < FRAME_EXECUTING;
}

static inline int _PyFrame_IsExecuting(struct _frame *f) {
    return f->f_state == FRAME_EXECUTING;
}

static inline int _PyFrameHasCompleted(struct _frame *f) {
    return f->f_state > FRAME_EXECUTING;
}

/* Standard object interface */

PyAPI_DATA(PyTypeObject) PyFrame_Type;

#define PyFrame_Check(op) Py_IS_TYPE(op, &PyFrame_Type)

PyAPI_FUNC(PyFrameObject *) PyFrame_New(PyThreadState *, PyCodeObject *,
                                        PyObject *, PyObject *);

/* only internal use */
PyFrameObject*
_PyFrame_New_NoTrack(PyThreadState *, PyFrameConstructor *, PyObject *);


/* The rest of the interface is specific for frame objects */

/* Conversions between "fast locals" and locals in dictionary */

PyAPI_FUNC(void) PyFrame_LocalsToFast(PyFrameObject *, int);

PyAPI_FUNC(int) PyFrame_FastToLocalsWithError(PyFrameObject *f);
PyAPI_FUNC(void) PyFrame_FastToLocals(PyFrameObject *);

PyAPI_FUNC(void) _PyFrame_DebugMallocStats(FILE *out);

PyAPI_FUNC(PyFrameObject *) PyFrame_GetBack(PyFrameObject *frame);
