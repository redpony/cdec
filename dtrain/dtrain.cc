#include "dtrain.h"


bool
dtrain_init(int argc, char** argv, po::variables_map* cfg)
{
  po::options_description ini("Configuration File Options");
  ini.add_options()
    ("input",          po::value<string>()->default_value("-"),                                                "input file")
    ("output",         po::value<string>()->default_value("-"),                       "output weights file, '-' for STDOUT")
    ("input_weights",  po::value<string>(),                             "input weights file (e.g. from previous iteration)")
    ("decoder_config", po::value<string>(),                                                   "configuration file for cdec")
    ("sample_from",    po::value<string>()->default_value("kbest"),      "where to sample translations from: kbest, forest")
    ("k",              po::value<unsigned>()->default_value(100),                         "how many translations to sample")
    ("filter",         po::value<string>()->default_value("unique"),                        "filter kbest list: no, unique")
    ("pair_sampling",  po::value<string>()->default_value("all"),                  "how to sample pairs: all, rand, 108010")
    ("N",              po::value<unsigned>()->default_value(3),                                       "N for Ngrams (BLEU)")
    ("epochs",         po::value<unsigned>()->default_value(2),                             "# of iterations T (per shard)") 
    ("scorer",         po::value<string>()->default_value("stupid_bleu"),     "scoring: bleu, stupid_*, smooth_*, approx_*")
    ("stop_after",     po::value<unsigned>()->default_value(0),                              "stop after X input sentences")
    ("print_weights",  po::value<string>(),                                            "weights to print on each iteration")
    ("hstreaming",     po::value<bool>()->zero_tokens(),                                     "run in hadoop streaming mode")
    ("learning_rate",  po::value<weight_t>()->default_value(0.0005),                                        "learning rate")
    ("gamma",          po::value<weight_t>()->default_value(0),                          "gamma for SVM (0 for perceptron)")
    ("tmp",            po::value<string>()->default_value("/tmp"),                                        "temp dir to use")
    ("select_weights", po::value<string>()->default_value("last"), "output 'best' or 'last' weights ('VOID' to throw away)")
#ifdef DTRAIN_LOCAL
    ("refs,r",         po::value<string>(),                                                      "references in local mode")
#endif
    ("noup",           po::value<bool>()->zero_tokens(),                                            "do not update weights");
  po::options_description cl("Command Line Options");
  cl.add_options()
    ("config,c",         po::value<string>(),              "dtrain config file")
    ("quiet,q",          po::value<bool>()->zero_tokens(),           "be quiet")
    ("verbose,v",        po::value<bool>()->zero_tokens(),         "be verbose");
  cl.add(ini);
  po::store(parse_command_line(argc, argv, cl), *cfg);
  if (cfg->count("config")) {
    ifstream ini_f((*cfg)["config"].as<string>().c_str());
    po::store(po::parse_config_file(ini_f, ini), *cfg);
  }
  po::notify(*cfg);
  if (!cfg->count("decoder_config")) { 
    cerr << cl << endl;
    return false;
  }
  if (cfg->count("hstreaming") && (*cfg)["output"].as<string>() != "-") {
    cerr << "When using 'hstreaming' the 'output' param should be '-'.";
    return false;
  }
#ifdef DTRAIN_LOCAL
  if ((*cfg)["input"].as<string>() == "-") {
    cerr << "Can't use stdin as input with this binary. Recompile without DTRAIN_LOCAL" << endl;
    return false;
  }
#endif
  if ((*cfg)["sample_from"].as<string>() != "kbest"
       && (*cfg)["sample_from"].as<string>() != "forest") {
    cerr << "Wrong 'sample_from' param: '" << (*cfg)["sample_from"].as<string>() << "', use 'kbest' or 'forest'." << endl;
    return false;
  }
  if ((*cfg)["sample_from"].as<string>() == "kbest" && (*cfg)["filter"].as<string>() != "unique"
       && (*cfg)["filter"].as<string>() != "no") {
    cerr << "Wrong 'filter' param: '" << (*cfg)["filter"].as<string>() << "', use 'unique' or 'no'." << endl;
    return false;
  }
  if ((*cfg)["pair_sampling"].as<string>() != "all"
       && (*cfg)["pair_sampling"].as<string>() != "rand" && (*cfg)["pair_sampling"].as<string>() != "108010") {
    cerr << "Wrong 'pair_sampling' param: '" << (*cfg)["pair_sampling"].as<string>() << "', use 'all' or 'rand'." << endl;
    return false;
  }
  if ((*cfg)["select_weights"].as<string>() != "last"
       && (*cfg)["select_weights"].as<string>() != "best" && (*cfg)["select_weights"].as<string>() != "VOID") {
    cerr << "Wrong 'select_weights' param: '" << (*cfg)["select_weights"].as<string>() << "', use 'last' or 'best'." << endl;
    return false;
  }
  return true;
}

