#ifndef Py_CPYTHON_DICTOBJECT_H
#  error "this header file must not be included directly"
#endif

typedef struct _dictkeysobject PyDictKeysObject;

/* The ma_values pointer is NULL for a combined table
 * or points to an array of PyObject* for a split table
 */
typedef struct {
    /*
    字典对象，dict在内存中有两种形式
        combined-table: 如果**ma_values这个指针为空，则keys和values都存储在ma_keys（也就是PyDictKeysObject）中；
        split-table: 如果**ma_values这个指针不为空，则keys和values分别存储在ma_keys和ma_values中。
    split-table这种形式是Python3.3引进的PEP 412 -- Key-Sharing Dictionary，主要是出于对优化同一个类的实例对象的
    属性字典(__dict__)，共享同一份key，在面向对象编程中，实例化的对象较多的情况下，可以节省内存开销。
    */
    PyObject_HEAD

    /* Number of items in the dictionary */
    Py_ssize_t ma_used;

    /* Dictionary version: globally unique, value change each time
       the dictionary is modified */
    uint64_t ma_version_tag;

    PyDictKeysObject *ma_keys;

    /* If ma_values is NULL, the table is "combined": keys and values
       are stored in ma_keys.

       If ma_values is not NULL, the table is splitted:
       keys are stored in ma_keys and values are stored in ma_values */
    PyObject **ma_values;
} PyDictObject;

PyAPI_FUNC(PyObject *) _PyDict_GetItem_KnownHash(PyObject *mp, PyObject *key,
                                       Py_hash_t hash);
PyAPI_FUNC(PyObject *) _PyDict_GetItemIdWithError(PyObject *dp,
                                                  struct _Py_Identifier *key);
PyAPI_FUNC(PyObject *) _PyDict_GetItemStringWithError(PyObject *, const char *);
PyAPI_FUNC(PyObject *) PyDict_SetDefault(
    PyObject *mp, PyObject *key, PyObject *defaultobj);
PyAPI_FUNC(int) _PyDict_SetItem_KnownHash(PyObject *mp, PyObject *key,
                                          PyObject *item, Py_hash_t hash);
PyAPI_FUNC(int) _PyDict_DelItem_KnownHash(PyObject *mp, PyObject *key,
                                          Py_hash_t hash);
PyAPI_FUNC(int) _PyDict_DelItemIf(PyObject *mp, PyObject *key,
                                  int (*predicate)(PyObject *value));
PyDictKeysObject *_PyDict_NewKeysForClass(void);
PyAPI_FUNC(int) _PyDict_Next(
    PyObject *mp, Py_ssize_t *pos, PyObject **key, PyObject **value, Py_hash_t *hash);

/* Get the number of items of a dictionary. */
#define PyDict_GET_SIZE(mp)  (assert(PyDict_Check(mp)),((PyDictObject *)mp)->ma_used)
PyAPI_FUNC(int) _PyDict_Contains_KnownHash(PyObject *, PyObject *, Py_hash_t);
PyAPI_FUNC(int) _PyDict_ContainsId(PyObject *, struct _Py_Identifier *);
PyAPI_FUNC(PyObject *) _PyDict_NewPresized(Py_ssize_t minused);
PyAPI_FUNC(void) _PyDict_MaybeUntrack(PyObject *mp);
PyAPI_FUNC(int) _PyDict_HasOnlyStringKeys(PyObject *mp);
Py_ssize_t _PyDict_KeysSize(PyDictKeysObject *keys);
PyAPI_FUNC(Py_ssize_t) _PyDict_SizeOf(PyDictObject *);
PyAPI_FUNC(PyObject *) _PyDict_Pop(PyObject *, PyObject *, PyObject *);
PyObject *_PyDict_Pop_KnownHash(PyObject *, PyObject *, Py_hash_t, PyObject *);
PyObject *_PyDict_FromKeys(PyObject *, PyObject *, PyObject *);
#define _PyDict_HasSplitTable(d) ((d)->ma_values != NULL)

/* Like PyDict_Merge, but override can be 0, 1 or 2.  If override is 0,
   the first occurrence of a key wins, if override is 1, the last occurrence
   of a key wins, if override is 2, a KeyError with conflicting key as
   argument is raised.
*/
PyAPI_FUNC(int) _PyDict_MergeEx(PyObject *mp, PyObject *other, int override);
PyAPI_FUNC(int) _PyDict_SetItemId(PyObject *dp, struct _Py_Identifier *key, PyObject *item);

PyAPI_FUNC(int) _PyDict_DelItemId(PyObject *mp, struct _Py_Identifier *key);
PyAPI_FUNC(void) _PyDict_DebugMallocStats(FILE *out);

int _PyObjectDict_SetItem(PyTypeObject *tp, PyObject **dictptr, PyObject *name, PyObject *value);
PyObject *_PyDict_LoadGlobal(PyDictObject *, PyDictObject *, PyObject *);
Py_ssize_t _PyDict_GetItemHint(PyDictObject *, PyObject *, Py_ssize_t, PyObject **);

/* _PyDictView */

typedef struct {
    PyObject_HEAD
    PyDictObject *dv_dict;
} _PyDictViewObject;

PyAPI_FUNC(PyObject *) _PyDictView_New(PyObject *, PyTypeObject *);
PyAPI_FUNC(PyObject *) _PyDictView_Intersect(PyObject* self, PyObject *other);
