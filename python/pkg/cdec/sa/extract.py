#!/usr/bin/env python
import sys
import os
import argparse
import logging
import cdec.sa

def main():
    logging.basicConfig(level=logging.INFO)
    parser = argparse.ArgumentParser(description='Extract grammars from a compiled corpus.')
    parser.add_argument('-c', '--config', required=True,
                        help='Extractor configuration')
    parser.add_argument('-g', '--grammars', required=True,
                        help='Grammar output path')
    args = parser.parse_args()

    if not os.path.exists(args.grammars):
        os.mkdir(args.grammars)

    extractor = cdec.sa.GrammarExtractor(args.config)
    for i, sentence in enumerate(sys.stdin):
        sentence = sentence[:-1]
        grammar_file = os.path.join(args.grammars, 'grammar.{0}'.format(i))
        with open(grammar_file, 'w') as output:
            for rule in extractor.grammar(sentence):
                output.write(str(rule)+'\n')
        grammar_file = os.path.abspath(grammar_file)
        print('<seg grammar="{0}">{1}</seg>'.format(grammar_file, sentence))

if __name__ == '__main__':
    main()
