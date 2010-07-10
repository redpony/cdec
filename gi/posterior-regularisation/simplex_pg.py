#
# Following Leunberger and Ye, Linear and Nonlinear Progamming, 3rd ed. p367
# "The gradient projection method"
# applied to an equality constraint for a simplex.
#
#   min f(x)
#   s.t. x >= 0, sum_i x = d
#
# FIXME: enforce the positivity constraint - a limit on the line search?
#

from numpy import *
from scipy import *
from linesearch import line_search
# local copy of scipy's Amijo line_search - wasn't enforcing alpha max correctly
import sys

dims = 4

def f(x):
    fv = x[0]*x[0] + x[1]*x[1] + x[2]*x[2] + x[3]*x[3] - 2*x[0] - 3*x[3]
    # print 'evaluating f at', x, 'value', fv
    return fv

def g(x):
    return array([2*x[0] - 2, 2*x[1], 2*x[2], 2*x[3]-3])

def pg(x):
    gv = g(x)
    return gv - sum(gv) / dims

x = ones(dims) / dims
old_fval = None

while True:
    fv = f(x)
    gv = g(x)
    dv = pg(x)

    print 'x', x, 'f', fv, 'g', gv, 'd', dv

    if old_fval == None:
        old_fval = fv + 0.1

    # solve for maximum step size i.e. when positivity constraints kick in
    # x - alpha d = 0   => alpha = x/d
    amax = max(x/dv)
    if amax < 1e-8: break

    stuff = line_search(f, pg, x, -dv, dv, fv, old_fval, amax=amax)
    alpha = stuff[0] # Nb. can avoid next evaluation of f,g,d using 'stuff'
    if alpha < 1e-8: break
    x -= alpha * dv

    old_fval = fv
