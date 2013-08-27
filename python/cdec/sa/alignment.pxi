from libc.stdio cimport FILE, fopen, fread, fwrite, fclose
from libc.stdlib cimport malloc, realloc, free

# Note: Callison-Burch uses short instead of int.  
# We have the space for our corpus, so this is not a problem;
# May need to revisit if things get really tight, though.

cdef int ALIGNMENT_CODE = 1 << 16

cdef class Alignment:
    cdef IntList links
    cdef IntList sent_index

    cdef int link(self, int i, int j):
        """Integerizes an alignment link pair"""
        return i * ALIGNMENT_CODE + j

    def unlink(self, link):
        """De-integerizes an alignment link pair"""
        return (link / ALIGNMENT_CODE, link % ALIGNMENT_CODE)

    cdef _unlink(self, int link, int* f, int* e):
        f[0] = link / ALIGNMENT_CODE
        e[0] = link % ALIGNMENT_CODE

    def get_sent_links(self, int sent_id):
        cdef IntList sent_links
        cdef int* arr
        cdef int arr_len
        sent_links = IntList()
        arr = self._get_sent_links(sent_id, &arr_len)
        sent_links._extend_arr(arr, arr_len*2)
        free(arr)
        return sent_links

    cdef int* _get_sent_links(self, int sent_id, int* num_links):
        cdef int* sent_links
        cdef int i, start, end
        start = self.sent_index.arr[sent_id]
        end = self.sent_index.arr[sent_id+1]
        num_links[0] = end - start
        sent_links = <int*> malloc(2*num_links[0]*sizeof(int))
        for i from 0 <= i < num_links[0]:
            self._unlink(self.links.arr[start + i], sent_links + (2*i), sent_links + (2*i) + 1)
        return sent_links

    def __cinit__(self, from_binary=None, from_text=None):
        self.links = IntList(1000,1000)
        self.sent_index = IntList(1000,1000)
        if from_binary:
            self.read_binary(from_binary)
        elif from_text:
            self.read_text(from_text)

    def read_text(self, char* filename):
        with gzip_or_text(filename) as f:
            for line in f:
                self.sent_index.append(len(self.links))
                pairs = line.split()
                for pair in pairs:
                    (i, j) = map(int, pair.split('-'))
                    self.links.append(self.link(i, j))
            self.sent_index.append(len(self.links))

    def read_binary(self, char* filename):
        cdef FILE* f
        f = fopen(filename, "r")
        self.links.read_handle(f)
        self.sent_index.read_handle(f)
        fclose(f)

    def write_text(self, char* filename):
        with open(filename, "w") as f:
            sent_num = 0
            for i, link in enumerate(self.links):
                while i >= self.sent_index[sent_num]:
                    f.write("\n")
                    sent_num = sent_num + 1
                f.write("%d-%d " % self.unlink(link))
            f.write("\n")

    def write_binary(self, char* filename):
        cdef FILE* f
        f = fopen(filename, "w")
        self.links.write_handle(f)
        self.sent_index.write_handle(f)
        fclose(f)

    def write_enhanced(self, char* filename):
        with open(filename, "w") as f:
            for link in self.links:
                f.write("%d " % link)
            f.write("\n")
            for i in self.sent_index:
                f.write("%d " % i)
            f.write("\n")

    def alignment(self, i):
        """Return all (e,f) pairs for sentence i"""
        cdef int j, start, end
        result = []
        start = self.sent_index.arr[i]
        end = self.sent_index.arr[i+1]
        for j from start <= j < end:
            result.append(self.unlink(self.links.arr[j]))
        return result
