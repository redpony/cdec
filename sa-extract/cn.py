# cn.py
# Chris Dyer <redpony@umd.edu>
# Copyright (c) 2006 University of Maryland.

# vim:tabstop=4:autoindent:expandtab

import sys
import math
import sym
import log
import sgml

epsilon = sym.fromstring('*EPS*');

class CNStats(object):
    def __init__(self):
      self.read = 0
      self.colls = 0
      self.words = 0

    def collect(self, cn):
      self.read += 1
      self.colls += cn.get_length()
      for col in cn.columns:
        self.words += len(col)

    def __str__(self):
      return "confusion net statistics:\n succ. read: %d\n columns:    %d\n words:      %d\n avg. words/column:\t%f\n avg. cols/sent:\t%f\n\n" % (self.read, self.colls, self.words, float(self.words)/float(self.colls), float(self.colls)/float(self.read))

class ConfusionNet(object):
    def __init__(self, sent):
        object.__init__(self)
        if (len(sent.words) == 0):
            self.columns = ()
            return # empty line, it happens
        line = sent.words[0]
        if (line.startswith("(((")):
            if (len(sent.words) > 1):
                log.write("Bad sentence: %s\n" % (line))
            assert(len(sent.words) == 1) # make sure there are no spaces in your confusion nets!
            line =  "((('<s>',1.0,1),),"+line[1:len(line)-1]+"(('</s>',1.0,1),))"
            cols = eval(line)
            res = []
            for col in cols:
               x = []
               for alt in col:
                   costs = alt[1]
                   if (type(costs) != type((1,2))):
                       costs=(float(costs),)
                   j=[]
                   for c in costs:
                       j.append(float(c))
                   cost = tuple(j)
                   spanlen = 1
                   if (len(alt) == 3):
                       spanlen = alt[2]
                   x.append((sym.fromstring(alt[0],terminal=True), None, spanlen))
               res.append(tuple(x))
            self.columns = tuple(res)
        else:  # convert a string of input into a CN
            res = [];
            res.append(((sym.fromstring('<s>',terminal=True), None, 1), ))
            for word in sent.words:
               res.append(((sym.fromstring(word,terminal=True), None, 1), ));  # (alt=word, cost=0.0)
            res.append(((sym.fromstring('</s>',terminal=True), None, 1), ))
            self.columns = tuple(res)

    def is_epsilon(self, position):
        x = self.columns[position[0]][position[1]][0]
        return x == epsilon

    def compute_epsilon_run_length(self, cn_path):
        if (len(cn_path) == 0):
            return 0
        x = len(cn_path) - 1
        res = 0
        ''' -1 denotes a non-terminal '''
        while (x >= 0 and cn_path[x][0] >= 0 and self.is_epsilon(cn_path[x])):
            res += 1
            x -= 1
        return res

    def compute_cn_cost(self, cn_path):
       c = None
       for (col, row) in cn_path:
           if (col >= 0):
               if c is None:
                   c = self.columns[col][row][1].clone()
               else:
                   c += self.columns[col][row][1]
       return c                                        

    def get_column(self, col):
        return self.columns[col]

    def get_length(self):
        return len(self.columns)

    def __str__(self):
        r = "conf net: %d\n" % (len(self.columns),)
        i = 0
        for col in self.columns:
            r += "%d -- " % i
            i += 1
            for alternative in col:
                r += "(%s, %s, %s) " % (sym.tostring(alternative[0]), alternative[1], alternative[2])
            r += "\n"
        return r
    
    def listdown(_columns, col = 0):
        # output all the possible sentences out of the self lattice
        # will be used by the "dumb" adaptation of lattice decoding with suffix array
        result = []
        for entry in _columns[col]:
            if col+entry[2]+1<=len(_columns) :
                for suffix in self.listdown(_columns,col+entry[2]):
                    result.append(entry[0]+' '+suffix)
                    #result.append(entry[0]+' '+suffix)
            else:
                result.append(entry[0])
                #result.append(entry[0])
        return result
    
    def next(self,_columns,curr_idx, min_dist=1):
        # can be used only when prev_id is defined
        result = []
        #print "curr_idx=%i\n" % curr_idx
        if curr_idx+min_dist >= len(_columns): 
            return result
        for alt_idx in xrange(len(_columns[curr_idx])):
            alt = _columns[curr_idx][alt_idx]
            #print "checking %i alternative : " % alt_idx
            #print "%s %f %i\n" % (alt[0],alt[1],alt[2])
            #print alt
            if alt[2]<min_dist:
                #print "recursive next(%i, %i, %i)\n" % (curr_idx,alt_idx,min_dist-alt[2])
                result.extend(self.next(_columns,curr_idx+alt[2],min_dist-alt[2]))
            elif curr_idx+alt[2]<len(_columns):
                #print "adding because the skip %i doesn't go beyong the length\n" % alt[2]
                result.append(curr_idx+alt[2])
        return set(result)
                
    


#file = open(sys.argv[1], "rb")
#sent = sgml.process_sgml_line(file.read())
#print sent
#cn = ConfusionNet(sent)
#print cn
#results = cn.listdown()
#for result in results:
#    print sym.tostring(result)
#print cn.next(0);
#print cn.next(1);
#print cn.next(2);
#print cn.next(3);
#print cn
#cn = ConfusionNet()
#k = 0
#while (cn.read(file)):
#  print cn
  
#print cn.stats
