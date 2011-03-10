AddOption('--prefix',
                  dest='prefix',
                  type='string',
                  nargs=1,
                  action='store',
                  metavar='DIR',
                  help='installation prefix')

AddOption('--with-boost',
                  dest='boost',
                  type='string',
                  nargs=1,
                  action='store',
                  metavar='DIR',
                  help='boost installation directory (if in a non-standard location)')

platform = ARGUMENTS.get('OS', Platform())

srcs = []
for pattern in ['decoder/*.cc', 'decoder/*.c', 'klm/*/*.cc', 'utils/*.cc', 'mteval/*.cc']:
    srcs.extend([ file for file in Glob(pattern)
    		       if not 'test' in str(file)
		       	  and 'build_binary.cc' not in str(file)
			  and 'ngram_query.cc' not in str(file)
			  and 'mbr_kbest.cc' not in str(file)
			  and 'sri.cc' not in str(file)
			  and 'fast_score.cc' not in str(file)
		])

include = Split('decoder utils klm mteval .')
libPaths = []

boost = GetOption('boost')
if boost:
   include.append(boost+'/include')
   libPaths.append(boost+'/lib')	

glcDir = None
glcDir = '../GlobalLexicalCoherence'
if glcDir:
   include.append(glcDir)

env = Environment(PREFIX=GetOption('prefix'),
                      PLATFORM = platform,
#                      BINDIR = bin,
                      INCDIR = include,
#                      LIBDIR = lib,
                      CPPPATH = [include, '.'],
                      LIBPATH = libPaths,
                      LIBS = Split('boost_program_options boost_serialization boost_thread z'))
env.Program(target='decoder/cdec', source=srcs)
