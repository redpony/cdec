#!/usr/bin/env python

import lcp
import sys
import optparse
import csuf

'''Run various computations using the LCP array'''
def main():

  optparser = optparse.OptionParser()
#    optparser.add_option("-c", "--config", dest="config", help="configuration module")
  optparser.add_option("-s", "--sa-check", dest="sa_check", default=False, action="store_true")
  optparser.add_option("-l", "--lcp-check", dest="lcp_check", default=False, action="store_true")
  optparser.add_option("-t", "--stats", dest="stats", default=0, type="int", action="store")
  optparser.add_option("-u", "--unigram", dest="uni_check", default=False, action="store_true")
  optparser.add_option("-r", "--report-long-lcps", dest="long_lcp", type="int", default=0, action="store")
  (opts,args) = optparser.parse_args()

  if len(args) < 1:
    print >>sys.stderr, "Usage: lcp.py [opts] <sa file>"
    sys.exit(1)

  safile = args[0]
  sa = csuf.SuffixArray(safile, from_binary=True)

#  if opts.sa_check:
#    check_sufarray(sa)

  l = lcp.LCP(sa)

  if opts.lcp_check:
    print >>sys.stderr, "Checking LCP Array..."
    l.check()
    print >>sys.stderr, "Check finished"

  if opts.stats > 0:
    l.compute_stats(opts.stats)

#  if opts.uni_check:
#    if lcp is None:
#      lcp = LCP(sa)
#    unigram_stats(sa, lcp)
#
#  if opts.long_lcp:
#    if lcp is None:
#      lcp = LCP(sa, opts.long_lcp)

if __name__ == "__main__":
  sys.exit(main())
  