int
main(int argc, char** argv)
{
  // handle most parameters
  po::variables_map cfg;
  if (!dtrain_init(argc, argv, &cfg)) exit(1); // something is wrong 
  bool quiet = false;
  if (cfg.count("quiet")) quiet = true;
  bool verbose = false;  
  if (cfg.count("verbose")) verbose = true;
  bool noup = false;
  if (cfg.count("noup")) noup = true;
  bool hstreaming = false;
  if (cfg.count("hstreaming")) {
    hstreaming = true;
  }
  const unsigned k = cfg["k"].as<unsigned>();
  const unsigned N = cfg["N"].as<unsigned>(); 
  const unsigned T = cfg["epochs"].as<unsigned>();
  const unsigned stop_after = cfg["stop_after"].as<unsigned>();
  const string filter_type = cfg["filter"].as<string>();
  const string sample_from = cfg["sample_from"].as<string>();
  const string pair_sampling = cfg["pair_sampling"].as<string>();
  const string select_weights = cfg["select_weights"].as<string>();
  vector<string> print_weights;
  if (cfg.count("print_weights"))
    boost::split(print_weights, cfg["print_weights"].as<string>(), boost::is_any_of(" "));

  // setup decoder
  register_feature_functions();
  SetSilent(true);
  ReadFile ini_rf(cfg["decoder_config"].as<string>());
  if (!quiet)
    cerr << setw(25) << "cdec cfg " << "'" << cfg["decoder_config"].as<string>() << "'" << endl;
  Decoder decoder(ini_rf.stream());

  // scoring metric/scorer
  string scorer_str = cfg["scorer"].as<string>();
  LocalScorer* scorer;
  if (scorer_str == "bleu") {
    scorer = dynamic_cast<BleuScorer*>(new BleuScorer);
  } else if (scorer_str == "stupid_bleu") {
    scorer = dynamic_cast<StupidBleuScorer*>(new StupidBleuScorer);
  } else if (scorer_str == "smooth_bleu") {
    scorer = dynamic_cast<SmoothBleuScorer*>(new SmoothBleuScorer);
  } else if (scorer_str == "approx_bleu") {
    scorer = dynamic_cast<ApproxBleuScorer*>(new ApproxBleuScorer(N));
  } else {
    cerr << "Don't know scoring metric: '" << scorer_str << "', exiting." << endl;
    exit(1);
  }
  vector<score_t> bleu_weights;
  scorer->Init(N, bleu_weights);
  if (!quiet) cerr << setw(26) << "scorer '" << scorer_str << "'" << endl << endl;

  // setup decoder observer
  MT19937 rng; // random number generator
  HypSampler* observer;
  if (sample_from == "kbest")
    observer = dynamic_cast<KBestGetter*>(new KBestGetter(k, filter_type));
  else
    observer = dynamic_cast<KSampler*>(new KSampler(k, &rng));
  observer->SetScorer(scorer);

  // init weights
  vector<weight_t>& dense_weights = decoder.CurrentWeightVector();
  SparseVector<weight_t> lambdas;
  if (cfg.count("input_weights")) Weights::InitFromFile(cfg["input_weights"].as<string>(), &dense_weights);
  Weights::InitSparseVector(dense_weights, &lambdas);

  // meta params for perceptron, SVM
  weight_t eta = cfg["learning_rate"].as<weight_t>();
  weight_t gamma = cfg["gamma"].as<weight_t>();

  string output_fn = cfg["output"].as<string>();
  // input
  string input_fn = cfg["input"].as<string>();
  ReadFile input(input_fn);
  // buffer input for t > 0
  vector<string> src_str_buf;          // source strings (decoder takes only strings)
  vector<vector<WordID> > ref_ids_buf; // references as WordID vecs
  vector<string> weights_files;        // remember weights for each iteration
  // where temp files go
  string tmp_path = cfg["tmp"].as<string>();
#ifdef DTRAIN_LOCAL
  string refs_fn = cfg["refs"].as<string>();
  ReadFile refs(refs_fn);
#else
  string grammar_buf_fn = gettmpf(tmp_path, "dtrain-grammars");
  ogzstream grammar_buf_out;
  grammar_buf_out.open(grammar_buf_fn.c_str());
#endif
  
  unsigned in_sz = UINT_MAX; // input index, input size
  vector<pair<score_t, score_t> > all_scores;
  score_t max_score = 0.;
  unsigned best_it = 0;
  float overall_time = 0.;

  // output cfg
  if (!quiet) {
    cerr << _p5;
    cerr << endl << "dtrain" << endl << "Parameters:" << endl;
    cerr << setw(25) << "k " << k << endl;
    cerr << setw(25) << "N " << N << endl;
    cerr << setw(25) << "T " << T << endl;
    if (cfg.count("stop-after"))
      cerr << setw(25) << "stop_after " << stop_after << endl;
    if (cfg.count("input_weights"))
      cerr << setw(25) << "weights in" << cfg["input_weights"].as<string>() << endl;
    cerr << setw(25) << "input " << "'" << input_fn << "'" << endl;
#ifdef DTRAIN_LOCAL 
    cerr << setw(25) << "refs " << "'" << refs_fn << "'" << endl;
#endif
    cerr << setw(25) << "output " << "'" << output_fn << "'" << endl;
    if (sample_from == "kbest")
      cerr << setw(25) << "filter " << "'" << filter_type << "'" << endl;
    cerr << setw(25) << "learning rate " << eta << endl;
    cerr << setw(25) << "gamma " << gamma << endl;
    cerr << setw(25) << "sample from " << "'" << sample_from << "'" << endl;
    cerr << setw(25) << "pairs " << "'" << pair_sampling << "'" << endl;
    cerr << setw(25) << "select weights " << "'" << select_weights << "'" << endl;
    if (!verbose) cerr << "(a dot represents " << DTRAIN_DOTS << " lines of input)" << endl;
  }


  for (unsigned t = 0; t < T; t++) // T epochs
  {

  time_t start, end;  
  time(&start);
#ifndef DTRAIN_LOCAL
  igzstream grammar_buf_in;
  if (t > 0) grammar_buf_in.open(grammar_buf_fn.c_str());
#endif
  score_t score_sum = 0.;
  score_t model_sum(0);
  unsigned ii = 0, nup = 0, npairs = 0;
  if (!quiet) cerr << "Iteration #" << t+1 << " of " << T << "." << endl;

  while(true)
  {

    string in;
    bool next = false, stop = false; // next iteration or premature stop
    if (t == 0) {
      if(!getline(*input, in)) next = true;
    } else {
      if (ii == in_sz) next = true; // stop if we reach the end of our input
    }
    // stop after X sentences (but still iterate for those)
    if (stop_after > 0 && stop_after == ii && !next) stop = true;
    
    // produce some pretty output
    if (!hstreaming && !quiet && !verbose) {
      if (ii == 0) cerr << " ";
      if ((ii+1) % (DTRAIN_DOTS) == 0) {
        cerr << ".";
        cerr.flush();
      }
      if ((ii+1) % (20*DTRAIN_DOTS) == 0) {
        cerr << " " << ii+1 << endl;
        if (!next && !stop) cerr << " ";
      }
      if (stop) {
        if (ii % (20*DTRAIN_DOTS) != 0) cerr << " " << ii << endl;
        cerr << "Stopping after " << stop_after << " input sentences." << endl;
      } else {
        if (next) {
          if (ii % (20*DTRAIN_DOTS) != 0) cerr << " " << ii << endl;
        }
      }
    }
   
    // next iteration
    if (next || stop) break;

    // weights
    lambdas.init_vector(&dense_weights);

    // getting input
    vector<WordID> ref_ids; // reference as vector<WordID>
#ifndef DTRAIN_LOCAL
    vector<string> in_split; // input: sid\tsrc\tref\tpsg
    if (t == 0) {
      // handling input
      split_in(in, in_split); 
      // getting reference
      vector<string> ref_tok;
      boost::split(ref_tok, in_split[2], boost::is_any_of(" "));
      register_and_convert(ref_tok, ref_ids);
      ref_ids_buf.push_back(ref_ids);
      // process and set grammar
      bool broken_grammar = true;
      for (string::iterator it = in.begin(); it != in.end(); it++) {
        if (!isspace(*it)) {
          broken_grammar = false;
          break;
        }
      }
      if (broken_grammar) continue;
      boost::replace_all(in, "\t", "\n");
      in += "\n";
      grammar_buf_out << in << DTRAIN_GRAMMAR_DELIM << " " << in_split[0] << endl;
      decoder.SetSentenceGrammarFromString(in);
      src_str_buf.push_back(in_split[1]);
      // decode
      observer->SetRef(ref_ids);
      decoder.Decode(in_split[1], observer);
    } else {
      // get buffered grammar
      string grammar_str;
      while (true) {
        string rule;  
        getline(grammar_buf_in, rule);
        if (boost::starts_with(rule, DTRAIN_GRAMMAR_DELIM)) break;
        grammar_str += rule + "\n";
      }
      decoder.SetSentenceGrammarFromString(grammar_str);
      // decode
      observer->SetRef(ref_ids_buf[ii]);
      decoder.Decode(src_str_buf[ii], observer);
    }
#else
    if (t == 0) {
      string r_;
      getline(*refs, r_);
      vector<string> ref_tok;
      boost::split(ref_tok, r_, boost::is_any_of(" "));
      register_and_convert(ref_tok, ref_ids);
      ref_ids_buf.push_back(ref_ids);
      src_str_buf.push_back(in);
    } else {
      ref_ids = ref_ids_buf[ii];
    }
    observer->SetRef(ref_ids);
    if (t == 0) 
      decoder.Decode(in, observer);
    else
      decoder.Decode(src_str_buf[ii], observer);
#endif

    // get (scored) samples 
    vector<ScoredHyp>* samples = observer->GetSamples();

    if (verbose) {
      cerr << "--- ref for " << ii << " ";
      if (t > 0) printWordIDVec(ref_ids_buf[ii]);
      else printWordIDVec(ref_ids);
      for (unsigned u = 0; u < samples->size(); u++) {
        cerr << _p5 << _np << "[" << u << ". '";
        printWordIDVec((*samples)[u].w);
        cerr << "'" << endl;
        cerr << "SCORE=" << (*samples)[0].score << ",model="<< (*samples)[0].model << endl;
        cerr << "F{" << (*samples)[0].f << "} ]" << endl << endl;
      }
    }

    score_sum += (*samples)[0].score;
    model_sum += (*samples)[0].model;

    // weight updates
    if (!noup) {
      vector<pair<ScoredHyp,ScoredHyp> > pairs;
      if (pair_sampling == "all")
        sample_all_pairs(samples, pairs);
      if (pair_sampling == "rand")
        sample_rand_pairs(samples, pairs, &rng);
      if (pair_sampling == "108010")
        sample108010(samples, pairs);
      npairs += pairs.size();
       
      for (vector<pair<ScoredHyp,ScoredHyp> >::iterator it = pairs.begin();
           it != pairs.end(); it++) {
        score_t rank_error = it->second.score - it->first.score;
        if (!gamma) {
          // perceptron
          if (rank_error > 0) {
            SparseVector<weight_t> diff_vec = it->second.f - it->first.f;
            lambdas.plus_eq_v_times_s(diff_vec, eta);
            nup++;
          }
        } else {
          // SVM
          score_t margin = it->first.model - it->second.model;
          if (rank_error > 0 || margin < 1) {
            SparseVector<weight_t> diff_vec = it->second.f - it->first.f;
            lambdas.plus_eq_v_times_s(diff_vec, eta);
            nup++;
          }
          // regularization
          lambdas.plus_eq_v_times_s(lambdas, -2*gamma*eta*(1./npairs));
        }
      }
    }
    
    ++ii;

    if (hstreaming) cerr << "reporter:counter:dtrain,sid," << ii << endl;

  } // input loop

  if (scorer_str == "approx_bleu") scorer->Reset();

  if (t == 0) {
    in_sz = ii; // remember size of input (# lines)
  }

#ifndef DTRAIN_LOCAL
  if (t == 0) {
    grammar_buf_out.close();
  } else {
    grammar_buf_in.close();
  }
#endif

  // print some stats
  score_t score_avg = score_sum/(score_t)in_sz;
  score_t model_avg = model_sum/(score_t)in_sz;
  score_t score_diff, model_diff;
  if (t > 0) {
    score_diff = score_avg - all_scores[t-1].first;
    model_diff = model_avg - all_scores[t-1].second;
  } else {
    score_diff = score_avg;
    model_diff = model_avg;
  }
  if (!quiet) {
    cerr << _p5 << _p << "WEIGHTS" << endl;
    for (vector<string>::iterator it = print_weights.begin(); it != print_weights.end(); it++) {
      cerr << setw(18) << *it << " = " << lambdas.get(FD::Convert(*it)) << endl;
    }
    cerr << "        ---" << endl;
    cerr << _np << "      1best avg score: " << score_avg;
    cerr << _p << " (" << score_diff << ")" << endl;
    cerr << _np << "1best avg model score: " << model_avg;
    cerr << _p << " (" << model_diff << ")" << endl;
    cerr << "           avg #pairs: ";
    cerr << _np << npairs/(float)in_sz << endl;
    cerr << "              avg #up: ";
    cerr << nup/(float)in_sz << endl;
  }
  pair<score_t,score_t> remember;
  remember.first = score_avg;
  remember.second = model_avg;
  all_scores.push_back(remember);
  if (score_avg > max_score) {
    max_score = score_avg;
    best_it = t;
  }
  time (&end);
  float time_diff = difftime(end, start);
  overall_time += time_diff;
  if (!quiet) {
    cerr << _p2 << _np << "(time " << time_diff/60. << " min, ";
    cerr << time_diff/(float)in_sz<< " s/S)" << endl;
  }
  if (t+1 != T && !quiet) cerr << endl;

  if (noup) break;

  // write weights to file
  if (select_weights == "best") {
    string infix = "dtrain-weights-" + boost::lexical_cast<string>(t);
    lambdas.init_vector(&dense_weights);
    string w_fn = gettmpf(tmp_path, infix, "gz");
    Weights::WriteToFile(w_fn, dense_weights, true); 
    weights_files.push_back(w_fn);
  }

  } // outer loop

#ifndef DTRAIN_LOCAL
  unlink(grammar_buf_fn.c_str());
#endif

  if (!noup) {
    if (!quiet) cerr << endl << "Writing weights file to '" << output_fn << "' ..." << endl;
    if (select_weights == "last") { // last
      WriteFile of(output_fn); // works with '-'
      ostream& o = *of.stream();
      o.precision(17);
      o << _np;
      for (SparseVector<weight_t>::const_iterator it = lambdas.begin(); it != lambdas.end(); ++it) {
	    if (it->second == 0) continue;
        o << FD::Convert(it->first) << '\t' << it->second << endl;
      }
    } else if (select_weights == "VOID") { // do nothing with the weights
    } else { // best
      if (output_fn != "-") {
        CopyFile(weights_files[best_it], output_fn);  // always gzipped
      } else {
        ReadFile bestw(weights_files[best_it]);
        string o;
        cout.precision(17);
        cout << _np;
        while(getline(*bestw, o)) cout << o << endl;
      }
      for (vector<string>::iterator it = weights_files.begin(); it != weights_files.end(); ++it) {
        unlink(it->c_str());
        it->erase(it->end()-3, it->end());
        unlink(it->c_str());
      }
    }
    if (output_fn == "-" && hstreaming) cout << "__SHARD_COUNT__\t1" << endl;
    if (!quiet) cerr << "done" << endl;
  }
  
  if (!quiet) {
    cerr << _p5 << _np << endl << "---" << endl << "Best iteration: ";
    cerr << best_it+1 << " [SCORE '" << scorer_str << "'=" << max_score << "]." << endl;
    cerr << _p2 << "This took " << overall_time/60. << " min." << endl;
  }

  return 0;
}

