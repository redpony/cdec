#include "dtrain.h"


bool
dtrain_init(int argc, char** argv, po::variables_map* cfg)
{
  po::options_description ini("Configuration File Options");
  ini.add_options()
    ("input",          po::value<string>()->default_value("-"),                            "input file")
    ("output",         po::value<string>()->default_value("-"),         "output weights file (or VOID)")
    ("input_weights",  po::value<string>(),         "input weights file (e.g. from previous iteration)")
    ("decoder_config", po::value<string>(),                               "configuration file for cdec")
    ("k",              po::value<unsigned>()->default_value(100), "size of kbest or sample from forest")
    ("sample_from",    po::value<string>()->default_value("kbest"),    "where to get translations from")
    ("filter",         po::value<string>()->default_value("unique"),                "filter kbest list")
    ("pair_sampling",  po::value<string>()->default_value("all"),      "how to sample pairs: all, rand")
    ("N",              po::value<unsigned>()->default_value(3),                          "N for Ngrams")
    ("epochs",         po::value<unsigned>()->default_value(2),                     "# of iterations T") 
    ("scorer",         po::value<string>()->default_value("stupid_bleu"),              "scoring metric")
    ("stop_after",     po::value<unsigned>()->default_value(0),          "stop after X input sentences")
    ("print_weights",  po::value<string>(),                        "weights to print on each iteration")
    ("hstreaming",     po::value<bool>()->zero_tokens(),                 "run in hadoop streaming mode")
    ("learning_rate",  po::value<double>()->default_value(0.0005),                      "learning rate")
    ("gamma",          po::value<double>()->default_value(0.),       "gamma for SVM (0 for perceptron)")
    ("tmp",            po::value<string>()->default_value("/tmp"),                    "temp dir to use")
    ("noup",           po::value<bool>()->zero_tokens(),                        "do not update weights");
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
  if ((*cfg)["filter"].as<string>() != "unique"
       && (*cfg)["filter"].as<string>() != "no") {
    cerr << "Wrong 'filter' param: '" << (*cfg)["filter"].as<string>() << "', use 'unique' or 'no'." << endl;
  }
  if ((*cfg)["pair_sampling"].as<string>() != "all"
       && (*cfg)["pair_sampling"].as<string>() != "rand") {
    cerr << "Wrong 'pair_sampling' param: '" << (*cfg)["pair_sampling"].as<string>() << "', use 'all' or 'rand'." << endl;
  }
  if ((*cfg)["sample_from"].as<string>() != "kbest"
       && (*cfg)["sample_from"].as<string>() != "forest") {
    cerr << "Wrong 'sample_from' param: '" << (*cfg)["sample_from"].as<string>() << "', use 'kbest' or 'forest'." << endl;
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
    quiet = true;
  }
  const unsigned k = cfg["k"].as<unsigned>();
  const unsigned N = cfg["N"].as<unsigned>(); 
  const unsigned T = cfg["epochs"].as<unsigned>();
  const unsigned stop_after = cfg["stop_after"].as<unsigned>();
  const string filter_type = cfg["filter"].as<string>();
  const string sample_from = cfg["sample_from"].as<string>();
  const string pair_sampling = cfg["pair_sampling"].as<string>();
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
  } else if (scorer_str == "stupid_bleu") {
    scorer = dynamic_cast<StupidBleuScorer*>(new StupidBleuScorer);
  } else if (scorer_str == "smooth_bleu") {
      scorer = dynamic_cast<SmoothBleuScorer*>(new SmoothBleuScorer);
  } else if (scorer_str == "approx_bleu") {
      scorer = dynamic_cast<StupidBleuScorer*>(new StupidBleuScorer); // FIXME
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
  Weights weights;
  if (cfg.count("input_weights")) weights.InitFromFile(cfg["input_weights"].as<string>());
  SparseVector<double> lambdas;
  weights.InitSparseVector(&lambdas);
  vector<double> dense_weights;

  // meta params for perceptron, SVM
  double eta = cfg["learning_rate"].as<double>();
  double gamma = cfg["gamma"].as<double>();
  lambdas.add_value(FD::Convert("__bias"), 0);

  // input
  string input_fn = cfg["input"].as<string>();
  ReadFile input(input_fn);
    // buffer input for t > 0
  vector<string> src_str_buf;          // source strings
  vector<vector<WordID> > ref_ids_buf; // references as WordID vecs
  // this is for writing the grammar buffer file
  char grammar_buf_fn[1024];
  strcpy(grammar_buf_fn, cfg["tmp"].as<string>().c_str());
  strcat(grammar_buf_fn, "/dtrain-grammars-XXXXXX");
  mkstemp(grammar_buf_fn);
  ogzstream grammar_buf_out;
  grammar_buf_out.open(grammar_buf_fn);
  
  unsigned in_sz = 999999999; // input index, input size
  vector<pair<score_t,score_t> > all_scores;
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
    cerr << setw(25) << "input " << "'" << cfg["input"].as<string>() << "'" << endl;
    cerr << setw(25) << "output " << "'" << cfg["output"].as<string>() << "'" << endl;
    if (sample_from == "kbest")
      cerr << setw(25) << "filter " << "'" << filter_type << "'" << endl;
    cerr << setw(25) << "learning rate " << eta << endl;
    cerr << setw(25) << "gamma " << gamma << endl;
    cerr << setw(25) << "sample from " << "'" << sample_from << "'" << endl;
    cerr << setw(25) << "pairs " << "'" << pair_sampling << "'" << endl;
    if (!verbose) cerr << "(a dot represents " << DTRAIN_DOTS << " lines of input)" << endl;
  }


  for (unsigned t = 0; t < T; t++) // T epochs
  {

  time_t start, end;  
  time(&start);
  igzstream grammar_buf_in;
  if (t > 0) grammar_buf_in.open(grammar_buf_fn);
  score_t score_sum = 0., model_sum = 0.;
  unsigned ii = 0;
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
    if (!quiet && !verbose) {
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
    dense_weights.clear();
    weights.InitFromVector(lambdas);
    weights.InitVector(&dense_weights);
    decoder.SetWeights(dense_weights);

    // getting input
    vector<string> in_split; // input: sid\tsrc\tref\tpsg
    vector<WordID> ref_ids;  // reference as vector<WordID>
    if (t == 0) {
      // handling input
      boost::split(in_split, in, boost::is_any_of("\t"));
      // getting reference
      vector<string> ref_tok;
      boost::split(ref_tok, in_split[2], boost::is_any_of(" "));
      register_and_convert(ref_tok, ref_ids);
      ref_ids_buf.push_back(ref_ids);
      // process and set grammar
      bool broken_grammar = true;
      for (string::iterator ti = in_split[3].begin(); ti != in_split[3].end(); ti++) {
        if (!isspace(*ti)) {
          broken_grammar = false;
          break;
        }
      }
      if (broken_grammar) continue;
      boost::replace_all(in_split[3], " __NEXT__RULE__ ", "\n"); // TODO
      in_split[3] += "\n";
      grammar_buf_out << in_split[3] << DTRAIN_GRAMMAR_DELIM << " " << in_split[0] << endl;
      decoder.SetSentenceGrammarFromString(in_split[3]);
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

    // get (scored) samples 
    vector<ScoredHyp>* samples = observer->GetSamples();

    if (verbose) {
      cout << "[ref: '";
      if (t > 0) cout << ref_ids_buf[ii];
      else cout << ref_ids;
      cout << endl;
      cout << _p5 << _np << "1best: " << "'" << (*samples)[0].w << "'" << endl;
      cout << "SCORE=" << (*samples)[0].score << ",model="<< (*samples)[0].model << endl;
      cout << "F{" << (*samples)[0].f << "} ]" << endl << endl;
    }
    score_sum += (*samples)[0].score;
    model_sum += (*samples)[0].model;

//////////////////////////////////////////////////////////
    // UPDATE WEIGHTS
    if (!noup) {
      vector<pair<ScoredHyp,ScoredHyp> > pairs;
      if (pair_sampling == "all")
        sample_all_pairs(samples, pairs);
      if (pair_sampling == "rand")
        sample_rand_pairs(samples, pairs, &rng);
      if (pair_sampling == "108010")
        sample108010(samples, pairs);
       
      for (vector<pair<ScoredHyp,ScoredHyp> >::iterator ti = pairs.begin();
            ti != pairs.end(); ti++) {

        SparseVector<double> dv;
        if (ti->first.score - ti->second.score < 0) {
          dv = ti->second.f - ti->first.f;
      //} else {
        //dv = ti->first - ti->second;
      //}
          dv.add_value(FD::Convert("__bias"), -1);
        
          //SparseVector<double> reg;
          //reg = lambdas * (2 * gamma);
          //dv -= reg;
          lambdas += dv * eta;

          if (verbose) {
            /*cerr << "{{ f("<< ti->first_rank <<") > f(" << ti->second_rank << ") but g(i)="<< ti->first_score <<" < g(j)="<< ti->second_score << " so update" << endl;
            cerr << " i  " << TD::GetString(samples->sents[ti->first_rank]) << endl;
            cerr << "    " << samples->feats[ti->first_rank] << endl;
            cerr << " j  " << TD::GetString(samples->sents[ti->second_rank]) << endl;
            cerr << "    " << samples->feats[ti->second_rank] << endl; 
            cerr << " diff vec: " << dv << endl;
            cerr << " lambdas after update: " << lambdas << endl;
            cerr << "}}" << endl;*/
          }
        } else {
          //SparseVector<double> reg;
          //reg = lambdas * (2 * gamma);
          //lambdas += reg * (-eta);
        }

      }

      //double l2 = lambdas.l2norm();
      //if (l2) lambdas /= lambdas.l2norm();
    }
//////////////////////////////////////////////////////////

    ++ii;

    if (hstreaming) cerr << "reporter:counter:dtrain,sid," << ii << endl;

  } // input loop

  if (t == 0) {
    in_sz = ii; // remember size of input (# lines)
    grammar_buf_out.close();
  } else {
    grammar_buf_in.close();
  }

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
    cerr << setw(16) << *it << " = " << lambdas.get(FD::Convert(*it)) << endl;
  }
  cerr << "        ---" << endl;
  cerr << _np << "      1best avg score: " << score_avg;
  cerr << _p << " (" << score_diff << ")" << endl;
  cerr << _np << "1best avg model score: " << model_avg;
  cerr << _p << " (" << model_diff << ")" << endl;
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

  } // outer loop

  unlink(grammar_buf_fn);

  if (!noup) {
    if (!quiet) cerr << endl << "writing weights file '" << cfg["output"].as<string>() << "' ...";
    if (cfg["output"].as<string>() == "-") {
      cout << _p9;
      for (SparseVector<double>::const_iterator ti = lambdas.begin();
            ti != lambdas.end(); ++ti) {
	if (ti->second == 0) continue;
        cout << _np << FD::Convert(ti->first) << "\t" << ti->second << endl;
      }
      if (hstreaming) cout << "__SHARD_COUNT__\t1" << endl;
    } else if (cfg["output"].as<string>() != "VOID") {
      weights.InitFromVector(lambdas);
      weights.WriteToFile(cfg["output"].as<string>(), true);
    }
    if (!quiet) cerr << "done" << endl;
  }
  
  if (!quiet) {
    cerr << _p5 << _np << endl << "---" << endl << "Best iteration: ";
    cerr << best_it+1 << " [SCORE '" << scorer_str << "'=" << max_score << "]." << endl;
    cerr << _p2 << "This took " << overall_time/60. << " min." << endl;
  }

  return 0;
}

