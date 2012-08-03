from distutils.core import setup
from distutils.extension import Extension
import sys
import os
import glob

INC = ['..', 'src/', '../decoder', '../utils', '../mteval']
LIB = ['../decoder', '../utils', '../mteval', '../training', '../klm/lm', '../klm/util']
LINK_ARGS = []

# Detect Boost
BOOST_ROOT = os.getenv('BOOST_ROOT')
if BOOST_ROOT:
    BOOST_INC = os.path.join(BOOST_ROOT, 'include')
    BOOST_LIB = os.path.join(BOOST_ROOT, 'lib')
    if not os.path.exists(BOOST_INC):
        sys.stderr.write('Error: could not find Boost headers in <%s>\n' % BOOST_INC)
        sys.exit(1)
    if not os.path.exists(BOOST_LIB):
        sys.stderr.write('Error: could not find Boost libraries in <%s>\n' % BOOST_LIB)
        sys.exit(1)
    INC.append(BOOST_INC)
    LIB.append(BOOST_LIB)
    LINK_ARGS += ['-Wl,-rpath', '-Wl,'+BOOST_LIB]
else:
    BOOST_LIB = '/usr/local/lib'

# Detect -mt
if glob.glob(os.path.join(BOOST_LIB, 'libboost_program_options-mt.*')):
    BOOST_PROGRAM_OPTIONS = 'boost_program_options-mt'
else:
    BOOST_PROGRAM_OPTIONS = 'boost_program_options'

ext_modules = [
    Extension(name='cdec._cdec',
        sources=['src/_cdec.cpp'],
        include_dirs=INC,
        library_dirs=LIB,
        libraries=[BOOST_PROGRAM_OPTIONS, 'z',
                   'cdec', 'utils', 'mteval', 'training', 'klm', 'klm_util'],
        extra_compile_args=['-DHAVE_CONFIG_H'],
        extra_link_args=LINK_ARGS),
    Extension(name='cdec.sa._sa',
        sources=['src/sa/_sa.c', 'src/sa/strmap.cc'])
]

setup(
    name='cdec',
    ext_modules=ext_modules,
    requires=['configobj'],
    packages=['cdec', 'cdec.sa'],
    package_dir={'': 'pkg'}
)
