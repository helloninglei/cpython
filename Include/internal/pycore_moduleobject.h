#ifndef Py_INTERNAL_MODULEOBJECT_H
#define Py_INTERNAL_MODULEOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

typedef struct {
    PyObject_HEAD //头部信息
    PyObject *md_dict; //属性字典, 所有的属性和值都在里面
    struct PyModuleDef *md_def; //module对象包含的操作集合, 里面是一些结构体, 每个结构体包含一个函数的相关信息
    void *md_state;
    PyObject *md_weaklist;
    // for logging purposes after md_dict is cleared
    PyObject *md_name; //模块名
} PyModuleObject;

static inline PyModuleDef* _PyModule_GetDef(PyObject *mod) {
    assert(PyModule_Check(mod));
    return ((PyModuleObject *)mod)->md_def;
}

static inline void* _PyModule_GetState(PyObject* mod) {
    assert(PyModule_Check(mod));
    return ((PyModuleObject *)mod)->md_state;
}

static inline PyObject* _PyModule_GetDict(PyObject *mod) {
    assert(PyModule_Check(mod));
    PyObject *dict = ((PyModuleObject *)mod) -> md_dict;
    // _PyModule_GetDict(mod) must not be used after calling module_clear(mod)
    assert(dict != NULL);
    return dict;
}

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_MODULEOBJECT_H */
