from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext

ext_modules = [
    Extension(name='_cdec',
        sources=['src/_cdec.pyx'],
        language='C++', 
        include_dirs=['..', 'src/', '../decoder', '../utils'],
        library_dirs=['../decoder', '../utils', '../mteval', '../klm/lm', '../klm/util'],
        libraries=['boost_program_options-mt', 'z',
                   'cdec', 'utils', 'mteval', 'klm', 'klm_util'])
]

setup(
    name='cdec',
    cmdclass={'build_ext': build_ext},
    ext_modules=ext_modules,
    packages=['cdec', 'cdec.scfg']
)
