#!/usr/bin/env python
import sys, os, re, shutil
import subprocess, shlex, glob
import argparse
import logging
import random, time
import gzip, itertools
try:
  import cdec.score
except ImportError:
  sys.stderr.write('Could not import pycdec, see cdec/python/README.md for details\n')
  sys.exit(1)
have_mpl = True
try: 
  import matplotlib
  matplotlib.use('Agg')
  import matplotlib.pyplot as plt
except ImportError:
  have_mpl = False

#mira run script
#requires pycdec to be built, since it is used for scoring hypothesis
#translations.
#matplotlib must be installed for graphing to work
#email option requires mail

#scoring function using pycdec scoring
def fast_score(hyps, refs, metric):
  scorer = cdec.score.Scorer(metric)
  logging.info('loaded {0} references for scoring with {1}'.format(
                len(refs), metric))
  if metric=='BLEU':
    logging.warning('BLEU is ambiguous, assuming IBM_BLEU\n')
    metric = 'IBM_BLEU'
  elif metric=='COMBI':
    logging.warning('COMBI metric is no longer supported, switching to '
                    'COMB:TER=-0.5;BLEU=0.5')
    metric = 'COMB:TER=-0.5;BLEU=0.5'
  stats = sum(scorer(r).evaluate(h) for h,r in itertools.izip(hyps,refs))
  logging.info('Score={} ({})'.format(stats.score, stats.detail))
  return stats.score

#create new parallel input file in output directory in sgml format
def enseg(devfile, newfile, gprefix):
  try:
    dev = open(devfile)
    new = open(newfile, 'w')
  except IOError, msg:
    logging.error('Error opening source file')
    raise

  i = 0
  for line in dev:
    (src, refs) = line.split(' ||| ', 1)
    if re.match('\s*<seg', src):
      if re.search('id="[0-9]+"', src):
        new.write(line)
      else:
        logging.error('When using segments with pre-generated <seg> tags, '
                      'yout must include a zero based id attribute')
        sys.exit()
    else:
      sgml = '<seg id="{0}"'.format(i)
      if gprefix:
        #TODO check if grammar files gzipped or not
        if os.path.exists('{}.{}.gz'.format(gprefix,i)):
          sgml += ' grammar="{0}.{1}.gz"'.format(gprefix,i)
        elif os.path.exists('{}.{}'.format(gprefix,i)):
          sgml += ' grammar="{}.{}"'.format(gprefix,i)
        else:
          logging.error('Could not find grammar files with prefix '
                        '{}\n'.format(gprefix))
          sys.exit()
      sgml += '>{0}</seg> ||| {1}'.format(src, refs)
      new.write(sgml)
    i+=1
  new.close()
  dev.close()
  return i

