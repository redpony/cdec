#!/usr/bin/env python
import argparse
import os
import logging
import cdec.configobj
import cdec.sa
from cdec.sa._sa import monitor_cpu
import sys

MAX_PHRASE_LENGTH = 4
def precompute(f_sa, max_len, max_nt, max_size, min_gap, rank1, rank2, tight_phrases):
    lcp = cdec.sa.LCP(f_sa)
    stats = sorted(lcp.compute_stats(MAX_PHRASE_LENGTH), reverse=True)
    precomp = cdec.sa.Precomputation(from_stats=stats,
            fsarray=f_sa,
            precompute_rank=rank1,
            precompute_secondary_rank=rank2,
            max_length=max_len,
            max_nonterminals=max_nt,
            train_max_initial_size=max_size,
            train_min_gap_size=min_gap)
    return precomp

def main():
    preprocess_start_time = monitor_cpu()
    sys.setrecursionlimit(sys.getrecursionlimit() * 100)

    logging.basicConfig(level=logging.INFO)
    logger = logging.getLogger('cdec.sa.compile')
    parser = argparse.ArgumentParser(description='Compile a corpus into a suffix array.')
    parser.add_argument('--maxnt', '-n', type=int, default=2,
                        help='Maximum number of non-terminal symbols')
    parser.add_argument('--maxlen', '-l', type=int, default=5,
                        help='Maximum number of terminals')
    parser.add_argument('--maxsize', '-s', type=int, default=15,
                        help='Maximum rule span')
    parser.add_argument('--mingap', '-g', type=int, default=1,
                        help='Minimum gap size')
    parser.add_argument('--rank1', '-r1', type=int, default=100,
                        help='Number of pre-computed frequent patterns')
    parser.add_argument('--rank2', '-r2', type=int, default=10,
                        help='Number of pre-computed super-frequent patterns)')
    parser.add_argument('--loose', action='store_true',
                        help='Enable loose phrase extraction (default: tight)')
    parser.add_argument('-c', '--config', default='/dev/stdout',
                        help='Output configuration')
    parser.add_argument('-f', '--source',
                        help='Source language corpus')
    parser.add_argument('-e', '--target',
                        help='Target language corpus')
    parser.add_argument('-b', '--bitext',
                        help='Parallel text (source ||| target)')
    parser.add_argument('-a', '--alignment', required=True,
                        help='Bitext word alignment')
    parser.add_argument('-o', '--output', required=True,
                        help='Output path')
    args = parser.parse_args()

    if not ((args.source and args.target) or args.bitext):
        parser.error('a parallel corpus is required\n'
        '\tuse -f (source) with -e (target) or -b (bitext)')

    param_names = ('max_len', 'max_nt', 'max_size', 'min_gap',
            'rank1', 'rank2', 'tight_phrases')
    params = (args.maxlen, args.maxnt, args.maxsize, args.mingap,
            args.rank1, args.rank2, not args.loose)

    if not os.path.exists(args.output):
        os.mkdir(args.output)

    f_sa_bin = os.path.join(args.output, 'f.sa.bin')
    e_bin = os.path.join(args.output, 'e.bin')
    precomp_file = 'precomp.{0}.{1}.{2}.{3}.{4}.{5}.bin'.format(*params)
    precomp_bin = os.path.join(args.output, precomp_file)
    a_bin = os.path.join(args.output, 'a.bin')
    lex_bin = os.path.join(args.output, 'lex.bin')

    start_time = monitor_cpu()
    logger.info('Compiling source suffix array')
    if args.bitext:
        f_sa = cdec.sa.SuffixArray(from_text=args.bitext, side='source')
    else:
        f_sa = cdec.sa.SuffixArray(from_text=args.source)
    f_sa.write_binary(f_sa_bin)
    stop_time = monitor_cpu()
    logger.info('Compiling source suffix array took %f seconds', stop_time - start_time)

    start_time = monitor_cpu()
    logger.info('Compiling target data array')
    if args.bitext:
        e = cdec.sa.DataArray(from_text=args.bitext, side='target')
    else:
        e = cdec.sa.DataArray(from_text=args.target)
    e.write_binary(e_bin)
    stop_time = monitor_cpu()
    logger.info('Compiling target data array took %f seconds', stop_time - start_time)

    start_time = monitor_cpu()
    logger.info('Precomputing frequent phrases')
    precompute(f_sa, *params).write_binary(precomp_bin)
    stop_time = monitor_cpu()
    logger.info('Compiling precomputations took %f seconds', stop_time - start_time)

    start_time = monitor_cpu()
    logger.info('Compiling alignment')
    a = cdec.sa.Alignment(from_text=args.alignment)
    a.write_binary(a_bin)
    stop_time = monitor_cpu()
    logger.info('Compiling alignment took %f seonds', stop_time - start_time)

    start_time = monitor_cpu()
    logger.info('Compiling bilexical dictionary')
    lex = cdec.sa.BiLex(from_data=True, alignment=a, earray=e, fsarray=f_sa)
    lex.write_binary(lex_bin)
    stop_time = monitor_cpu()
    logger.info('Compiling bilexical dictionary took %f seconds', stop_time - start_time)

    # Write configuration
    config = cdec.configobj.ConfigObj(args.config, unrepr=True)
    config['f_sa_file'] = os.path.abspath(f_sa_bin)
    config['e_file'] = os.path.abspath(e_bin)
    config['a_file'] = os.path.abspath(a_bin)
    config['lex_file'] = os.path.abspath(lex_bin)
    config['precompute_file'] = os.path.abspath(precomp_bin)
    for name, value in zip(param_names, params):
        config[name] = value
    config.write()
    preprocess_stop_time = monitor_cpu()
    logger.info('Overall preprocessing step took %f seconds', preprocess_stop_time - preprocess_start_time)

if __name__ == '__main__':
    main()
