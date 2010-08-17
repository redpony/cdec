class Alignment:
    SURE, POSSIBLE = 'S', 'P'
    
    def __init__(self, swords, twords, align):
        self.swords = swords
        self.twords = twords
        self.align = align

    def reverse(self):
        als = {}
        for (frm, to), conf in self.align.items():
            als[to, frm] = conf
        return Alignment(self.twords, self.swords, als)

    def merge(self, other):
        assert self.swords == other.swords
        assert self.twords == other.twords

        als = {}
        for frm, to in self.align.keys():
            als[frm, to] = Alignment.POSSIBLE

        for frm, to in other.align.keys():
            if (frm, to) in als:
                als[frm, to] = Alignment.SURE
            else:
                als[frm, to] = Alignment.POSSIBLE

        return Alignment(self.swords, self.twords, als)

    def __repr__(self):
        return 'Alignment(swords=%s, twords=%s, align=%s)' % (self.swords, self.twords, self.align)

def read_pharaoh_text(infile):
    return infile.readline().strip().split()

def parse_pharaoh_align(text):
    als = {}
    for part in text.strip().split():
        frm, to = map(int, part.split('-'))
        als[frm, to] = Alignment.SURE
    return als

def read_pharaoh_align(infile):
    als = {}
    for part in infile.readline().strip().split():
        frm, to = map(int, part.split('-'))
        als[frm, to] = Alignment.SURE
    return als

def read_pharaoh_alignment(swfile, twfile, afile):
    sw = read_pharaoh_text(swfile)
    tw = read_pharaoh_text(twfile)
    als = read_pharaoh_align(afile)
    return Alignment(sw, tw, als)
    
def read_giza_alignment(infile):
    infile.readline() # ignore
    swords = infile.readline().strip().split()
    twords = []
    als = {}
    state = 0
    for token in infile.readline().strip().split():
        if state == 0:
            if token != 'NULL':
                if token != '({':
                    twords.append(token)
                else:
                    state = 1
        elif state == 1:
            if token != '})':
                if twords:
                    als[int(token)-1, len(twords)-1] = Alignment.SURE
            else:
                state = 0
    return Alignment(swords, twords, als)

def read_naacl_aligns(infile):
    aligns = []
    last = None
    for line in infile:
        index, frm, to, conf = line.rstrip().split()
        if int(index) != last:
            aligns.append({})
        aligns[-1][int(frm)-1, int(to)-1] = conf
        last = int(index)
    return aligns

#
# This phrase-extraction function largely mimics Pharaoh's phrase-extract
# code. It also supports the option to not advance over NULL alignments.
#

def xextract_phrases(alignment, maxPhraseLength=None, advance=True):
    T = len(alignment.twords)
    S = len(alignment.swords)
    if not maxPhraseLength:
        maxPhraseLength = max(T, S)

    alignedCountS = [0 for s in alignment.swords]
    alignedToT = [[] for t in alignment.twords]
    alignedToS = [[] for s in alignment.swords]
    for (s, t), conf in alignment.align.items():
        if conf == Alignment.SURE:
            alignedCountS[s] += 1
            alignedToT[t].append(s)
            alignedToS[s].append(t)

    # check alignments for english phrase startT...endT
    for st in range(T):
        for et in range(st, min(T, st + maxPhraseLength)):
            minS = 9999
            maxS = -1
            usedS = alignedCountS[:]
            for ti in range(st, et+1):
                for si in alignedToT[ti]:
                    #print 'point (%d, %d)' % (si, ti)
                    if si<minS: minS = si
                    if si>maxS: maxS = si
                    usedS[si] -= 1
                    
            #print 's projected (%d-%d, %d, %d)' % (minS, maxS, st, et)
            if (maxS >= 0 and  # aligned to any foreign words at all
                    maxS-minS < maxPhraseLength): # foreign phrase within limits
                # check if foreign words are aligned to out of bound english words
                out_of_bounds = False
                for si in range(minS, maxS):
                    if usedS[si] > 0:
                        #print 'out of bounds:', si
                        out_of_bounds = True
                        break

                # Pharoah doesn't use this check, but I think it's required
                if not out_of_bounds:
                    for s in range(minS, maxS+1):
                        for t in alignedToS[s]:
                            if not (st <= t <= et):
                                #print 'out of bounds2:', t,s
                                out_of_bounds = True
                                break

                #print 'doing it for (%d-%d, %d, %d)' % (minS, maxS, st, et)
                if not out_of_bounds:
                    if advance:
                        #print 'attempting to advance'
                        # start point of foreign phrase may advance over unaligned
                        ss = minS
                        while (ss>=0 and
                                 ss>maxS-maxPhraseLength and # within length limit
                                 (ss==minS or alignedCountS[ss]==0)): # unaligned
                            # end point of foreign phrase may advance over unaligned
                            es = maxS
                            while (es<S and 
                                     es<ss+maxPhraseLength and # within length limit
                                     (es==maxS or alignedCountS[es]==0)): #unaligned
                                yield (ss, es, st, et)
                                es += 1
                            ss -= 1
                    else:
                        ss, es = minS, maxS
                        yield (minS, maxS, st, et)

