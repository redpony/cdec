
class Model(object):
    def __init__(self, name=None):
        object.__init__(self)
        if name is None:
            self.name = self.__class__.__name__
        else:
            self.name = name

    def input(self, fwords, meta):
        pass

