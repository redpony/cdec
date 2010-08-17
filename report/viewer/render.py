from reportlab.pdfgen import canvas
from reportlab.lib.colors import black, gray, white, magenta, Color
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet
from reportlab.lib.units import cm, inch
from reportlab.platypus import SimpleDocTemplate, Spacer, Paragraph
from reportlab.platypus.flowables import Flowable
import re

class Alignment(Flowable):
    def __init__(self, x_words, y_words, alignment, x_font, y_font, ptsize, unit, scale=True, colours=None):
        self._x_words = x_words
        self._y_words = y_words
        self._alignment = alignment
        self._unit = unit*cm
        self._x_font = x_font
        self._y_font = y_font
        self._ptsize = ptsize
        self._scale = 1
        self._do_scale = scale
        self._colours = colours
        if not colours:
            self._colours = {'S':black, 'P':gray, 'N':magenta}

    def wrap(self, rw, rh):
        xws = [self.canv.stringWidth(w, self._x_font, self._ptsize)
               for w in self._x_words]
        yws = [self.canv.stringWidth(w, self._y_font, self._ptsize)
               for w in self._y_words]
        width = (len(self._x_words) + 0.22)* self._unit + max(yws)
        height = (len(self._y_words) + 0.22)* self._unit + max(xws)
        
        if self._do_scale:
            self._scale = min(rw / width, 1.5)
            width *= self._scale
            height *= self._scale

        return (width, height)

    def draw(self):
        c = self.canv
        print c.getAvailableFonts()

        X=len(self._x_words)
        Y=len(self._y_words)

        c.saveState()
        c.scale(self._scale, self._scale)

        for (x, y), conf in self._alignment.items():
            col = self._colours[conf]
            if isinstance(col, Color):
                c.setFillColor(col)
                c.rect((0.02 + x)*self._unit, (0.02+Y-y-1)*self._unit,
                       self._unit, self._unit, 0, 1)
            else:
                bl = (x*self._unit, (Y-y-1)*self._unit)
                tl = (x*self._unit, (Y-y)*self._unit)
                tr = ((x+1)*self._unit, (Y-y)*self._unit)
                br = ((x+1)*self._unit, (Y-y-1)*self._unit)

                p = c.beginPath()
                p.moveTo(*br)
                p.lineTo(*tr)
                p.lineTo(*tl)
                c.setFillColor(col[0])
                c.drawPath(p, fill=1)
                p = c.beginPath()
                p.moveTo(*br)
                p.lineTo(*bl)
                p.lineTo(*tl)
                c.setFillColor(col[1])
                c.drawPath(p, fill=1)

        c.setStrokeColor(black)
        c.grid(map(lambda x: (0.02+x)*self._unit, range(X+1)),
               map(lambda y: (0.02+y)*self._unit, range(Y+1)))

        c.setFont(self._x_font, self._ptsize)
        c.setFillColor(black)
        for x, word in enumerate(self._x_words):
            c.saveState()
            c.translate((x+0.52)*self._unit, (Y+0.22)*self._unit)
            c.rotate(60)
            c.drawString(0, 0, word)
            c.restoreState()

        c.setFont(self._y_font, self._ptsize)
        for y, word in enumerate(self._y_words):
            c.drawString((X+0.22)*self._unit, (Y-y+0.42-1)*self._unit, word)

        c.restoreState()

class Document:
    def __init__(self, filename):
        self._styles = getSampleStyleSheet()
        self._doc = SimpleDocTemplate(filename)
        self._story = []

    def append(self, flowable):
        self._story.append(flowable)
        self._story.append(Spacer(1, 1*cm))

    def render(self):
        self._doc.build(self._story[:-1])

class Canvas:
    def __init__(self, filename):
        self._filename = filename
        self._canvas = canvas.Canvas('.' + filename, A4)
        self._size = A4
        self._body = None

    def append(self, flowable):
        if self._body:
            print >>sys.stderr, 'WARNING: replacing existing flowable' 
        self._body = flowable

    def render(self):
        self._body.canv = self._canvas
        width, height = self._body.wrap(*self._size)
        width *= 1.02
        height *= 1.02

        self._canvas = canvas.Canvas(self._filename, (width, height))
        self._body.canv = self._canvas
        self._body.draw()
        self._canvas.save()
