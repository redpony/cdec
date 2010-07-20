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

def project_heads(node):
    #print 'project_heads', node
    is_head = node.data.tag.endswith('-HEAD')
    if node.children:
        found = 0
        for child in node.children:
            x = project_heads(child)
            if x:
                node.data.tag = x
                found += 1
        assert found == 1
    elif is_head:
        node.data.tag = node.data.tag[:-len('-HEAD')]

    if is_head:
        return node.data.tag
    else:
        return None

for tline, eline in itertools.izip(tinfile, einfile):
    if tline.strip() != '(())':
        if tline.startswith('( '):
            tline = tline[2:-1].strip()
        tr = tree.parse_PST(tline)
	if tr != None:
		number_leaves(tr)
		#project_heads(tr) # assumes Bikel-style head annotation for the input trees
    else:
        tr = None
    
    parts = eline.strip().split(" ||| ")
    zh, en = parts[:2]
    spans = parts[-1]
    print '|||',
    for span in spans.split():
        sps = span.split(":")
        i, j, x, y = map(int, sps[0].split("-"))

        if tr:
            a = ancestor(tr, range(x,y))
	    try:
		fs = frontier(a, range(x,y))
	    except:
		print >>sys.stderr, "problem with line", tline.strip(), "--", eline.strip()
		raise

            #print x, y
            #print 'ancestor', a
            #print 'frontier', fs

            cat = a.data.tag
            for f in fs:
                if f.right < x:
                    cat += '\\' + f.data.tag
                else:
                    break
            fs.reverse()
            for f in fs:
                if f.left >= y:
                    cat += '/' + f.data.tag
                else:
                    break
        else:
            cat = 'FAIL'
            
        print '%d-%d:%s' % (x, y, cat),
    print
