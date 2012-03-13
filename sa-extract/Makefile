PYVER=python2.7
PYDIR=/usr/local/Cellar/python/2.7.2
PYINCLUDE=$(PYDIR)/include/$(PYVER)
CYTHON=/usr/local/share/python/cython
PYTHON=$(PYDIR)/bin/python

%.c: %.pyx
	$(CYTHON) $< -o $@

%.o: %.cc
	g++ -O6 -g -fPIC -c $<

all: cstrmap.c strmap.cc rule.c sym.c cdat.c cintlist.c cfloatlist.c calignment.c csuf.c clex.c rulefactory.c cveb.c lcp.c precomputation.c
	$(PYTHON) setup.py build

clean:
	rm -f cdat.c cstrmap.c sym.c rule.c cintlist.c cfloatlist.c calignment.c csuf.c clex.c rulefactory.c cveb.c lcp.c precomputation.c *.so *.o *.cxx *~ *.pyc
	rm -rf build
