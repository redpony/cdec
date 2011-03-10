AddOption('--prefix', dest='prefix', type='string', nargs=1, action='store', metavar='DIR', 
		      help='installation prefix')
AddOption('--with-boost', dest='boost', type='string', nargs=1, action='store', metavar='DIR',
                  help='boost installation directory (if in a non-standard location)')
AddOption('--with-glc', dest='glc', type='string', nargs=1, action='store', metavar='DIR',
                  help='path to Global Lexical Coherence package (optional)')
AddOption('--efence', dest='efence', action='store_true',
                  help='use electric fence for debugging memory corruptions')

platform = ARGUMENTS.get('OS', Platform())
include = Split('decoder utils klm mteval .')
env = Environment(PREFIX=GetOption('prefix'),
                      PLATFORM = platform,
#                      BINDIR = bin,
#                      INCDIR = include,
#                      LIBDIR = lib,
                      CPPPATH = include,
                      LIBPATH = [],
                      LIBS = Split('boost_program_options boost_serialization boost_thread z'),
		      CCFLAGS=Split('-g -O3'))

boost = GetOption('boost')
if boost:
   print 'Using Boost at {0}'.format(boost)
   env.Append(CPPPATH=boost+'/include',
	      LIBPATH=boost+'/lib')

if GetOption('efence'):
   env.Append(LIBS=Split('efence Segfault'))

srcs = []

# TODO: Get rid of config.h

glc = GetOption('glc')
if glc:
   print 'Using Global Lexical Coherence package at {0}'.format(glc)
   env.Append(CCFLAGS='-DHAVE_GLC',
	      CPPPATH=[glc, glc+'/cdec'])
   srcs.append(glc+'/string_util.cc')
   srcs.append(glc+'/feature-factory.cc')
   srcs.append(glc+'/cdec/ff_glc.cc')

for pattern in ['decoder/*.cc', 'decoder/*.c', 'klm/*/*.cc', 'utils/*.cc', 'mteval/*.cc']:
    srcs.extend([ file for file in Glob(pattern)
    		       if not 'test' in str(file)
		       	  and 'build_binary.cc' not in str(file)
			  and 'ngram_query.cc' not in str(file)
			  and 'mbr_kbest.cc' not in str(file)
			  and 'sri.cc' not in str(file)
			  and 'fast_score.cc' not in str(file)
		])

env.Program(target='decoder/cdec', source=srcs)
