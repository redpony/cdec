import argparse
import logging
from itertools import izip
import cdec, cdec.score

def evaluate(hyp, ref):
    """ Compute BLEU score for a set of hypotheses+references """
    return sum(cdec.score.BLEU(r).evaluate(h) for h, r in izip(hyp, ref)).score

T1, T2, T3 = 5, 10, 20 # number of iterations (global, CCCP, SSD)
K = 500 # k-best list size
C = 1 # regularization coefficient
eta = 1e-4 # step size
cost = lambda c: 10 * (1 - c.score) # cost definition

def rampion(decoder, sources, references):
    # Empty k-best lists
    candidate_sets = [cdec.score.BLEU(refs).candidate_set() for refs in references]
    # Weight vector -> sparse
    w = decoder.weights.tosparse()
    w0 = w.copy()

    N = len(sources)
    for t in range(T1):
        logging.info('Iteration {0}: translating...'.format(t+1))
        # Get the hypergraphs and extend the k-best lists
        hgs = []
        for src, candidates in izip(sources, candidate_sets):
            hg = decoder.translate(src)
            hgs.append(hg)
            candidates.add_kbest(hg, K)
        # BLEU score for the previous iteration
        score = evaluate((hg.viterbi() for hg in hgs), references)
        logging.info('BLEU: {:.2f}'.format(100 * score))
        logging.info('Optimizing...')
        for _ in range(T2):
            # y_i^+, h_i^+; i=1..N
            plus = [max(candidates, key=lambda c: w.dot(c.fmap) - cost(c)).fmap
                    for candidates in candidate_sets]
            for _ in range(T3):
                for fp, candidates in izip(plus, candidate_sets):
                    # y^-, h^-
                    fm = max(candidates, key=lambda c: w.dot(c.fmap) + cost(c)).fmap
                    # update weights (line 11-12)
                    w += eta * ((fp - fm) - C/N * (w - w0))
        logging.info('Updated weight vector: {0}'.format(dict(w)))
        # Update decoder weights
        decoder.weights = w

def main():
    logging.basicConfig(level=logging.INFO, format='%(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--config', help='cdec config', required=True)
    parser.add_argument('-w', '--weights', help='initial weights', required=True)
    parser.add_argument('-r', '--reference', help='reference file', required=True)
    parser.add_argument('-s', '--source', help='source file', required=True)
    args = parser.parse_args()

    with open(args.config) as fp:
        config = fp.read()

    decoder = cdec.Decoder(config)
    decoder.read_weights(args.weights)
    with open(args.reference) as fp:
        references = fp.readlines()
    with open(args.source) as fp:
        sources = fp.readlines()
    assert len(references) == len(sources)
    rampion(decoder, sources, references)

    for fname, fval in sorted(dict(decoder.weights).iteritems()):
        print('{0}\t{1}'.format(fname, fval))

if __name__ == '__main__':
    main()
