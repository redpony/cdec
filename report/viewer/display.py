#!/usr/bin/env python

import sys, os, gzip, re
import render, alignment

import reportlab.rl_config
reportlab.rl_config.warnOnMissingFontGlyphs = 0 
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfbase import pdfmetrics

pdfmetrics.registerFont(TTFont('PMingLiU', 'PMingLiU.ttf'))

doc = render.Document(sys.argv[1])

for line in sys.stdin:
    src, tgt, align = line.split(' ||| ')
    src = src.split()
    tgt = tgt.split()
    align = alignment.parse_pharaoh_align(align)
    doc.append(render.Alignment(src, tgt, align, 'PMingLiU', 'Helvetica', 8, 0.4))

doc.render()
