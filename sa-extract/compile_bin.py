#!/usr/bin/env python

'''This program compiles/decompiles binary data objects used
by the decoder'''

import sys
import cdat
import calignment
import csuf
import clex
import precomputation
#import parse
import monitor
import optparse

def main(argv=None):
	'''Call this from the command-line to create a 
	pre-computed binary data array for later use'''
	if argv is None:
		argv = sys.argv

	parser = optparse.OptionParser(usage="Usage: %prog [-s|-d|-a|-p] <input file> <output file>"+
								"\n\nNote: -d,-s,-a, and -p are mutually exclusive")
	parser.add_option("-d", "--data-array", 
					action="store_true", default=False,
					dest="da", help="Compile file into data array (default)")
	parser.add_option("-s", "--suffix-array", 
					action="store_true", default=False,
					dest="sa", help="Compile file into suffix array")
	parser.add_option("-a", "--alignment", 
					action="store_true", default=False,
					dest="a", help="Compile file into alignment")
	parser.add_option("-l", "--lexical", 
					action="store_true", default=False,
					dest="l", help="Compile file into lex file")
	parser.add_option("-x", "--compute_lexical", action="store", nargs=2,
					dest="lex_args", help="Compute lex file from data",
					metavar="<f file> <e file>")
	parser.add_option("-p", "--parse", 
					action="store_true", default=False,
					dest="p", help="Compile file into parse")
	parser.add_option("-b", "--binary-infile", 
					action="store_true", default=False,
					dest="bin", help="Input file is binary (default: text)")
	parser.add_option("-t", "--text-outfile", 
					action="store_true", default=False,
					dest="text", help="Output file is text (default: binary)")
	parser.add_option("-e", "--enhanced-outfile", 
					action="store_true", default=False,
					dest="enhanced", help="Output file is enhanced text (default: binary)")
	parser.add_option("-r", action="store", nargs=7,
					dest="precomp_args", help="Precompute collocations (Hiero only)", 
					metavar="max-len=<INT> max-nt=<INT> max-size=<INT> min-gap=<INT> rank1=<INT> rank2=<INT> sa=<FILE>")
	(options, args) = parser.parse_args()

	filetype_opts =  [options.da, options.sa, options.a, options.p]

	if (len(filter(lambda x: x, filetype_opts))) > 1 or len(args) != 2:
		parser.print_help()
		sys.exit(1)

	(infilename, outfilename) = args
	if options.bin:
		bin = " binary"
	else:
		bin = ""

	start_time = monitor.cpu()
	if options.precomp_args:
		if options.bin:
			obj = precomputation.Precomputation(infilename, from_binary=True)
		else:
			keys = set(["max-len", "max-nt", "max-size", "min-gap", "rank1", "rank2", "sa"])
			precomp_opts = {} 
			sys.stderr.write("Precomputing statistics for list %s\n" % infilename)
			for pair in options.precomp_args:
				(key, val) = pair.split("=")
				if key in keys:
					keys.remove(key)
					if key != "sa":
						val = int(val)
					precomp_opts[key] = val
				else:
					sys.stderr.write("Unknown keyword arg %s for -r (must be one of: max-len, max-nt, max-size, min-gap, rank1, rank2)\n" % key)
					return 1
			sa = csuf.SuffixArray(precomp_opts["sa"], True)
			obj = precomputation.Precomputation(infilename, sa, 
				precompute_rank=precomp_opts["rank1"], 
				precompute_secondary_rank=precomp_opts["rank2"], 
				max_length=precomp_opts["max-len"], 
				max_nonterminals=precomp_opts["max-nt"], 
				train_max_initial_size=precomp_opts["max-size"], 
				train_min_gap_size=precomp_opts["min-gap"])
	elif options.sa:
		sys.stderr.write("Reading %s as%s suffix array...\n" % (infilename, bin))
		obj = csuf.SuffixArray(infilename, options.bin)
	elif options.a:
		sys.stderr.write("Reading %s as%s alignment array...\n" % (infilename, bin))
		obj = calignment.Alignment(infilename, options.bin)
	elif options.p:
		sys.stderr.write("Reading %s as%s parse array...\n" % (infilename, bin))
		obj = parse.ParseArray(infilename, options.bin)
	elif options.l:
		sys.stderr.write("Reading %s as%s lex array...\n" % (infilename, bin))
		obj = clex.CLex(infilename, options.bin)
	elif options.lex_args:
		ffile = options.lex_args[0]
		efile = options.lex_args[1]
		sys.stderr.write("Computing lex array from:\n A=%s\n F=%s\n E=%s\n" % (infilename, ffile, efile))
		fsarray = csuf.SuffixArray(ffile, True)
		earray = cdat.DataArray(efile, True)
		aarray = calignment.Alignment(infilename, True)
		obj = clex.CLex(aarray, from_data=True, earray=earray, fsarray=fsarray)
	else:
		sys.stderr.write("Reading %s as%s data array...\n" % (infilename, bin))
		obj = cdat.DataArray(infilename, options.bin)

	sys.stderr.write("  Total time for read: %f\n" % (monitor.cpu() - start_time))
	start_time = monitor.cpu()
	if options.text:
		sys.stderr.write("Writing text file %s...\n" % outfilename)
		obj.write_text(outfilename)
	elif options.enhanced:
		sys.stderr.write("Writing enhanced text file %s...\n" % outfilename)
		obj.write_enhanced(outfilename)
	else:
		sys.stderr.write("Writing binary file %s...\n" % outfilename)
		obj.write_binary(outfilename)
	sys.stderr.write("Finished.\n")
	sys.stderr.write("  Total time for write: %f\n" % (monitor.cpu() - start_time))

	mem_use = float(monitor.memory())
	metric = "B"
	if mem_use / 1000 > 1:
		mem_use /= 1000
		metric = "KB"
	if mem_use / 1000 > 1:
		mem_use /= 1000
		metric = "MB"
	if mem_use / 1000 > 1:
		mem_use /= 1000
		metric = "GB"
	sys.stderr.write("  Memory usage: %.1f%s\n" % (mem_use, metric))



if __name__ == "__main__":
	sys.exit(main(sys.argv))
