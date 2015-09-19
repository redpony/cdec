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
  const string score_name        = conf["score"].as<string>();
  const size_t N                 = conf["N"].as<size_t>();
  const size_t T                 = conf["iterations"].as<size_t>();
  const weight_t eta             = conf["learning_rate"].as<weight_t>();
  const weight_t margin          = conf["margin"].as<weight_t>();
  const bool average             = conf["average"].as<bool>();
  const bool structured          = conf["struct"].as<bool>();
  const weight_t l1_reg          = conf["l1_reg"].as<weight_t>();
  const bool keep                = conf["keep"].as<bool>();
  const bool noup                = conf["disable_learning"].as<bool>();
  const string output_fn         = conf["output"].as<string>();
  const string output_data_which = conf["output_data"].as<string>();
  const bool output_data         = output_data_which!="";
  vector<string> print_weights;
  boost::split(print_weights, conf["print_weights"].as<string>(),
               boost::is_any_of(" "));

  // setup decoder and scorer
  register_feature_functions();
  SetSilent(true);
  ReadFile f(conf["decoder_conf"].as<string>());
  Decoder decoder(f.stream());
  Scorer* scorer;
  if (score_name == "nakov") {
     scorer = static_cast<PerSentenceBleuScorer*>(new PerSentenceBleuScorer(N));
  } else if (score_name == "papineni") {
     scorer = static_cast<BleuScorer*>(new BleuScorer(N)); 
  } else if (score_name == "lin") {
     scorer = static_cast<OriginalPerSentenceBleuScorer*>\
                           (new OriginalPerSentenceBleuScorer(N));
  } else if (score_name == "liang") {
     scorer = static_cast<SmoothPerSentenceBleuScorer*>\
                           (new SmoothPerSentenceBleuScorer(N));
  } else if (score_name == "chiang") {
      scorer = static_cast<ApproxBleuScorer*>(new ApproxBleuScorer(N));
  } else {
    assert(false);
  }
  ScoredKbest* observer = new ScoredKbest(k, scorer);

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
  vector<vector<Ngrams> > buf_ngs; // compute ngrams and lengths of references
  vector<vector<size_t> > buf_ls;  // just once
  size_t input_sz = 0;

  cerr << _p4;
  // output configuration
  cerr << "Parameters:" << endl;
  cerr << setw(25) << "bitext " << "'" << input_fn << "'" << endl;
  cerr << setw(25) << "k " << k << endl;
  cerr << setw(25) << "score " << "'" << score_name << "'" << endl;
  cerr << setw(25) << "N " << N << endl;
  cerr << setw(25) << "T " << T << endl;
  cerr << setw(25) << "learning rate " << eta << endl;
  cerr << setw(25) << "margin " << margin << endl;
  cerr << setw(25) << "average " << average << endl;
  cerr << setw(25) << "l1 reg " << l1_reg << endl;
  cerr << setw(25) << "decoder conf " << "'"
       << conf["decoder_conf"].as<string>() << "'" << endl;
  cerr << setw(25) << "input " << "'" << input_fn << "'" << endl;
  cerr << setw(25) << "output " << "'" << output_fn << "'" << endl;
  if (conf.count("input_weights")) {
    cerr << setw(25) << "weights in " << "'"
         << conf["input_weights"].as<string>() << "'" << endl;
  }
  cerr << "(1 dot per processed input)" << endl;

  // meta
  weight_t best=0., gold_prev=0.;
  size_t best_iteration = 0;
  time_t total_time = 0.;

  for (size_t t = 0; t < T; t++) // T iterations
  {

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
        buf_ngs.push_back({});
        buf_ls.push_back({});
        for (auto s: parts) {
          vector<WordID> r;
          vector<string> toks;
          boost::split(toks, s, boost::is_any_of(" "));
          for (auto tok: toks)
            r.push_back(TD::Convert(tok));
          buf_ngs.back().emplace_back(MakeNgrams(r, N));
          buf_ls.back().push_back(r.size());
        }
      }
    } else {
      next = i<input_sz;
    }

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
    observer->SetReference(buf_ngs[i], buf_ls[i]);
    decoder.Decode(buf[i], observer);
    vector<ScoredHyp>* samples = observer->GetSamples();

    // stats for 1best
    gold_sum += samples->front().gold;
    model_sum += samples->front().model;
    feature_count += observer->GetFeatureCount();
    list_sz += observer->GetSize();

    if (output_data) {
      if (output_data_which == "kbest") {
        OutputKbest(samples);
      } else if (output_data_which == "default") {
        OutputMultipartitePairs(samples, margin);
      } else if (output_data_which == "all") {
        OutputAllPairs(samples);
      }
    }

    // get pairs and update
    if (!noup) {

    SparseVector<weight_t> updates;
    if (structured)
      num_up += CollectUpdatesStruct(samples, updates);
    else
      num_up += CollectUpdates(samples, updates, margin);
    SparseVector<weight_t> lambdas_copy;
    if (l1_reg)
      lambdas_copy = lambdas;
    lambdas.plus_eq_v_times_s(updates, eta);

    // update context for approx. BLEU
    if (score_name == "chiang") {
      for (auto it: *samples) {
        if (it.rank == 0) {
          scorer->UpdateContext(it.w, buf_ngs[i], buf_ls[i], 0.9);
          break;
        }
      }
    }

    // l1 regularization
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

  // update average
  if (average)
    w_average += lambdas;

  // stats
  weight_t gold_avg = gold_sum/(weight_t)input_sz;
  cerr << _p << "WEIGHTS" << endl;
  for (auto name: print_weights)
    cerr << setw(18) << name << " = " << lambdas.get(FD::Convert(name)) << endl;
  cerr << "        ---" << endl;
  cerr << _np << "       1best avg score: " << gold_avg*100;
  cerr << _p << " (" << (gold_avg-gold_prev)*100 << ")" << endl;
  cerr << " 1best avg model score: "
       << model_sum/(weight_t)input_sz << endl;
  cerr << "         avg # updates: ";
  cerr << _np <<  num_up/(float)input_sz << endl;
  cerr << "   non-0 feature count: " << lambdas.num_nonzero() << endl;
  cerr << "           avg f count: " << feature_count/(float)list_sz << endl;
  cerr << "           avg list sz: " << list_sz/(float)input_sz << endl;

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

