# Defines suffix arrays that can be directly written to/read from disk in binary format
# Adam Lopez <alopez@cs.umd.edu>

from libc.stdio cimport FILE, fclose, fopen

cdef class SuffixArray:
    cdef DataArray darray
    cdef IntList sa
    cdef IntList ha

    def __cinit__(self, from_binary=None, from_text=None, side=None):
        self.darray = DataArray()
        self.sa = IntList()
        self.ha = IntList()
        if from_binary:
            self.read_binary(from_binary)
        elif from_text:
            self.read_text(from_text, side)

    def __getitem__(self, i):
        return self.sa.arr[i]

    def read_text(self, filename, side):
        '''Constructs suffix array using the algorithm
        of Larsson & Sadahkane (1999)'''
        cdef int V, N, i, j, h, a_i, n, current_run, skip
        cdef IntList isa, word_count

        self.darray = DataArray(from_text=filename, side=side, use_sent_id=True)
        N = len(self.darray)
        V = len(self.darray.id2word)

        self.sa = IntList(initial_len=N)
        self.ha = IntList(initial_len=V+1)

        isa = IntList(initial_len=N)
        word_count = IntList(initial_len=V+1)

        '''Step 1: bucket sort data'''
        cdef float sort_start_time = monitor_cpu()
        cdef float start_time = sort_start_time
        for i from 0 <= i < N:
            a_i = self.darray.data.arr[i]
            word_count.arr[a_i] = word_count.arr[a_i] + 1

        n = 0
        for i from 0 <= i < V+1:
            self.ha.arr[i] = n
            n = n + word_count.arr[i]
            word_count.arr[i] = 0

        for i from 0 <= i < N:
            a_i = self.darray.data.arr[i]
            self.sa.arr[self.ha.arr[a_i] + word_count.arr[a_i]] = i
            isa.arr[i] = self.ha.arr[a_i + 1] - 1 # bucket pointer is last index in bucket
            word_count.arr[a_i] = word_count.arr[a_i] + 1

        '''Determine size of initial runs'''
        current_run = 0
        for i from 0 <= i < V+1:
            if i < V and self.ha.arr[i+1] - self.ha.arr[i] == 1:
                current_run = current_run + 1
            else:
                if current_run > 0:
                    self.sa.arr[self.ha.arr[i] - current_run] = -current_run
                    current_run = 0

        logger.info("    Bucket sort took %f seconds", (monitor_cpu() - sort_start_time))

        '''Step 2: prefix-doubling sort'''
        h = 1
        while self.sa.arr[0] != -N:
            sort_start_time = monitor_cpu()
            logger.debug("    Refining, sort depth = %d", h)
            i = 0
            skip = 0
            while i < N:
                if self.sa.arr[i] < 0:
                    skip = skip + self.sa.arr[i]
                    i = i - self.sa.arr[i]
                else:
                    if skip < 0:
                        self.sa.arr[i+skip] = skip
                        skip = 0
                    j = isa.arr[self.sa.arr[i]]
                    self.q3sort(i, j, h, isa)
                    i = j+1
            if skip < 0:
                self.sa.arr[i+skip] = skip
            h = h * 2
            logger.debug("    Refinement took %f seconds", (monitor_cpu() - sort_start_time))

        '''Step 3: read off suffix array from inverse suffix array''' 
        logger.info("    Finalizing sort...")
        for i from 0 <= i < N:
            j = isa.arr[i]
            self.sa.arr[j] = i
        logger.info("Suffix array construction took %f seconds", (monitor_cpu() - start_time))

    def q3sort(self, int i, int j, int h, IntList isa, pad=""):
        '''This is a ternary quicksort. It divides the array into
        three partitions: items less than the pivot, items equal
        to pivot, and items greater than pivot.    The first and last
        of these partitions are then recursively sorted'''
        cdef int k, midpoint, pval, phead, ptail, tmp

        if j-i < -1:
            raise Exception("Unexpected condition found in q3sort: sort from %d to %d" % (i,j))
        if j-i == -1:    # recursive base case -- empty interval
            return
        if (j-i == 0):    # recursive base case -- singleton interval
            isa.arr[self.sa.arr[i]] = i
            self.sa.arr[i] = -1
            return 

        # NOTE: choosing the first item as a pivot value resulted in 
        # stack overflow for some very large buckets.    I think there
        # is a natural bias towards order due the way the word ids are
        # assigned; thus this resulted in the range to the left of the 
        # pivot being nearly empty.     Therefore, choose the middle item.
        # If the method of assigning word_id's is changed, this method
        # may need to be reconsidered as well.
        midpoint = (i+j)/2
        pval = isa.arr[self.sa.arr[midpoint] + h]
        if i != midpoint:
            tmp = self.sa.arr[midpoint]
            self.sa.arr[midpoint] = self.sa.arr[i]
            self.sa.arr[i] = tmp
        phead = i
        ptail = i

        # find the three partitions.    phead marks the first element
        # of the middle partition, and ptail marks the last element
        for k from i+1 <= k < j+1:
            if isa.arr[self.sa.arr[k] + h] < pval:
                if k > ptail+1:
                    tmp = self.sa.arr[phead]
                    self.sa.arr[phead] = self.sa.arr[k]
                    self.sa.arr[k] = self.sa.arr[ptail+1]
                    self.sa.arr[ptail+1] = tmp
                else: # k == ptail+1
                    tmp = self.sa.arr[phead]
                    self.sa.arr[phead] = self.sa.arr[k]
                    self.sa.arr[k] = tmp
                phead = phead + 1
                ptail = ptail + 1
            else:
                if isa.arr[self.sa.arr[k] + h] == pval:
                    if k > ptail+1:
                        tmp = self.sa.arr[ptail+1]
                        self.sa.arr[ptail+1] = self.sa.arr[k]
                        self.sa.arr[k] = tmp
                    ptail = ptail + 1

        # recursively sort smaller suffixes
        self.q3sort(i, phead-1, h, isa, pad+"    ")

        # update suffixes with pivot value
        # corresponds to update_group function in Larsson & Sadakane
        for k from phead <= k < ptail+1:
            isa.arr[self.sa.arr[k]] = ptail
        if phead == ptail:
            self.sa.arr[phead] = -1

        # recursively sort larger suffixes
        self.q3sort(ptail+1, j, h, isa, pad+"    ")


    def write_text(self, char* filename):
        self.darray.write_text(filename)

    def read_binary(self, char* filename):
        cdef FILE *f
        f = fopen(filename, "r")
        self.darray.read_handle(f)
        self.sa.read_handle(f)
        self.ha.read_handle(f)
        fclose(f)

    def write_binary(self, char* filename):
        cdef FILE* f
        f = fopen(filename, "w")
        self.darray.write_handle(f)
        self.sa.write_handle(f)
        self.ha.write_handle(f)
        fclose(f)

    def write_enhanced(self, char* filename):
        with open(filename, "w") as f:
            self.darray.write_enhanced_handle(f)
            for a_i in self.sa:
                f.write("%d " % a_i)
            f.write("\n")
            for w_i in self.ha:
                f.write("%d " % w_i)
            f.write("\n")

    cdef int __search_high(self, int word_id, int offset, int low, int high):
        cdef int midpoint

        if low >= high:
            return high
        midpoint = (high + low) / 2
        if self.darray.data.arr[self.sa.arr[midpoint] + offset] == word_id:
            return self.__search_high(word_id, offset, midpoint+1, high)
        else:
            return self.__search_high(word_id, offset, low, midpoint)

    cdef int __search_low(self, int word_id, int offset, int low, int high):
        cdef int midpoint

        if low >= high:
            return high
        midpoint = (high + low) / 2
        if self.darray.data.arr[self.sa.arr[midpoint] + offset] == word_id:
            return self.__search_low(word_id, offset, low, midpoint)
        else:
            return self.__search_low(word_id, offset, midpoint+1, high)

    cdef __get_range(self, int word_id, int offset, int low, int high, int midpoint):
        return (self.__search_low(word_id, offset, low, midpoint),
                self.__search_high(word_id, offset, midpoint, high))

    cdef __lookup_helper(self, int word_id, int offset, int low, int high):
        cdef int midpoint

        if offset == 0:
            return (self.ha.arr[word_id], self.ha.arr[word_id+1])
        if low >= high:
            return None

        midpoint = (high + low) / 2
        if self.darray.data.arr[self.sa.arr[midpoint] + offset] == word_id:
            return self.__get_range(word_id, offset, low, high, midpoint)
        if self.darray.data.arr[self.sa.arr[midpoint] + offset] > word_id:
            return self.__lookup_helper(word_id, offset, low, midpoint)
        else:
            return self.__lookup_helper(word_id, offset, midpoint+1, high)

    def lookup(self, word, int offset, int low, int high):
        cdef int wordid
        if low == -1: 
            low = 0
        if high == -1:
            high = len(self.sa)
        if word in self.darray.word2id:
            word_id = self.darray.word2id[word]
            return self.__lookup_helper(word_id, offset, low, high)
        else:
            return None
