"""This module implements a partial stratified tree (van Emde Boas, 1977).
Only insert findsucc, __iter__, and __contains__ are implemented.
Delete is currently not supported.
There is very little error-checking in this code -- it is designed
to be used in the limited situation described in Lopez (EMNLP-CoNLL 2007),
which doesn't cover all of the possible ways that you could misuse it
(e.g. trying to insert a key larger than the universe size)
Other notes -- this code is really rather ugly C code masquerading as
Pyrex/Python.  Virtual function calls are bypassed by hand in several
places for the sake of efficiency, and other Python niceties are 
removed for the same reason."""

from libc.stdlib cimport malloc, free
from libc.math cimport log, ceil
from libc.string cimport memset

cdef int MIN_BOTTOM_SIZE = 32
cdef int MIN_BOTTOM_BITS = 5
cdef int LOWER_MASK[32]

cdef void _init_lower_mask():
    cdef unsigned i
    cdef int mask = 0
    for i in range(MIN_BOTTOM_SIZE):
        mask = (mask << 1) + 1
        LOWER_MASK[i] = mask

_init_lower_mask()

cdef struct _BitSet:
    long bitset
    int min_val
    int max_val
    int size


cdef _BitSet* new_BitSet():
    cdef _BitSet* b

    b = <_BitSet*> malloc(sizeof(_BitSet))
    b.bitset = 0
    b.min_val = -1
    b.max_val = -1
    b.size = 0
    return b


cdef int bitset_findsucc(_BitSet* b, int i):
    cdef int bitset, mask
    cdef int low, high, mid

    if b.max_val == -1 or i >= b.max_val:
        return -1
    if i < b.min_val:
        return b.min_val

    bitset = b.bitset & ~LOWER_MASK[i]
    low = i+1
    high = b.max_val+1
    while low < high-1:
        mid = (high + low)/2
        mask = ~(LOWER_MASK[high-1] ^ LOWER_MASK[mid-1])
        if bitset & mask == 0:
            low = mid
        else: 
            bitset = bitset & mask
            high = mid
    return low


cdef int bitset_insert(_BitSet* b, int i):
    cdef int val

    val = 1 << i
    if b.bitset & val == 0:
        b.bitset = b.bitset | val
        if b.size == 0:
            b.min_val = i
            b.max_val = i
        else:
            if i < b.min_val:
                b.min_val = i
            if i > b.max_val:
                b.max_val = i
        b.size = b.size + 1
        return 1
    return 0


cdef int bitset_contains(_BitSet* b, int i):
    cdef int val

    val = 1 << i
    if b.bitset & val == 0:
        return 0
    else:
        return 1


cdef class BitSetIterator:
    cdef _BitSet* b
    cdef int next_val

    def __next__(self):
        cdef int ret_val

        if self.next_val == -1:
            raise StopIteration()
        ret_val = self.next_val
        self.next_val = bitset_findsucc(self.b, ret_val)
        return ret_val



# This is a Python wrapper class to give access to the
# (entirely C-implemented) _BitSet struct. 
# Very slow; use only for debugging
cdef class BitSet:

    cdef _BitSet* b

    def __cinit__(self):
        self.b = new_BitSet()

    def __dealloc__(self):
        free(self.b)

    def __iter__(self):
        cdef BitSetIterator it
        it = BitSetIterator()
        it.b = self.b
        it.next_val = self.b.min_val
        return it

    def insert(self, i):
        return bitset_insert(self.b, i)

    def findsucc(self, i):
        return bitset_findsucc(self.b, i)

    def __str__(self):
        return dec2bin(self.b.bitset)+"  ("+str(self.b.size)+","+str(self.b.min_val)+","+str(self.b.max_val)+")"

    def min(self):
        return self.b.min_val

    def max(self):
        return self.b.max_val

    def __len__(self):
        return self.b.size

    def __contains__(self, i):
        return bool(bitset_contains(self.b, i))


cdef str dec2bin(long i):
    cdef str result = ""
    cdef unsigned d
    for d in range(MIN_BOTTOM_SIZE):
        if i & LOWER_MASK[0] == 0:
            result = "0"+result
        else:
            result = "1"+result
        i = i >> 1
    return result

cdef struct _VEB:
    int top_universe_size
    int num_bottom_bits
    int max_val
    int min_val
    int size
    void* top
    void** bottom

cdef _VEB* new_VEB(int n):
    cdef _VEB* veb
    cdef int num_bits, num_top_bits, i

    veb = <_VEB*> malloc(sizeof(_VEB))

    num_bits = int(ceil(log(n) / log(2)))
    veb.num_bottom_bits = num_bits/2
    if veb.num_bottom_bits < MIN_BOTTOM_BITS:
        veb.num_bottom_bits = MIN_BOTTOM_BITS
    veb.top_universe_size = (n >> veb.num_bottom_bits) + 1

    veb.bottom = <void**> malloc(veb.top_universe_size * sizeof(void*))
    memset(veb.bottom, 0, veb.top_universe_size * sizeof(void*))

    if veb.top_universe_size > MIN_BOTTOM_SIZE:
        veb.top = new_VEB(veb.top_universe_size)
    else:
        veb.top = new_BitSet()

    veb.max_val = -1
    veb.min_val = -1
    veb.size = 0
    return veb


