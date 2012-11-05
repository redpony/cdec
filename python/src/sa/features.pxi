cdef StringMap FD = StringMap() # Feature name dictionary

INITIAL_CAPACITY = 7 # default number of features
INCREMENT = INITIAL_CAPACITY # double size

cdef class FeatureVector:
    def __cinit__(self):
        self.names = IntList(INITIAL_CAPACITY, INCREMENT)
        self.values = FloatList(INITIAL_CAPACITY, INCREMENT)

    def set(self, unsigned name, float value):
        self.names.append(name)
        self.values.append(value)

    def __iter__(self):
        cdef unsigned i
        for i in range(self.names.len):
            yield (FD.word(self.names[i]), self.values[i])

    def __str__(self):
        return ' '.join('%s=%s' % feat for feat in self)

cdef class Scorer:
    cdef models
    def __init__(self, *models):
        names = [FD.index(<char *>model.__name__) for model in models]
        self.models = zip(names, models)

    cdef FeatureVector score(self, ctx):
        cdef FeatureVector scores = FeatureVector()
        for name, model in self.models:
            scores.set(name, model(ctx))
        return scores
