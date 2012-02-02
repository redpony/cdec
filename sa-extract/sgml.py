import sys, sgmllib, xml.sax.saxutils, sym

def attrs_to_str(d):
    if len(d) == 0:
        return ""
    l = [""]+["%s=%s" % (name, xml.sax.saxutils.quoteattr(value)) for (name, value) in d]
    return " ".join(l)

def attrs_to_dict(a):
    d = {}
    for (name, value) in a:
	if d.has_key(name.lower()):
	    raise ValueError, "duplicate attribute names"
	d[name.lower()] = value
    return d

class Sentence(object):
    def __init__(self, words=None, meta=None):
        if words is not None:
            self.words = list(words)
        else:
            self.words = []
        if meta is not None:
            self.meta = meta
        else:
            self.meta = []

    def copy(self):
        return Sentence(self.words, list(self.meta))

    def mark(self, tag, attrs):
        self.meta.append((tag, attrs, 0, len(self.words)))

    def getmark(self):
        if len(self.meta) > 0:
            (tag, attrs, i, j) = self.meta[-1]
            if i == 0 and j == len(self.words):
                return (tag, attrs)
            else:
                return None
        else:
            return None

    def unmark(self):
        mark = self.getmark()
        if mark is not None:
            self.meta = self.meta[:-1]
        return mark

    def __cmp__(self, other):
        return cmp((self.words, self.meta), (other.words, other.meta))

    def __str__(self):
        def cmp_spans((tag1,attr1,i1,j1),(tag2,attr2,i2,j2)):
            if i1==i2<=j1==j2:
                return 0
            elif i2<=i1<=j1<=j2:
                return -1
            elif i1<=i2<=j2<=j1:
                return 1
            else:
                return cmp((i1,j1),(i2,j2)) # don't care
        # this guarantees that equal spans will come out nested
        # we want the later spans to be outer
        # this relies on stable sort
        open = [[] for i in xrange(len(self.words)+1)]
        # there seems to be a bug still with empty spans
        empty = [[] for i in xrange(len(self.words)+1)]
        close = [[] for j in xrange(len(self.words)+1)]
        for (tag,attrs,i,j) in sorted(self.meta, cmp=cmp_spans):
            if i == j:
                # do we want these to nest?
                empty[i].append("<%s%s></%s>\n" % (tag, attrs_to_str(attrs), tag))
            else:
                open[i].append("<%s%s>\n" % (tag, attrs_to_str(attrs)))
                close[j].append("</%s>\n" % tag)

        result = []
        if len(empty[0]) > 0:
            result.extend(empty[0])
        for i in xrange(len(self.words)):
            if i > 0:
                result.append(" ")
            result.extend(reversed(open[i]))
            result.append(xml.sax.saxutils.escape(sym.tostring(self.words[i])))
            result.extend(close[i+1])
            if len(empty[i+1]) > 0:
                result.extend(empty[i+1])

        return "".join(result)

    def __add__(self, other):
        if type(other) in (list, tuple):
            return Sentence(self.words + list(other), self.meta)
        else:
            othermeta = [(tag, attrs, i+len(self.words), j+len(self.words)) for (tag, attrs, i, j) in other.meta]
            return Sentence(self.words + other.words, self.meta+othermeta)

def read_raw(f):
    """Read a raw file into a list of Sentences."""
    if type(f) is str:
        f = file(f, "r")
    i = 0
    line = f.readline()
    while line != "":
        sent = process_sgml_line(line, i)
        mark = sent.getmark()
        if mark is None:
            sent.mark('seg', [('id',str(i))])
        else:
            (tag, attrs) = mark
            if tag == "seg" and not attrs_to_dict(attrs).has_key('id'):
                x = ('id',str(i))
                attrs.append(x)
                sent.mark('seg', attrs)
            if tag != "seg":
                sent.mark('seg', [('id',str(i))])
        yield sent
        i += 1
        line = f.readline()

def process_sgml_line(line, id=None):
    p = DatasetParser(None)
    p.pos = 0
    p.words = []
    p.meta = []
    p.feed(line)
    p.close()
    sent = Sentence(p.words, p.meta)
    return sent

class DatasetParser(sgmllib.SGMLParser):
    def __init__(self, set):
        sgmllib.SGMLParser.__init__(self)
	self.words = None
        self.mystack = []
	self.string = None
	self.convref = d = {"amp":"&", "lt":"<", "gt":">", "quot":'"', "squot":"'"}
    def close(self):
        self.convert()
        sgmllib.SGMLParser.close(self)

    def handle_starttag(self, tag, method, attrs):
        thing = method(attrs)
        self.mystack.append(thing)

    def handle_endtag(self, tag, method):
        thing = self.mystack.pop()
        method(thing)

    def unknown_starttag(self, tag, attrs):
        thing = self.start(tag, attrs)
        self.mystack.append(thing)

    def unknown_endtag(self, tag):
        thing = self.mystack.pop()
        self.end(tag, thing)

    def start(self, tag, attrs):
        self.convert()
        if self.words is not None:
            return (tag, attrs, self.pos, None)
        else:
            return None

    def convert(self):
        if self.words is not None and self.string is not None:
            words = self.string.split()
            self.pos += len(words)
	    self.words.extend(words)
	    self.string = None
	
    def end(self, tag, thing):
        self.convert()
        if self.words is not None:
            (tag, attrs, i, j) = thing
            self.meta.append((tag, attrs, i, self.pos))

    def handle_data(self, s):
        if self.words is not None:
	    if (self.string is None):
	       self.string = s
	    else:
	       self.string += s

    def handle_entityref(self, ref):
	# s=self.convert_entityref(ref)  # if python 2.5
	s=self.convref[ref]
        if self.words is not None:
	    if (self.string is None):
	       self.string = s
	    else:
	       self.string += s