cdef int VEB_insert(_VEB* veb, int i):
    cdef _VEB* subv
    cdef _BitSet* subb
    cdef int a, b, tmp

    if veb.size == 0:
        veb.min_val = i
        veb.max_val = i
    elif i == veb.min_val or i == veb.max_val:
        return 0
    else:
        if i < veb.min_val:
            tmp = i
            i = veb.min_val
            veb.min_val = tmp
        a = i >> veb.num_bottom_bits
        b = i & LOWER_MASK[veb.num_bottom_bits-1]
        if veb.bottom[a] == NULL:
            if veb.top_universe_size > MIN_BOTTOM_SIZE:
                subv = <_VEB*> veb.top
                VEB_insert(subv, a)
            else:
                subb = <_BitSet*> veb.top
                bitset_insert(subb, a)
            if veb.num_bottom_bits > MIN_BOTTOM_BITS:
                veb.bottom[a] = new_VEB(1 << veb.num_bottom_bits)
            else:
                veb.bottom[a] = new_BitSet()
        if veb.num_bottom_bits > MIN_BOTTOM_BITS:
            subv = <_VEB*> veb.bottom[a]
            if VEB_insert(subv, b) == 0:
                return 0
        else:
            subb = <_BitSet*> veb.bottom[a]
            if bitset_insert(subb, b) == 0:
                return 0

        if i > veb.max_val:
            veb.max_val = i
    veb.size = veb.size + 1
    return 1


cdef del_VEB(_VEB* veb):
    cdef int i

    if veb.top_universe_size > MIN_BOTTOM_SIZE:
        i = (<_VEB*> veb.top).min_val
    else:
        i = (<_BitSet*> veb.top).min_val

    while i != -1:
        if veb.num_bottom_bits > MIN_BOTTOM_BITS:
            del_VEB(<_VEB*> veb.bottom[i])
        else:
            free(<_BitSet*> veb.bottom[i])

        if veb.top_universe_size > MIN_BOTTOM_SIZE:
            i = VEB_findsucc(<_VEB*> veb.top, i)
        else:
            i = bitset_findsucc(<_BitSet*> veb.top, i)

    if veb.top_universe_size > MIN_BOTTOM_SIZE:
        del_VEB(<_VEB*> veb.top)
    else:
        free(<_BitSet*> veb.top)
    free(veb.bottom)
    free(veb)


cdef int VEB_findsucc(_VEB* veb, int i):
    cdef _VEB* subv
    cdef _BitSet* subb
    cdef int a, b, j, c, found

    if veb.max_val == -1 or i>=veb.max_val:
        return -1
    if i < veb.min_val:
        return veb.min_val

    a = i >> veb.num_bottom_bits
    b = i & LOWER_MASK[veb.num_bottom_bits-1]
    found = 0
    if veb.bottom[a] != NULL:
        if veb.num_bottom_bits > MIN_BOTTOM_BITS:
            subv = <_VEB*> veb.bottom[a]
            if subv.max_val > b:
                j = (a << veb.num_bottom_bits) + VEB_findsucc(subv, b)
                found = 1
        else:
            subb = <_BitSet*> veb.bottom[a]
            if subb.max_val > b:
                j = (a << veb.num_bottom_bits) + bitset_findsucc(subb, b)
                found = 1
    if found==0:
        if veb.top_universe_size > MIN_BOTTOM_SIZE:
            subv = <_VEB*> veb.top
            c = VEB_findsucc(subv, a)
        else:
            subb = <_BitSet*> veb.top
            c = bitset_findsucc(subb, a)
        if veb.num_bottom_bits > MIN_BOTTOM_BITS:
            subv = <_VEB*> veb.bottom[c]
            j = (c << veb.num_bottom_bits) + subv.min_val
        else:
            subb = <_BitSet*> veb.bottom[c]
            j = (c << veb.num_bottom_bits) + subb.min_val
    return j


cdef int VEB_contains(_VEB* veb, int i):
    cdef _VEB* subv
    cdef _BitSet* subb
    cdef int a, b

    if veb.size == 0 or i < veb.min_val or i > veb.max_val:
        return 0

    if veb.min_val == i:
        return 1
    else:
        if veb.size == 1:
            return 0

    a = i >> veb.num_bottom_bits
    b = i & LOWER_MASK[veb.num_bottom_bits-1]
    if veb.bottom[a] == NULL:
        return 0
    else:
        if veb.num_bottom_bits > MIN_BOTTOM_BITS:
            subv = <_VEB*> veb.bottom[a]
            return VEB_contains(subv, b)
        else:
            subb = <_BitSet*> veb.bottom[a]
            return bitset_contains(subb, b)


cdef class VEBIterator:
    cdef _VEB* v
    cdef int next_val

    def __next__(self):
        cdef int ret_val

        if self.next_val == -1:
            raise StopIteration()
        ret_val = self.next_val
        self.next_val = VEB_findsucc(self.v, ret_val)
        return ret_val


cdef class VEB:
    cdef _VEB* veb
    cdef int _findsucc(self, int i)
    cdef int _insert(self, int i)
    cdef int _first(self)

    def __cinit__(self, int size):
        self.veb = new_VEB(size)

    def __dealloc__(self):
        del_VEB(self.veb)

    def __iter__(self):
        cdef VEBIterator it
        it = VEBIterator()
        it.v = self.veb
        it.next_val = self.veb.min_val
        return it

    def insert(self, i):
        return VEB_insert(self.veb, i)

    cdef int _insert(self, int i):
        return VEB_insert(self.veb, i)

    def findsucc(self, i):
        return VEB_findsucc(self.veb, i)

    cdef int _first(self):
        return self.veb.min_val

    cdef int _findsucc(self, int i):
        return VEB_findsucc(self.veb, i)

    def __len__(self):
        return self.veb.size

    def __contains__(self, i):
        return VEB_contains(self.veb, i)
