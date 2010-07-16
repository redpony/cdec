#!/usr/bin/env python

import sys, math, itertools

ginfile = open(sys.argv[1])
pinfile = open(sys.argv[2])
if len(sys.argv) > 3:
    slash_threshold = int(sys.argv[3])
    #print >>sys.stderr, 'slash threshold', slash_threshold
else:
    slash_threshold = 99999

# evaluating: H(G | P) = sum_{g,p} p(g,p) log { p(p) / p(g,p) }
#                      = sum_{g,p} c(g,p)/N { log c(p) - log N - log c(g,p) + log N }
#                      = 1/N sum_{g,p} c(g,p) { log c(p) - log c(g,p) }
# where G = gold, P = predicted, N = number of events

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

        if gtag.count('/') + gtag.count('\\') <= slash_threshold:
            joint_frequencies.setdefault((gtag, ptag), 0)
            joint_frequencies[gtag,ptag] += 1

            predict_frequencies.setdefault(ptag, 0)
            predict_frequencies[ptag] += 1

            gold_frequencies.setdefault(gtag, 0)
            gold_frequencies[gtag] += 1

            N += 1

hg2p = 0
hp2g = 0
for (gtag, ptag), cgp in joint_frequencies.items():
    hp2g += cgp * (math.log(predict_frequencies[ptag], 2) - math.log(cgp, 2))
    hg2p += cgp * (math.log(gold_frequencies[gtag], 2) - math.log(cgp, 2))
hg2p /= N
hp2g /= N

hg = 0
for gtag, c in gold_frequencies.items():
    hg -= c * (math.log(c, 2) - math.log(N, 2))
hg /= N

print 'H(P|G)', hg2p, 'H(G|P)', hp2g, 'VI', hg2p + hp2g, 'H(G)', hg

# find top tags
gtags = gold_frequencies.items()
gtags.sort(lambda x,y: x[1]-y[1])
gtags.reverse()
#gtags = gtags[:50]

print '%7s %7s' % ('pred', 'cnt'),
for gtag, gcount in gtags: print '%7s' % gtag,
print
print '=' * 80

preds = predict_frequencies.items()
preds.sort(lambda x,y: x[1]-y[1])
preds.reverse()
for ptag, pcount in preds:
    print '%7s %7d' % (ptag, pcount),
    for gtag, gcount in gtags:
	print '%7d' % joint_frequencies.get((gtag, ptag), 0),
    print

print '%7s %7d' % ('total', N),
for gtag, gcount in gtags: print '%7d' % gcount,
print

if len(sys.argv) > 4:
	# needs Python Image Library (PIL)
	import Image, ImageDraw

	offset=10

	image = Image.new("RGB", (len(preds), len(gtags)), (255, 255, 255))
	#hsl(hue, saturation%, lightness%)

	# resort preds to get a better diagonal
	ptags = []
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
	image.save(sys.argv[4])
