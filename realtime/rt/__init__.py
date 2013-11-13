# Add pycdec to the Python path if user hasn't
import os
import sys
try:
    import cdec
except ImportError as ie:
    try:
        pycdec = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'python')
        sys.path.append(pycdec)
        import cdec
    except:
        sys.stderr.write('Error: cannot import pycdec.  Please check that cdec/python is built.\n')
        raise ie

# Regular init imports
from rt import *
import aligner
import decoder
import util
