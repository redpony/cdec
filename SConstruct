#!/usr/bin/python

# EXPERIMENTAL and HACKY version of cdec build in scons

# TODO: Persist these so that you don't have to specify flags every time
# http://www.scons.org/wiki/SavingVariablesToAFile
AddOption('--prefix', dest='prefix', type='string', nargs=1, action='store', metavar='DIR', 
		      help='installation prefix')
AddOption('--with-boost', dest='boost', type='string', nargs=1, action='store', metavar='DIR',
                  help='boost installation directory (if in a non-standard location)')
AddOption('--with-glc', dest='glc', type='string', nargs=1, action='store', metavar='DIR',
                  help='path to Global Lexical Coherence package (optional)')
AddOption('--with-mpi', dest='mpi', action='store_true',
                  help='build tools that use Message Passing Interface? (optional)')
AddOption('--efence', dest='efence', action='store_true',
                  help='use electric fence for debugging memory corruptions')

platform = ARGUMENTS.get('OS', Platform())
include = Split('decoder utils klm mteval training .')
env = Environment(PREFIX=GetOption('prefix'),
                      PLATFORM = platform,
#                      BINDIR = bin,
#                      INCDIR = include,
#                      LIBDIR = lib,
                      CPPPATH = include,
                      LIBPATH = [],
                      LIBS = Split('boost_program_options boost_serialization boost_thread z'),
		      CCFLAGS=Split('-g -O3 -DHAVE_SCONS'))


# Do some autoconf-like sanity checks (http://www.scons.org/wiki/SconsAutoconf)
conf = Configure(env)
print('Checking if the environment is sane...')
if not conf.CheckCXX():
    print('!! Your compiler and/or environment is not correctly configured.')
    Exit(1)
if not conf.CheckFunc('printf'):
    print('!! Your compiler and/or environment is not correctly configured.')
    Exit(1)
#env = conf.Finish()

boost = GetOption('boost')
if boost:
   print 'Using Boost at {0}'.format(boost)
   env.Append(CCFLAGS='-DHAVE_BOOST',
              CPPPATH=boost+'/include',
	      LIBPATH=boost+'/lib')

if not conf.CheckLib('boost_program_options'):
   print "Boost library 'boost_program_options' not found"
   Exit(1)
#if not conf.CheckHeader('boost/math/special_functions/digamma.hpp'):
#   print "Boost header 'digamma.hpp' not found"
#   Exit(1)

mpi = GetOption('mpi')
if mpi:
   if not conf.CheckHeader('mpi.h'):
      print "MPI header 'mpi.h' not found"
      Exit(1)   

if GetOption('efence'):
   env.Append(LIBS=Split('efence Segfault'))

print('Environment is sane.')
print

srcs = []

# TODO: Get rid of config.h

glc = GetOption('glc')
if glc:
   print 'Using Global Lexical Coherence package at {0}'.format(glc)
   env.Append(CCFLAGS='-DHAVE_GLC',
	      CPPPATH=[glc, glc+'/cdec'])
   srcs.append(glc+'/string_util.cc')
   srcs.append(glc+'/sys_util.cc')
   srcs.append(glc+'/debug.cc')
   srcs.append(glc+'/feature-factory.cc')
   srcs.append(glc+'/cdec/ff_glc.cc')

for pattern in ['decoder/*.cc', 'decoder/*.c', 'klm/*/*.cc', 'utils/*.cc', 'mteval/*.cc', 'vest/*.cc']:
    srcs.extend([ file for file in Glob(pattern)
    		       if not 'test' in str(file)
		       	  and 'build_binary.cc' not in str(file)
			  and 'ngram_query.cc' not in str(file)
			  and 'mbr_kbest.cc' not in str(file)
			  and 'sri.cc' not in str(file)
			  and 'fast_score.cc' not in str(file)
                          and 'cdec.cc' not in str(file)
                          and 'mr_' not in str(file)
                          and 'utils/ts.cc' != str(file)
		])

print 'Found {0} source files'.format(len(srcs))
def comb(cc, srcs):
   x = [cc]
   x.extend(srcs)
   return x

env.Program(target='decoder/cdec', source=comb('decoder/cdec.cc', srcs))
# TODO: The various decoder tests
# TODO: extools
env.Program(target='klm/lm/build_binary', source=comb('klm/lm/build_binary.cc', srcs))
# TODO: klm ngram_query and tests
env.Program(target='mteval/fast_score', source=comb('mteval/fast_score.cc', srcs))
env.Program(target='mteval/mbr_kbest', source=comb('mteval/mbr_kbest.cc', srcs))
#env.Program(target='mteval/scorer_test', source=comb('mteval/fast_score.cc', srcs))
# TODO: phrasinator

# TODO: Various training binaries
env.Program(target='training/model1', source=comb('training/model1.cc', srcs))
env.Program(target='training/augment_grammar', source=comb('training/augment_grammar.cc', srcs))
env.Program(target='training/grammar_convert', source=comb('training/grammar_convert.cc', srcs))
#env.Program(target='training/optimize_test', source=comb('training/optimize_test.cc', srcs))
env.Program(target='training/collapse_weights', source=comb('training/collapse_weights.cc', srcs))
#env.Program(target='training/lbfgs_test', source=comb('training/lbfgs_test.cc', srcs))
#env.Program(target='training/mr_optimize_reduce', source=comb('training/mr_optimize_reduce.cc', srcs))
env.Program(target='training/mr_em_map_adapter', source=comb('training/mr_em_map_adapter.cc', srcs))
env.Program(target='training/mr_reduce_to_weights', source=comb('training/mr_reduce_to_weights.cc', srcs))
env.Program(target='training/mr_em_adapted_reduce', source=comb('training/mr_em_adapted_reduce.cc', srcs))

env.Program(target='vest/sentserver', source=['vest/sentserver.c'], LINKFLAGS='-all-static')
env.Program(target='vest/sentclient', source=['vest/sentclient.c'], LINKFLAGS='-all-static')
env.Program(target='vest/mr_vest_generate_mapper_input', source=comb('vest/mr_vest_generate_mapper_input.cc', srcs))
env.Program(target='vest/mr_vest_map', source=comb('vest/mr_vest_map.cc', srcs))
env.Program(target='vest/mr_vest_reduce', source=comb('vest/mr_vest_reduce.cc', srcs))
#env.Program(target='vest/lo_test', source=comb('vest/lo_test.cc', srcs))
# TODO: util tests

if mpi:
   env.Program(target='training/mpi_online_optimize', source=comb('training/mpi_online_optimize.cc', srcs))
   env.Program(target='training/mpi_batch_optimize', source=comb('training/mpi_batch_optimize.cc', srcs))
   env.Program(target='training/compute_cllh', source=comb('training/compute_cllh.cc', srcs))
   env.Program(target='training/cllh_filter_grammar', source=comb('training/cllh_filter_grammar.cc', srcs))

