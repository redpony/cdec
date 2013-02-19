import logging
import resource
import gzip

def monitor_cpu():
    return (resource.getrusage(resource.RUSAGE_SELF).ru_utime+
            resource.getrusage(resource.RUSAGE_SELF).ru_stime)

def gzip_or_text(char* filename):
    if filename.endswith('.gz'):
        return gzip.GzipFile(filename)
    else:
        return open(filename)

logger = logging.getLogger('cdec.sa')

include "float_list.pxi"
include "int_list.pxi"
include "str_map.pxi"
include "data_array.pxi"
include "alignment.pxi"
include "bilex.pxi"
include "veb.pxi"
include "lcp.pxi"
include "sym.pxi"
include "rule.pxi"
include "precomputation.pxi"
include "suffix_array.pxi"
include "rulefactory.pxi"
include "features.pxi"
