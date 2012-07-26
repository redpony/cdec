%.c: %.pyx
	cython -X cdivision=True $< -o $@

%.o: %.cc
	g++ -O6 -g -fPIC -c $<

all: cstrmap.c strmap.cc rule.c sym.c cdat.c cintlist.c cfloatlist.c calignment.c csuf.c clex.c rulefactory.c cveb.c lcp.c precomputation.c
	python setup.py build

clean:
	rm -f cdat.c cstrmap.c sym.c rule.c cintlist.c cfloatlist.c calignment.c csuf.c clex.c rulefactory.c cveb.c lcp.c precomputation.c *.so *.o *.cxx *~ *.pyc
	rm -rf build
