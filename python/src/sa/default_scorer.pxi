from libc.stdlib cimport malloc, realloc, free
from libc.math cimport log10

MAXSCORE = -99
EgivenFCoherent = 0
SampleCountF = 1
CountEF = 2
MaxLexFgivenE = 3
MaxLexEgivenF = 4
IsSingletonF = 5
IsSingletonFE = 6
NFEATURES = 7

cdef class DefaultScorer(Scorer):
    cdef BiLex ttable
    cdef int* fid

    def __dealloc__(self):
        free(self.fid)

    def __init__(self, BiLex ttable):
        self.ttable = ttable
        self.fid = <int*> malloc(NFEATURES*sizeof(int))
        cdef unsigned i
        for i, fnames in enumerate(('EgivenFCoherent', 'SampleCountF', 'CountEF',
                'MaxLexFgivenE', 'MaxLexEgivenF', 'IsSingletonF', 'IsSingletonFE')):
            self.fid[i] = FD.index(fnames)

    cdef FeatureVector score(self, Phrase fphrase, Phrase ephrase,
            unsigned paircount, unsigned fcount, unsigned fsample_count):
        cdef FeatureVector scores = FeatureVector()

        #  EgivenFCoherent
        cdef float efc = <float>paircount/fsample_count
        scores.set(self.fid[EgivenFCoherent], -log10(efc) if efc > 0 else MAXSCORE)

        # SampleCountF
        scores.set(self.fid[SampleCountF], log10(1 + fsample_count))

        # CountEF
        scores.set(self.fid[CountEF], log10(1 + paircount))

        # MaxLexFgivenE TODO typify
        ewords = ephrase.words
        ewords.append('NULL')
        cdef float mlfe = 0, max_score = -1
        for f in fphrase.words:
            for e in ewords:
                score = self.ttable.get_score(f, e, 1)
                if score > max_score:
                    max_score = score
            mlfe += -log10(max_score) if max_score > 0 else MAXSCORE
        scores.set(self.fid[MaxLexFgivenE], mlfe)

        # MaxLexEgivenF TODO same
        fwords = fphrase.words
        fwords.append('NULL')
        cdef float mlef = 0
        max_score = -1
        for e in ephrase.words:
            for f in fwords:
                score = self.ttable.get_score(f, e, 0)
                if score > max_score:
                    max_score = score
            mlef += -log10(max_score) if max_score > 0 else MAXSCORE
        scores.set(self.fid[MaxLexEgivenF], mlef)

        # IsSingletonF
        scores.set(self.fid[IsSingletonF], (fcount == 1))

        # IsSingletonFE
        scores.set(self.fid[IsSingletonFE], (paircount == 1))

        return scores
