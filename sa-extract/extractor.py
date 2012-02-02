#!/usr/bin/env python

# vim:expandtab:shiftwidth=4

import sys, gc, monitor, sgml
import optparse
import model
import log
import cn

models = []

def add_model(m,w=0.0):
    models.append(m)

def extract_grammar(input):
    confnet = cn.ConfusionNet(input)
    meta = input.meta
    for m in models:
        m.input(confnet.columns, meta)

if __name__ == "__main__":
    optparser = optparse.OptionParser()
    optparser.add_option("-c", "--config", dest="config", help="configuration module")
    optparser.add_option("-x", "--extra", dest="extra", help="output grammar name override")
    (opts,args) = optparser.parse_args()

    if opts.config is None:
        raise ValueError, "You must specify a configuration file."
    else:
        if log.level >= 1:
            log.write("Reading configuration from %s\n" % opts.config)
        execfile(opts.config)

    if len(args) >= 1 and args[0] != "-":
        input_file = file(args[0], "r")
    else:
        input_file = sys.stdin

    if len(args) >= 2 and args[1] != "-":
        output_file = file(args[1], "w")
    else:
        output_file = sys.stdout

    gc.collect()
    if log.level >= 1:
        log.write("all structures loaded, memory %s, time %s\n" % (monitor.memory(), monitor.cpu()))
        log.write("models: %s\n" % (" ".join(str(x.name) for x in models)))

    sents = sgml.read_raw(input_file)
    for sent in sents:
        mark = sent.getmark()
        if mark is not None:
            (tag, attrs) = mark
            if tag == "seg":
                sent.unmark()
                dattrs = sgml.attrs_to_dict(attrs)
                sent.meta = attrs
        extract_grammar(sent)

