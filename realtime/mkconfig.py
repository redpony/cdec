#!/usr/bin/env python

import os
import shutil
import sys

# Import first to make sure pycdec is on path
import rt

from cdec.configobj import ConfigObj


def main():

    if len(sys.argv[1:]) != 12:
        sys.stderr.write('usage: {} a.fwd_params a.fwd_err a.rev_params a.rev_err sa sa.ini mono.klm libcdec_ff_hpyplm.so corpus.hpyplm cdec.ini weights.final output.d\n'.format(sys.argv[0]))
        sys.exit(2)

    (a_fwd_params, a_fwd_err, a_rev_params, a_rev_err, sa, sa_ini, mono_klm, libcdec_ff_hpyplm_so, corpus_hpyplm, cdec_ini, weights_final, output_d) = sys.argv[1:]

    if os.path.exists(output_d):
        sys.stderr.write('Directory {} exists, exiting.\n'.format(output_d))
        sys.exit(1)
    
    # output.d
    os.mkdir(output_d)

    # alignment model
    shutil.copy(a_fwd_params, os.path.join(output_d, 'a.fwd_params'))
    shutil.copy(a_fwd_err, os.path.join(output_d, 'a.fwd_err'))
    shutil.copy(a_rev_params, os.path.join(output_d, 'a.rev_params'))
    shutil.copy(a_rev_err, os.path.join(output_d, 'a.rev_err'))

    # grammar extractor
    shutil.copytree(sa, os.path.join(output_d, 'sa'))
    config = ConfigObj(sa_ini, unrepr=True)
    config.filename = os.path.join(output_d, 'sa.ini')
    rt.util.sa_ini_for_config(config)
    config.write()

    # language models
    shutil.copy(mono_klm, os.path.join(output_d, 'mono.klm'))
    shutil.copy(libcdec_ff_hpyplm_so, os.path.join(output_d, 'libcdec_ff_hpyplm.so'))
    shutil.copy(corpus_hpyplm, os.path.join(output_d, 'corpus.hpyplm'))

    # decoder config
    config = [[f.strip() for f in line.split('=')] for line in open(cdec_ini)]
    rt.util.cdec_ini_for_config(config)
    with open(os.path.join(output_d, 'cdec.ini'), 'w') as output:
        for (k, v) in config:
            output.write('{}={}\n'.format(k, v))

    # weights
    shutil.copy(weights_final, os.path.join(output_d, 'weights.final'))
            
if __name__ == '__main__':
    main()
