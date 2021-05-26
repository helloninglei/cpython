#ifndef Py_DICT_COMMON_H
#define Py_DICT_COMMON_H

typedef struct {
    /* Cached hash code of me_key. */
    Py_hash_t me_hash;
    PyObject *me_key;
    PyObject *me_value; /* This field is only meaningful for combined tables */
} PyDictKeyEntry;

/* dict_lookup_func() returns index of entry which can be used like DK_ENTRIES(dk)[index].
 * -1 when no entry found, -3 when compare raises error.
 */
typedef Py_ssize_t (*dict_lookup_func)
    (PyDictObject *mp, PyObject *key, Py_hash_t hash, PyObject **value_addr);

#define DKIX_EMPTY (-1)
#define DKIX_DUMMY (-2)  /* Used internally */
#define DKIX_ERROR (-3)

/* See dictobject.c for actual layout of DictKeysObject */
struct _dictkeysobject {
    /*
    实现了Python字典的哈希表
    给字典my_dict={}添加一个值，key为hello，value为word
    my_dict['hello'] = 'word'

    # 假设是一个空列表，hash表初始如下
    indices = [None, None, None, None, None, None]
    enteies = []

    hash_value = hash('hello')  # 假设值为 12343543
    index = hash_value & ( len(indices) - 1)  # 假设index值计算后等于3

    # 会找到indices的index为3的位置，并插入enteies的长度
    indices = [None, None, None, 0, None, None]
    # 此时enteies会插入第一个元素
    enteies = [
        [12343543, 'hello', 'word']
    ]

    # 我们继续向字典中添加值
    my_dict['haimeimei'] = 'lihua'

    hash_value = hash('haimeimei')  # 假设值为 34323545
    index = hash_value & ( len(indices) - 1)  # 假设index值计算后同样等于 0

    # 会找到indices的index为0的位置，并插入enteies的长度
    indices = [1, None, None, 0, None, None]
    # 此时enteies会插入第一个元素
    enteies = [
        [12343543, 'hello', 'word'],
        [34323545, 'haimeimei', 'lihua']
    ]

    dk_indices是实际的哈希表，对应一个slot数组，存储着index。哈希表的每个slot有4种状态：
        Unused，这是每个slot的初始状态，slot中的index == DKIX_EMPTY，表示该slot没有被使用，这个状态可以转化为Active。
        Active，index >= 0，index对应到dk_entries的PyDictKeyEntry对象，me_key != NULL 和 me_value != NULL。
        Dummy，index == DKIX_DUMMY，当dict中一个元素被删除，slot就会从原来的Active状态变成Dummy状态，原先对应的PyDictKeyEntry对象不会被删掉，Dummy可以在key被再次插入的时候转化为Active状态，但是不能转化为Unused状态。这种状态只在combined-table中出现。
        Pending，index >= 0, key != NULL, and value == NULL，这种状态只在split-table中出现，表示还未插入到split-table中。
    */
    Py_ssize_t dk_refcnt;

    /* Size of the hash table (dk_indices). It must be a power of 2. */
    Py_ssize_t dk_size;

    /* Function to lookup in the hash table (dk_indices):

       - lookdict(): general-purpose, and may return DKIX_ERROR if (and
         only if) a comparison raises an exception.

       - lookdict_unicode(): specialized to Unicode string keys, comparison of
         which can never raise an exception; that function can never return
         DKIX_ERROR.

       - lookdict_unicode_nodummy(): similar to lookdict_unicode() but further
         specialized for Unicode string keys that cannot be the <dummy> value.

       - lookdict_split(): Version of lookdict() for split tables. */
    dict_lookup_func dk_lookup;

    /* Number of usable entries in dk_entries. */
    Py_ssize_t dk_usable;

    /* Number of used entries in dk_entries. */
    Py_ssize_t dk_nentries;

    /* Actual hash table of dk_size entries. It holds indices in dk_entries,
       or DKIX_EMPTY(-1) or DKIX_DUMMY(-2).

       Indices must be: 0 <= indice < USABLE_FRACTION(dk_size).

       The size in bytes of an indice depends on dk_size:

       - 1 byte if dk_size <= 0xff (char*)
       - 2 bytes if dk_size <= 0xffff (int16_t*)
       - 4 bytes if dk_size <= 0xffffffff (int32_t*)
       - 8 bytes otherwise (int64_t*)

       Dynamically sized, SIZEOF_VOID_P is minimum. */
    char dk_indices[];  /* char is required to avoid strict aliasing. */

    /* "PyDictKeyEntry dk_entries[dk_usable];" array follows:
       see the DK_ENTRIES() macro */
};

#endif
