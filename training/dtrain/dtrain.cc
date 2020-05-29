#include "dtrain.h"
#include "sample.h"
#include "score.h"
#include "update.h"

using namespace dtrain;

int
main(int argc, char** argv)
{
  // get configuration
  po::variables_map conf;
  if (!dtrain_init(argc, argv, &conf))
    return 1;
  const size_t k                 = conf["k"].as<size_t>();
  const bool unique_kbest        = conf["unique_kbest"].as<bool>();
  const bool forest_sample       = conf["forest_sample"].as<bool>();
  const string score_name        = conf["score"].as<string>();
  const weight_t nakov_fix       = conf["nakov_fix"].as<weight_t>();
  const weight_t chiang_decay    = conf["chiang_decay"].as<weight_t>();
  const size_t N                 = conf["N"].as<size_t>();
  const size_t T                 = conf["iterations"].as<size_t>();
  const weight_t eta             = conf["learning_rate"].as<weight_t>();
  const weight_t margin          = conf["margin"].as<weight_t>();
  const weight_t cut             = conf["cut"].as<weight_t>();
  const bool adjust_cut          = conf["adjust"].as<bool>();
  const bool all_pairs           = cut==0;
  const bool average             = conf["average"].as<bool>();
  const bool pro                 = conf["pro_sampling"].as<bool>();
  const bool structured          = conf["structured"].as<bool>();
  const weight_t threshold       = conf["threshold"].as<weight_t>();
  const size_t max_up            = conf["max_pairs"].as<size_t>();
  const weight_t l1_reg          = conf["l1_reg"].as<weight_t>();
  const bool keep                = conf["keep"].as<bool>();
  const bool noup                = conf["disable_learning"].as<bool>();
  const string output_fn         = conf["output"].as<string>();
  vector<string> print_weights;
  boost::split(print_weights, conf["print_weights"].as<string>(),
               boost::is_any_of(" "));
  const string output_updates_fn = conf["output_updates"].as<string>();
  const bool output_updates      = output_updates_fn!="";
  const string output_raw_fn     = conf["output_raw"].as<string>();
  const bool output_raw          = output_raw_fn!="";
  const bool use_adadelta        = conf["adadelta"].as<bool>();
  const weight_t adadelta_decay  = conf["adadelta_decay"].as<weight_t>();
  const weight_t adadelta_eta    = 0.000001;
  const string adadelta_input    = conf["adadelta_input"].as<string>();
  const string adadelta_output   = conf["adadelta_output"].as<string>();
  const size_t max_input         = conf["stop_after"].as<size_t>();
  const bool batch               = conf["batch"].as<bool>();
  const bool all                 = conf["all"].as<bool>();

  // setup decoder
  register_feature_functions();
  SetSilent(true);
  ReadFile f(conf["decoder_conf"].as<string>());
  Decoder decoder(f.stream());

  // setup scorer & observer
  Scorer* scorer;
  if (score_name == "nakov") {
     scorer = static_cast<NakovBleuScorer*>(new NakovBleuScorer(N, nakov_fix));
  } else if (score_name == "papineni") {
     scorer = static_cast<PapineniBleuScorer*>(new PapineniBleuScorer(N));
  } else if (score_name == "lin") {
     scorer = static_cast<LinBleuScorer*>(new LinBleuScorer(N));
  } else if (score_name == "liang") {
     scorer = static_cast<LiangBleuScorer*>(new LiangBleuScorer(N));
  } else if (score_name == "chiang") {
      scorer = static_cast<ChiangBleuScorer*>(new ChiangBleuScorer(N));
  } else if (score_name == "sum") {
      scorer = static_cast<SumBleuScorer*>(new SumBleuScorer(N));
  } else {
    assert(false);
  }
  HypSampler* observer;
  if (forest_sample)
    observer = new KSampler(k, scorer);
  else if (unique_kbest)
    observer = new KBestSampler(k, scorer);
  else
    observer = new KBestNoFilterSampler(k, scorer);

  // weights
  vector<weight_t>& decoder_weights = decoder.CurrentWeightVector();
  SparseVector<weight_t> lambdas, w_average;
  if (conf.count("input_weights")) {
    Weights::InitFromFile(conf["input_weights"].as<string>(), &decoder_weights);
    Weights::InitSparseVector(decoder_weights, &lambdas);
  }

  // input
  string input_fn = conf["bitext"].as<string>();
  ReadFile input(input_fn);
  vector<string> buf;              // decoder only accepts strings as input
  vector<vector<Ngrams> > buffered_ngrams; // compute ngrams and lengths of references
  vector<vector<size_t> > buffered_lengths;  // (just once)
  size_t input_sz = 0;

  // output configuration
  cerr << fixed << setprecision(2);
  cerr << "Parameters:" << endl;
  cerr << setw(25) << "bitext " << "'" << input_fn << "'" << endl;
  cerr << setw(25) << "k " << k << endl;
  if (unique_kbest && !forest_sample)
    cerr << setw(25) << "unique k-best " << unique_kbest << endl;
  if (forest_sample)
    cerr << setw(25) << "forest " << forest_sample << endl;
  if (all_pairs)
    cerr << setw(25) << "all pairs " << all_pairs << endl;
  else if (pro)
    cerr << setw(25) << "PRO " << pro << endl;
  cerr << setw(25) << "score " << "'" << score_name << "'" << endl;
  if (score_name == "nakov")
    cerr << setw(25) << "nakov fix " << nakov_fix << endl;
  if (score_name == "chiang")
    cerr << setw(25) << "chiang decay " << chiang_decay << endl;
  cerr << setw(25) << "N " << N << endl;
  cerr << setw(25) << "T " << T << endl;
  cerr << scientific << setw(25) << "learning rate " << eta << endl;
  cerr << setw(25) << "margin " << margin << endl;
  if (!structured) {
    cerr << fixed << setw(25) << "cut " << round(cut*100) << "%" << endl;
    cerr << setw(25) << "adjust " << adjust_cut << endl;
  } else {
    cerr << setw(25) << "struct. obj " << structured << endl;
  }
  if (threshold > 0)
    cerr << setw(25) << "threshold " << threshold << endl;
  if (max_up != numeric_limits<size_t>::max())
    cerr << setw(25) << "max up. " << max_up << endl;
  if (noup)
    cerr << setw(25) << "no up. " << noup << endl;
  cerr << setw(25) << "average " << average << endl;
  cerr << scientific << setw(25) << "l1 reg. " << l1_reg << endl;
  cerr << setw(25) << "decoder conf " << "'"
       << conf["decoder_conf"].as<string>() << "'" << endl;
  cerr << setw(25) << "input " << "'" << input_fn << "'" << endl;
  cerr << setw(25) << "output " << "'" << output_fn << "'" << endl;
  if (conf.count("input_weights")) {
    cerr << setw(25) << "weights in " << "'"
         << conf["input_weights"].as<string>() << "'" << endl;
  }
  cerr << setw(25) << "batch " << batch << endl;
  if (noup)
    cerr << setw(25) << "no updates!" << endl;
  if (use_adadelta) {
    cerr << setw(25) << "adadelta " << use_adadelta << endl;
    cerr << setw(25) << "   decay " << adadelta_decay << endl;
    if (adadelta_input != "")
      cerr << setw(25) << "-input "  << adadelta_input << endl;
    if (adadelta_output != "")
      cerr << setw(25) << "-output "  << adadelta_output << endl;
  }
  cerr << "(1 dot per processed input)" << endl;

  // meta
  weight_t best=0., gold_prev=0.;
  size_t best_iteration = 0;
  time_t total_time = 0.;

  // output
  WriteFile out_up, out_raw;
  if (output_raw) {
    out_raw.Init(output_raw_fn);
    *out_raw << setprecision(numeric_limits<double>::digits10+1);
  }
  if (output_updates) {
    out_up.Init(output_updates_fn);
    *out_up << setprecision(numeric_limits<double>::digits10+1);
  }

  // adadelta
  SparseVector<weight_t> gradient_accum, update_accum;
  if (use_adadelta && adadelta_input!="") {
    vector<weight_t> grads_tmp;
    Weights::InitFromFile(adadelta_input+".gradient.gz", &grads_tmp);
    Weights::InitSparseVector(grads_tmp, &gradient_accum);
    vector<weight_t> update_tmp;
    Weights::InitFromFile(adadelta_input+".update.gz", &update_tmp);
    Weights::InitSparseVector(update_tmp, &update_accum);
  }

  for (size_t t = 0; t < T; t++) // T iterations
  {

  // batch update
  SparseVector<weight_t> batch_update;

  time_t start, end;
  time(&start);
  weight_t gold_sum=0., model_sum=0.;
  size_t i=0, num_up=0, feature_count=0, list_sz=0;

  cerr << "Iteration #" << t+1 << " of " << T << "." << endl;

  while(true)
  {
    bool next = true;

    // getting input
    if (t == 0) {
      string in;
      if(!getline(*input, in)) {
        next = false;
      } else {
        vector<string> parts;
        boost::algorithm::split_regex(parts, in, boost::regex(" \\|\\|\\| "));
        buf.push_back(parts[0]);
        parts.erase(parts.begin());
        buffered_ngrams.push_back({});
        buffered_lengths.push_back({});
        for (auto s: parts) {
          vector<WordID> r;
          vector<string> toks;
          boost::split(toks, s, boost::is_any_of(" "));
          for (auto tok: toks)
            r.push_back(TD::Convert(tok));
          buffered_ngrams.back().emplace_back(ngrams(r, N));
          buffered_lengths.back().push_back(r.size());
        }
      }
    } else {
      next = i<input_sz;
    }

    if (max_input == i)
      next = false;

    // produce some pretty output
    if (next) {
      if (i%20 == 0)
        cerr << " ";
      cerr << ".";
      if ((i+1)%20==0)
        cerr << " " << i+1 << endl;
    } else {
      if (i%20 != 0)
        cerr << " " << i << endl;
    }
    cerr.flush();

    // stop iterating
    if (!next) break;

    // decode
    if (t > 0 || i > 0)
      lambdas.init_vector(&decoder_weights);
    observer->reference_ngrams = &buffered_ngrams[i];
    observer->reference_lengths = &buffered_lengths[i];
    decoder.Decode(buf[i], observer);
    vector<Hyp>* sample = &(observer->sample);

    // stats for 1-best
    gold_sum += sample->front().gold;
    model_sum += sample->front().model;
    feature_count += observer->feature_count;
    list_sz += observer->effective_size;

    if (output_raw)
      output_sample(sample, out_raw, i);

    // update model
    if (!noup) {

    SparseVector<weight_t> updates;
    if (structured)
      num_up += update_structured(sample, updates, margin,
                                  out_up, i);
    else if (all_pairs)
      num_up += updates_all(sample, updates, max_up, margin, threshold, all,
                            out_up, i);
    else if (pro)
      num_up += updates_pro(sample, updates, cut, max_up, threshold,
                            out_up, i);
    else
      num_up += updates_multipartite(sample, updates, cut, margin,
                                     max_up, threshold, adjust_cut, all,
                                     out_up, i);

    SparseVector<weight_t> lambdas_copy;
    if (l1_reg)
      lambdas_copy = lambdas;

    if (use_adadelta) { // adadelta update
      SparseVector<weight_t> squared;
      for (auto it: updates)
        squared[it.first] = pow(it.second, 2.0);
      gradient_accum *= adadelta_decay;
      squared *= 1.0-adadelta_decay;
      gradient_accum += squared;
      SparseVector<weight_t> u = gradient_accum + update_accum;
      for (auto it: u)
          u[it.first] = -1.0*(
                              sqrt(update_accum[it.first]+adadelta_eta)
                              /
                              sqrt(gradient_accum[it.first]+adadelta_eta)
                             ) * updates[it.first];
      lambdas += u;
      update_accum *= adadelta_decay;
      for (auto it: u)
          u[it.first] = pow(it.second, 2.0);
      update_accum = update_accum + (u*(1.0-adadelta_decay));
    } else if (batch) {
      batch_update += updates;
    } else { // regular update
      lambdas.plus_eq_v_times_s(updates, eta);
    }

    // update context for Chiang's approx. BLEU
    if (score_name == "chiang") {
      for (auto it: *sample) {
        if (it.rank == 0) {
          scorer->update_context(it.w, buffered_ngrams[i],
                                 buffered_lengths[i], chiang_decay);
          break;
        }
      }
    }

    // \ell_1 regularization
    // NB: regularization is done after each sentence,
    //     not after every single pair!
    if (l1_reg) {
      SparseVector<weight_t>::iterator it = lambdas.begin();
      for (; it != lambdas.end(); ++it) {
        weight_t v = it->second;
        if (!v)
          continue;
        if (!lambdas_copy.get(it->first)       // new or..
            || lambdas_copy.get(it->first)!=v) // updated feature
        {
          if (v > 0) {
            it->second = max(0., v - l1_reg);
          } else {
            it->second = min(0., v + l1_reg);
          }
        }
      }
    }

    } // noup

    i++;

  } // input loop

  if (t == 0)
    input_sz = i; // remember size of input (# lines)

  // batch
  if (batch) {
    batch_update /= (weight_t)num_up;
    lambdas.plus_eq_v_times_s(batch_update, eta);
    lambdas.init_vector(&decoder_weights);
  }

  // update average
  if (average)
    w_average += lambdas;

  if (adadelta_output != "") {
     WriteFile g(adadelta_output+".gradient.gz");
    for (auto it: gradient_accum)
      *g << FD::Convert(it.first) << " " << it.second << endl;
    WriteFile u(adadelta_output+".update.gz");
    for (auto it: update_accum)
      *u << FD::Convert(it.first) << " " << it.second << endl;
  }

  // stats
  weight_t gold_avg = gold_sum/(weight_t)input_sz;
  cerr << setiosflags(ios::showpos) << scientific << "WEIGHTS" << endl;
  for (auto name: print_weights) {
    cerr << setw(18) << name << " = "
         << lambdas.get(FD::Convert(name));
    if (use_adadelta) {
      weight_t rate = -1.0*(sqrt(update_accum[FD::Convert(name)]+adadelta_eta)
                          / sqrt(gradient_accum[FD::Convert(name)]+adadelta_eta));
      cerr << " {" << rate << "}";
    }
    cerr << endl;
  }
  cerr << "        ---" << endl;
  cerr << resetiosflags(ios::showpos)  << fixed
       << "       1best avg score: "   << gold_avg*100;
  cerr << setiosflags(ios::showpos)    << fixed << " ("
       << (gold_avg-gold_prev)*100     << ")" << endl;
  cerr << scientific << " 1best avg model score: "
       << model_sum/(weight_t)input_sz << endl;
  cerr << fixed;
  cerr << "         avg # updates: ";
  cerr << resetiosflags(ios::showpos)  <<  num_up/(float)input_sz << endl;
  cerr << "   non-0 feature count: "   << lambdas.num_nonzero() << endl;
  cerr << "           avg f count: "   << feature_count/(float)list_sz << endl;
  cerr << "           avg list sz: "   << list_sz/(float)input_sz << endl;

  if (gold_avg > best) {
    best = gold_avg;
    best_iteration = t;
  }
  gold_prev = gold_avg;

  time (&end);
  time_t time_diff = difftime(end, start);
  total_time += time_diff;
  cerr << "(time " << time_diff/60. << " min, ";
  cerr << time_diff/input_sz << " s/S)" << endl;
  if (t+1 != T) cerr << endl;

  if (keep) { // keep intermediate weights
    lambdas.init_vector(&decoder_weights);
    string w_fn = "weights." + boost::lexical_cast<string>(t) + ".gz";
    Weights::WriteToFile(w_fn, decoder_weights, true);
  }

  } // outer loop

  // final weights
  if (average) {
    w_average /= T;
    w_average.init_vector(decoder_weights);
  } else if (!keep) {
    lambdas.init_vector(decoder_weights);
  }
  if (average || !keep)
    Weights::WriteToFile(output_fn, decoder_weights, true);

  cerr << endl << "---" << endl << "Best iteration: ";
  cerr << best_iteration+1 << " [GOLD = " << best*100 << "]." << endl;
  cerr << "This took " << total_time/60. << " min." << endl;

  return 0;
}

