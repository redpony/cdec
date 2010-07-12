#!/usr/bin/env python

#
# Takes spans input along with treebank and spits out CG style categories for each span.
#   spans = output from CDEC's extools/extractor with --base_phrase_spans option
#   treebank = PTB format, one tree per line
# 
# Output is in CDEC labelled-span format
#

import sys, itertools, tree

tinfile = open(sys.argv[1])
einfile = open(sys.argv[2])

def number_leaves(node, next=0):
    left, right = None, None
    for child in node.children:
        l, r = number_leaves(child, next)
        next = max(next, r+1)
        if left == None or l < left:
            left = l
        if right == None or r > right:
            right = r

    #print node, left, right, next
    if left == None or right == None:
        assert not node.children
        left = right = next

    node.left = left
    node.right = right

    return left, right

def ancestor(node, indices):
    #print node, node.left, node.right, indices
    # returns the deepest node covering all the indices
    if min(indices) >= node.left and max(indices) <= node.right:
        # try the children
        for child in node.children:
            x = ancestor(child, indices)
            if x: return x
        return node
    else:
        return None

def frontier(node, indices):
    #print 'frontier for node', node, 'indices', indices
    if node.left > max(indices) or node.right < min(indices):
        #print '\toutside'
        return [node]
    elif node.children:
        #print '\tcovering at least part'
        ns = []
        for child in node.children:
            n = frontier(child, indices)
            ns.extend(n)
        return ns
    else:
        return [node]

for tline, eline in itertools.izip(tinfile, einfile):
    if tline.strip() != '(())':
        if tline.startswith('( '):
            tline = tline[2:-1].strip()
        tr = tree.parse_PST(tline)
        number_leaves(tr)
    else:
        tr = None
    
    zh, en, spans = eline.strip().split(" ||| ")
    print '|||',
    for span in spans.split():
        i, j, x, y = map(int, span.split("-"))

        if tr:
            a = ancestor(tr, range(x,y))
            fs = frontier(a, range(x,y))

            #print x, y
            #print 'ancestor', a
            #print 'frontier', fs

            cat = a.data.tag
            for f in fs:
                if f.right < x:
                    cat += '\\' + f.data.tag
                else:
                    break
            for f in reversed(fs):
                if f.left >= y:
                    cat += '/' + f.data.tag
                else:
                    break
        else:
            cat = 'FAIL'
            
        print '%d-%d:%s' % (x, y, cat),
    print
