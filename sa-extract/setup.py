from distutils.core import setup, Extension
from distutils.util import get_platform
import os.path

cstrmap_module = Extension('cstrmap', sources = ['cstrmap.c', 'strmap.cc'])
setup (name = 'CStringMap', version = '1.0', description = 'C string->int map', ext_modules = [cstrmap_module])

rule_module = Extension('rule',
                         sources = ['rule.c', 'strutil.c'])
setup (name = 'Rule', version = '1.0', description = 'rule class', ext_modules = [rule_module])

sym_module = Extension('sym',
                         sources = ['sym.c'])
setup (name = 'Sym', version = '1.0', description = 'symbol class', ext_modules = [sym_module])

cdat_module = Extension('cdat', sources = ['cdat.c'])
setup(name = "CDat", version = '1.0', description = 'C Data class', ext_modules = [cdat_module])

cintlist_module = Extension('cintlist', sources = ['cintlist.c'])
setup(name = "CIntList", version = '1.0', description = 'C int array/list class', ext_modules = [cintlist_module])

cfloatlist_module = Extension('cfloatlist', sources = ['cfloatlist.c'])
setup(name = "CFloatList", version = '1.0', description = 'C float array/list class', ext_modules = [cfloatlist_module])

calignment_module = Extension('calignment', sources = ['calignment.c'])
setup(name = "CAlignment", version = '1.0', description = 'C alignment class', ext_modules = [calignment_module])

csuf_module = Extension('csuf', sources = ['csuf.c'])
setup(name = "CSuffixArray", version = '1.0', description = 'C suffix array class', ext_modules = [csuf_module])

clex_module = Extension('clex', sources = ['clex.c'])
setup(name = "CLex", version = '1.0', description = 'C lexical class', ext_modules = [clex_module])

factory_module = Extension('rulefactory', sources = ['rulefactory.c'])
setup(name = "RuleFactory", version = '1.0', description = 'C rule factory classes', ext_modules = [factory_module])

cveb_module = Extension('cveb', sources = ['cveb.c'])
setup(name = "CVEB", version = '1.0', description = 'C impl. of van Emde Boas tree', ext_modules = [cveb_module])

lcp_module = Extension('lcp', sources = ['lcp.c'])
setup(name = "LCP", version = '1.0', description = 'C impl. of LCP', ext_modules = [lcp_module])

precomp_module = Extension('precomputation', sources = ['precomputation.c'])
setup(name = "Precomputation", version = '1.0', description = 'Precomputation Algorithm', ext_modules = [precomp_module])

