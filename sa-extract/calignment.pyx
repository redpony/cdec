import log
import gzip
import cintlist

from libc.stdio cimport FILE, fopen, fread, fwrite, fclose
from libc.stdlib cimport malloc, realloc, free

# Note: Callison-Burch uses short instead of int.  
# We have the space for our corpus, so this is not a problem;
# May need to revisit if things get really tight, though.
cdef class Alignment:


	cdef int link(self, int i, int j):
		'''Integerizes an alignment link pair'''
		return i*65536 + j


	def unlink(self, link):
		'''De-integerizes an alignment link pair'''
		return (link/65536, link%65536)


	cdef _unlink(self, int link, int* f, int* e):
		f[0] = link/65536
		e[0] = link%65536


	def get_sent_links(self, int sent_id):
		cdef cintlist.CIntList sent_links
		cdef int* arr
		cdef int arr_len

		sent_links = cintlist.CIntList()
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


	def __cinit__(self, filename, from_binary=False):
		self.links = cintlist.CIntList(1000,1000)
		self.sent_index = cintlist.CIntList(1000,1000)
		log.writeln("Reading alignment from file %s" % filename)
		if from_binary:
			self.read_binary(filename)
		else:
			self.read_text(filename)


	def read_text(self, filename):
		if filename[-2:] == "gz":
			f = gzip.GzipFile(filename)
		else:
			f = open(filename)
		for line in f:
			self.sent_index.append(len(self.links))
			pairs = line.split()
			for pair in pairs:
				(i, j) = map(int, pair.split('-'))
				self.links.append(self.link(i, j))
		self.sent_index.append(len(self.links))


	def read_binary(self, filename):
		cdef FILE* f
		cdef bytes bfilename = filename
		cdef char* cfilename = bfilename
		f = fopen(cfilename, "r")
		self.links.read_handle(f)
		self.sent_index.read_handle(f)
		fclose(f)


	def write_text(self, filename):
		f = open(filename, "w")
		sent_num = 0
		for i, link in enumerate(self.links):
			while i >= self.sent_index[sent_num]:
				f.write("\n")
				sent_num = sent_num + 1
			f.write("%d-%d " % self.unlink(link))
		f.write("\n")


	def write_binary(self, filename):
		cdef FILE* f
		cdef bytes bfilename = filename
		cdef char* cfilename = bfilename
		f = fopen(cfilename, "w")
		self.links.write_handle(f)
		self.sent_index.write_handle(f)
		fclose(f)


	def write_enhanced(self, filename):
		f = open(filename, "w")
		sent_num = 1
		for link in self.links:
			f.write("%d " % link)
		f.write("\n")
		for i in self.sent_index:
			f.write("%d " % i)
		f.write("\n")


	def alignment(self, i):
		'''Return all (e,f) pairs for sentence i'''
		cdef int j, start, end
		result = []
		start = self.sent_index.arr[i]
		end = self.sent_index.arr[i+1]
		for j from start <= j < end:
			result.append(self.unlink(self.links.arr[j]))
		return result
