#!/usr/bin/env python

import sys, math, itertools, getopt

def usage():
    print >>sys.stderr, 'Usage:', sys.argv[0], '[-s slash_threshold] [-p output] [-m] input-1 input-2'
    sys.exit(0)

optlist, args = getopt.getopt(sys.argv[1:], 'hs:mp:')
slash_threshold = None
output_fname = None
show_matrix = False
for opt, arg in optlist:
    if opt == '-s':
        slash_threshold = int(arg)
    elif opt == '-p':
        output_fname = arg
    elif opt == '-m':
        show_matrix = True
    else:
        usage()
if len(args) != 2 or (not show_matrix and not output_fname):
    usage()

ginfile = open(args[0])
pinfile = open(args[1])

if output_fname:
    try:
        import Image, ImageDraw
    except ImportError:
        print >>sys.stderr, "Error: Python Image Library not available. Did you forget to set your PYTHONPATH environment variable?" 
        sys.exit(1)

N = 0
gold_frequencies = {}
predict_frequencies = {}
joint_frequencies = {}

for gline, pline in itertools.izip(ginfile, pinfile):
    gparts = gline.split('||| ')[1].split()
    pparts = pline.split('||| ')[1].split()
    assert len(gparts) == len(pparts)

    for gpart, ppart in zip(gparts, pparts):
        gtag = gpart.split(':',1)[1]
        ptag = ppart.split(':',1)[1]

        if slash_threshold == None or gtag.count('/') + gtag.count('\\') <= slash_threshold:
            joint_frequencies.setdefault((gtag, ptag), 0)
            joint_frequencies[gtag,ptag] += 1

            predict_frequencies.setdefault(ptag, 0)
            predict_frequencies[ptag] += 1

            gold_frequencies.setdefault(gtag, 0)
            gold_frequencies[gtag] += 1

            N += 1

# find top tags
gtags = gold_frequencies.items()
gtags.sort(lambda x,y: x[1]-y[1])
gtags.reverse()
#gtags = gtags[:50]

preds = predict_frequencies.items()
preds.sort(lambda x,y: x[1]-y[1])
preds.reverse()

if show_matrix:
    print '%7s %7s' % ('pred', 'cnt'),
    for gtag, gcount in gtags: print '%7s' % gtag,
    print
    print '=' * 80

    for ptag, pcount in preds:
        print '%7s %7d' % (ptag, pcount),
        for gtag, gcount in gtags:
            print '%7d' % joint_frequencies.get((gtag, ptag), 0),
        print

    print '%7s %7d' % ('total', N),
    for gtag, gcount in gtags: print '%7d' % gcount,
    print

if output_fname:
    offset=10

    image = Image.new("RGB", (len(preds), len(gtags)), (255, 255, 255))
    #hsl(hue, saturation%, lightness%)

    # re-sort preds to get a better diagonal
    ptags=[]
    if True:
        ptags = map(lambda (p,c): p, preds)
    else:
        remaining = set(predict_frequencies.keys())
        for y, (gtag, gcount) in enumerate(gtags):
            best = (None, 0)
            for ptag in remaining:
                #pcount = predict_frequencies[ptag]
                p = joint_frequencies.get((gtag, ptag), 0)# / float(pcount)
                if p > best[1]: best = (ptag, p)
            ptags.append(ptag)
            remaining.remove(ptag)
            if not remaining: break

    print 'Predicted tag ordering:', ' '.join(ptags)
    print 'Gold tag ordering:', ' '.join(map(lambda (t,c): t, gtags))
    
    draw = ImageDraw.Draw(image)
    for x, ptag in enumerate(ptags):
        pcount = predict_frequencies[ptag]
        minval = math.log(offset)
        maxval = math.log(pcount + offset)
        for y, (gtag, gcount) in enumerate(gtags):
            f = math.log(offset + joint_frequencies.get((gtag, ptag), 0))
            z = int(240. * (maxval - f) / float(maxval - minval))
            #print x, y, z, f, maxval
            draw.point([(x,y)], fill='hsl(%d, 100%%, 50%%)' % z)
    del draw
    image.save(output_fname)
