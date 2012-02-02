import sys

level = 1
file = sys.stderr

def writeln(s="", l=0):
    if level >= l:
	file.write("%s\n" % s)
	file.flush()

def write(s, l=0):
    if level >= l:
	file.write(s)
	file.flush()


    
    
