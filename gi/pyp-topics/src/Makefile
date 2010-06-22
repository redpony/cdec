-include makefile.darwin

local_objs = ../obj/corpus.o ../obj/gzstream.o ../obj/mt19937ar.o ../obj/pyp-topics.o ../obj/gammadist.o

all: ../bin/pyp-topics-train

-include makefile.depend

#-----------------------#
# Local stuff
#-----------------------#

../bin/pyp-topics-train: ../obj/train.o $(local_objs)
	$(CXX) -o $@ $^ $(LDFLAGS)

../obj/%.o: %.cc
	${CXX} $(CXXFLAGS) -c $< -o $@

../obj/%.o: %.c
	${CC} $(CFLAGS) -c $< -o $@

.PHONY: depend
depend:
	$(CXX) -MM $(CXXFLAGS) *.cc | sed 's/^\(.*\.o:\)/obj\/\1/' > makefile.depend

clean:
	rm -f ../obj/*.o

#clobber: clean
#	rm makefile.depend ../bin/${ARCH}/*