def main():
  #set logging to write all info messages to stderr
  logging.basicConfig(level=logging.INFO)
  script_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
  if not have_mpl:
    logging.warning('Failed to import matplotlib, graphs will not be generated.')

  parser= argparse.ArgumentParser(
            formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('-d', '--devset', required=True,
                      help='dev set input file in parallel. '
                      'format: src ||| ref1 ||| ref2')
  parser.add_argument('-c', '--config', required=True,
                      help='decoder configuration file')
  parser.add_argument('-w','--weights',
                      help='initial weights file')
  parser.add_argument('-j', '--jobs', type=int, default=1,
                      help='number of decoder processes to run in parallel')
  parser.add_argument('-o','--output-dir', metavar='DIR',
                      help='directory for intermediate and output files. '
                      'defaults to mira.(devset name).(time)')
  parser.add_argument('-e', '--email', 
                      help='email address to send result report')
  parser.add_argument('-t', '--test', 
                      help='test set to decode and evaluate')
  parser.add_argument('--test-config', 
                      help='config file for testing. the config file used '
                      'for tuning feature weights will be used by default.')
  parser.add_argument('-m', '--metric', default='ibm_bleu',
                      help='metric to optimize. Example values: '
                           'ibm_bleu, nist_bleu, Koehn_bleu, TER, Combi')
  parser.add_argument('--max-iterations', type=int, default=20, metavar='N',
                      help='maximum number of iterations to run')
  parser.add_argument('--optimizer', type=int, default=2, choices=range(1,6),
                      help='learning method to use for weight update.'
                      ' Choices: 1) SGD, 2) PA MIRA with Selection from Cutting'
                      ' Plane, 3) Cutting Plane MIRA, 4) PA MIRA,'
                      ' 5) nbest MIRA with hope, fear, and model constraints')
  parser.add_argument('--metric-scale', type=int, default=1, metavar='N',
                      help='scale MT loss by this amount when computing'
                      ' hope/fear candidates')
  parser.add_argument('-k', '--kbest-size', type=int, default=500, metavar='N', 
                      help='size of k-best list to extract from forest')
  parser.add_argument('--update-size', type=int, metavar='N', 
                      help='size of k-best list to use for update. defaults to '
                      'equal kbest-size (applies to optimizer 5)')
  parser.add_argument('--step-size', type=float, default=0.001, 
                      help='controls aggresiveness of update')
  parser.add_argument('--hope', type=int, default=1, choices=range(1,3),
                     help='how to select hope candidate. options: '
                     '1) model score - cost, 2) min cost')
  parser.add_argument('--fear', type=int, default=1, choices=range(1,4),
                      help='how to select fear candidate. options: '
                      '1) model score + cost, 2) max cost, 3) max score')
  parser.add_argument('--sent-approx', action='store_true', 
                      help='use smoothed sentence-level MT metric')
  parser.add_argument('--no-pseudo', action='store_true',
                      help="don't use pseudo document to approximate MT metric")
  parser.add_argument('--no-unique', action='store_true',
                      help="don't extract unique k-best from forest")
  parser.add_argument('-g', '--grammar-prefix', metavar='PATH',
                      help='path to sentence specific grammar files')
  parser.add_argument('--pass-suffix', 
                      help='multipass decoding iteration. see documentation '
                           'at www.cdec-decoder.org for more information')
  args = parser.parse_args()

  args.metric = args.metric.upper()

  if not args.update_size:
    args.update_size = args.kbest_size
  
  #TODO fix path to match decode+evaluate (python month 1-12 instead of 0-11)
  #if an output directory isn't specified, create a unique directory name 
  #of the form mira.(devset).YYYYMMDD-HHMMSS
  if not args.output_dir:
    t = time.localtime()
    args.output_dir = 'mira.{0}.{1}{2:02}{3:02}-{4:02}{5:02}{6:02}'.format(
                      os.path.splitext(args.devset)[0], t[0], t[1], t[2],
                      t[3], t[4], t[5])
    
  if not os.path.isabs(args.output_dir):
    args.output_dir = os.path.abspath(args.output_dir)
  if os.path.exists(args.output_dir):
    if len(os.listdir(args.output_dir))>2:
      logging.error('Error: working directory {0} already exists\n'.format(
                    args.output_dir))
      sys.exit()
  else:
    os.mkdir(args.output_dir)

  if args.grammar_prefix:
    if not os.path.isabs(args.grammar_prefix):
      args.grammar_prefix = os.path.abspath(args.grammar_prefix)
  
  script = open(args.output_dir+'/rerun_mira.sh','w')
  script.write('cd {0}\n'.format(os.getcwd()))
  script.write(' '.join(sys.argv)+'\n')
  script.close()

  #create weights.0 file from initial weights file
  if args.weights:
    shutil.copy(args.weights,os.path.join(args.output_dir,'weights.0'))
  else: #if no weights given, use Glue 0 as default
    weights = open(args.output_dir+'/weights.0','w')
    weights.write('Glue 0\n')
    weights.close()
    args.weights = args.output_dir+'/weights.0'
  
  #create mira ini file
  shutil.copy(args.config,'{0}/kbest_cut_mira.ini'.format(args.output_dir))
  
  newdev = args.output_dir+'/dev.input'
  dev_size = enseg(args.devset, newdev, args.grammar_prefix)
  args.devset = newdev
  
  log_config(args)
  args.weights, hope_best_fear = optimize(args, script_dir, dev_size)
  
  graph_file = ''
  if have_mpl: graph_file = graph(args.output_dir, hope_best_fear, args.metric)

  dev_results, dev_bleu = evaluate(args.devset, args.weights, args.config, 
                         script_dir, args.output_dir)
  if args.test:
    if args.test_config:
      test_results, test_bleu = evaluate(args.test, args.weights, 
                              args.test_config, script_dir, args.output_dir)
    else:
      test_results, test_bleu = evaluate(args.test, args.weights, args.config,
                              script_dir, args.output_dir)
  else: 
    test_results = ''
    test_bleu = ''
  logging.info(dev_results+'\n')
  logging.info(test_results)

  write_report(graph_file, dev_results, dev_bleu, test_results, test_bleu, args)

  if graph_file:
    logging.info('A graph of the best/hope/fear scores over the iterations '
                 'has been saved to {}'.format(graph_file))

  print 'final weights:\n{}\n'.format(args.weights)

#graph of hope/best/fear metric values across all iterations
def graph(output_dir, hope_best_fear, metric):
  max_y = float(max(hope_best_fear['best']))*1.5
  plt.plot(hope_best_fear['best'], label='best')
  plt.plot(hope_best_fear['hope'], label='hope')
  plt.plot(hope_best_fear['fear'], label='fear')
  plt.axis([0,len(hope_best_fear['fear'])-1,0,max_y])
  plt.xlabel('Iteration')
  plt.ylabel(metric)
  plt.legend()
  graph_file = output_dir+'/mira.pdf'
  plt.savefig(graph_file)
  return graph_file

#evaluate a given test set using decode-and-evaluate.pl
def evaluate(testset, weights, ini, script_dir, out_dir):
  evaluator = '{}/../utils/decode-and-evaluate.pl'.format(script_dir)
  try:
    p = subprocess.Popen([evaluator, '-c', ini, '-w', weights, '-i', testset, 
                         '-d', out_dir], stdout=subprocess.PIPE)
    results, err = p.communicate()
    bleu, results = results.split('\n',1)
  except subprocess.CalledProcessError:
    logging.error('Evalutation of {} failed'.format(testset))
    results = ''
    bleu = ''
  return results, bleu

#print a report to out_dir/mira.results
#send email with results if email was given
def write_report(graph_file, dev_results, dev_bleu, 
                 test_results, test_bleu, args):
  features, top, bottom = weight_stats(args.weights) 
  top = [f+' '+str(w) for f,w in top]
  bottom = [f+' '+str(w) for f,w in bottom]
  subject = 'MIRA {0} {1:7}'.format(os.path.basename(args.devset), dev_bleu)
  if args.test:
    subject += ' {0} {1:7}'.format(os.path.basename(args.test), test_bleu)

  message = ('MIRA has finished running. '+
            'The final weights can be found at \n{}\n'.format(args.weights)+
            'Average weights across all iterations '+
            '\n{}/weights.average\n'.format(args.output_dir)+
            'Weights were calculated for {} features\n\n'.format(features)+
            '5 highest weights:\n{}\n\n'.format('\n'.join(top))+
            '5 lowest weights:\n{}\n'.format('\n'.join(bottom)))
  
  if dev_results:
    message += '\nEvaluation: dev set\n{}'.format(dev_results)
  if test_results:
    message += '\nEvaluation: test set\n{}'.format(test_results)
 
  out = open(args.output_dir+'/mira.results','w')
  out.write(message)
  out.close()
 
  if args.email:
    cmd = ['mail', '-s', subject]
    if graph_file:
      cmd += ['-a', graph_file]
    email_process = subprocess.Popen(cmd+[args.email], stdin = subprocess.PIPE)
    email_process.communicate(message)

#feature weights stats for report
def weight_stats(weight_file):
  f = open(weight_file)
  features = []
  for line in f:
    feat, weight = line.strip().split()
    features.append((feat,float(weight)))
  features.sort(key=lambda a: a[1], reverse=True)
  return len(features), features[:5], features[-5:]

#create source and refs files from parallel devset
#TODO remove when kbest_cut_mira changed to take parallel input
def split_devset(dev, outdir):
  parallel = open(dev)
  source = open(outdir+'/source.input','w')
  refs = open(outdir+'/refs.input', 'w')
  references = []
  for line in parallel:
    s,r = line.strip().split(' ||| ',1)
    source.write(s+'\n')
    refs.write(r+'\n')
    references.append(r)
  source.close()
  refs.close()
  return (outdir+'/source.input', outdir+'/refs.input')

def optimize(args, script_dir, dev_size):
  parallelize = script_dir+'/../utils/parallelize.pl'
  decoder = script_dir+'/kbest_cut_mira'
  (source, refs) = split_devset(args.devset, args.output_dir)
  port = random.randint(15000,50000)
  logging.info('using port {}'.format(port))
  num_features = 0
  last_p_score = 0
  best_score_iter = -1
  best_score = -1
  i = 0
  hope_best_fear = {'hope':[],'best':[],'fear':[]}
  #main optimization loop
  while i<args.max_iterations:
    logging.info('======= STARTING ITERATION {} ======='.format(i))
    logging.info('Starting at {}'.format(time.asctime()))

    #iteration specific files
    runfile = args.output_dir+'/run.raw.'+str(i)
    onebestfile = args.output_dir+'/1best.'+str(i)
    logdir = args.output_dir+'/logs.'+str(i)
    decoderlog = logdir+'/decoder.sentserver.log.'+str(i)
    weightdir = args.output_dir+'/weights.pass'+str(i)
    os.mkdir(logdir)
    os.mkdir(weightdir)
    weightsfile = args.output_dir+'/weights.'+str(i)
    logging.info('  log directory={}'.format(logdir))
    curr_pass = '0{}'.format(i)
    decoder_cmd = ('{0} -c {1} -w {2} -r{3} -m {4} -s {5} -b {6} -k {7} -o {8}'
                   ' -p {9} -O {10} -D {11} -h {12} -f {13} -C {14}').format(
                   decoder, args.config, weightsfile, refs, args.metric,
                   args.metric_scale, args.update_size, args.kbest_size, 
                   args.optimizer, curr_pass, weightdir, args.output_dir,
                   args.hope, args.fear, args.step_size)
    if not args.no_unique: 
      decoder_cmd += ' -u'
    if args.sent_approx:
      decoder_cmd += ' -a'
    if not args.no_pseudo:
      decoder_cmd += ' -e'
    
    #always use fork 
    parallel_cmd = '{0} --use-fork -e {1} -j {2} --'.format(
                    parallelize, logdir, args.jobs)
    
    cmd = parallel_cmd + ' ' + decoder_cmd
    logging.info('OPTIMIZATION COMMAND: {}'.format(cmd))
   
    dlog = open(decoderlog,'w')
    runf = open(runfile,'w')
    retries = 0
    num_topbest = 0

    while retries < 6:
      #call decoder through parallelize.pl
      p1 = subprocess.Popen(['cat', source], stdout=subprocess.PIPE)
      exit_code = subprocess.call(shlex.split(cmd), stderr=dlog, stdout=runf, 
                                  stdin=p1.stdout)
      p1.stdout.close()
      
      if exit_code:
        logging.error('Failed with exit code {}'.format(exit_code))
        sys.exit(exit_code)

      try:
        f = open(runfile)
      except IOError, msg:
        logging.error('Unable to open {}'.format(runfile))
        sys.exit()
      
      num_topbest = sum(1 for line in f)
      f.close()
      if num_topbest == dev_size: break
      logging.warning('Incorrect number of top best. Sleeping for 10 seconds and retrying...')
      time.sleep(10)
      retries += 1
    
    if dev_size != num_topbest:
      logging.error("Dev set contains "+dev_size+" sentences, but we don't "
                    "have topbest for all of these. Decoder failure? "
                    " Check "+decoderlog)
      sys.exit()
    dlog.close()
    runf.close()

    #write best, hope, and fear translations
    run = open(runfile)
    H = open(runfile+'.H', 'w')
    B = open(runfile+'.B', 'w')
    F = open(runfile+'.F', 'w')
    hopes = []
    bests = []
    fears = []
    for line in run:
      hope, best, fear = line.split(' ||| ')
      hopes.append(hope)
      bests.append(best)
      fears.append(fear)
      H.write('{}\n'.format(hope))
      B.write('{}\n'.format(best))
      F.write('{}\n'.format(fear))
    run.close()
    H.close()
    B.close()
    F.close()

    #gzip runfiles and log files to save space
    gzip_file(runfile)
    gzip_file(decoderlog)

    ref_file = open(refs)
    references = [line.split(' ||| ') for line in 
                  ref_file.read().strip().split('\n')]
    ref_file.close()
    #get score for best hypothesis translations, hope and fear translations
    dec_score = fast_score(bests, references, args.metric)
    dec_score_h = fast_score(hopes, references, args.metric)
    dec_score_f = fast_score(fears, references, args.metric)
    
    hope_best_fear['hope'].append(dec_score)
    hope_best_fear['best'].append(dec_score_h)
    hope_best_fear['fear'].append(dec_score_f)
    logging.info('DECODER SCORE: {0} HOPE: {1} FEAR: {2}'.format(
                  dec_score, dec_score_h, dec_score_f))
    if dec_score > best_score:
      best_score_iter = i
      best_score = dec_score

    new_weights_file = '{}/weights.{}'.format(args.output_dir, i+1)
    last_weights_file = '{}/weights.{}'.format(args.output_dir, i)
    i += 1
    weight_files = weightdir+'/weights.mira-pass*.*[0-9].gz'
    average_weights(new_weights_file, weight_files)

  logging.info('BEST ITERATION: {} (SCORE={})'.format(
               best_score_iter, best_score))
  weights_final = args.output_dir+'/weights.final'
  logging.info('WEIGHTS FILE: {}'.format(weights_final))
  shutil.copy(last_weights_file, weights_final)
  average_final_weights(args.output_dir)
  
  return weights_final, hope_best_fear

#TODO
#create a weights file with the average of the weights from each iteration
def average_final_weights(out_dir):
  logging.info('Average of weights from each iteration\n')
  weight_files = glob.glob(out_dir+'/weights.[1-9]*')
  features = {}
  for path in weight_files:
    weights = open(path)
    for line in weights:
      f, w = line.strip().split(' ', 1)
      if f in features:
        features[f] += float(w)
      else:
        features[f] = float(w)
    weights.close()

  out = open(out_dir+'/weights.average','w')
  for f in iter(features):
    out.write('{} {}\n'.format(f,features[f]/len(weight_files)))
  logging.info('An average weights file can be found at' 
               '\n{}\n'.format(out_dir+'/weights.average'))

#create gzipped version of given file with name filename.gz
# and delete original file
def gzip_file(filename):
  gzip_file = gzip.open(filename+'.gz','wb')
  f = open(filename)
  gzip_file.writelines(f)
  f.close()
  gzip_file.close()
  os.remove(filename)

#average the weights for a given pass
def average_weights(new_weights, weight_files):
  logging.info('AVERAGE {} {}'.format(new_weights, weight_files))
  feature_weights = {}
  total_mult = 0.0
  for path in glob.glob(weight_files):
    score = gzip.open(path)
    mult = 0
    logging.info('  FILE {}'.format(path))
    msg, ran, mult = score.readline().strip().split(' ||| ')
    logging.info('  Processing {} {}'.format(ran, mult))
    for line in score:
      f,w = line.split(' ',1)
      if f in feature_weights:
        feature_weights[f]+= float(mult)*float(w)
      else: 
        feature_weights[f] = float(mult)*float(w)
    total_mult += float(mult)
    score.close()
  
  #write new weights to outfile
  logging.info('Writing averaged weights to {}'.format(new_weights))
  out = open(new_weights, 'w')
  for f in iter(feature_weights):
    avg = feature_weights[f]/total_mult
    out.write('{} {}\n'.format(f,avg))

def log_config(args):
  logging.info('WORKING DIRECTORY={}'.format(args.output_dir))
  logging.info('INI FILE={}'.format(args.config))
  logging.info('DEVSET={}'.format(args.devset))
  logging.info('EVAL METRIC={}'.format(args.metric))
  logging.info('MAX ITERATIONS={}'.format(args.max_iterations))
  logging.info('PARALLEL JOBS={}'.format(args.jobs))
  logging.info('INITIAL WEIGHTS={}'.format(args.weights))
  if args.grammar_prefix:
    logging.info('GRAMMAR PREFIX={}'.format(args.grammar_prefix))
  if args.test:
    logging.info('TEST SET={}'.format(args.test))
  else:
    logging.info('TEST SET=none specified')
  if args.test_config:
    logging.info('TEST CONFIG={}'.format(args.test_config))
  if args.email:
    logging.info('EMAIL={}'.format(args.email))

if __name__=='__main__':
  main()
