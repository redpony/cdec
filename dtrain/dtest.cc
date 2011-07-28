#include "dcommon.h"




/*
 * init
 *
 */
bool
init(int argc, char** argv, po::variables_map* conf)
{
  int N;
  po::options_description opts( "Options" );
  opts.add_options()
    ( "decoder-config,c", po::value<string>(),                  "configuration file for cdec" )
    ( "weights,w",        po::value<string>(),                  "weights file")
    ( "ngrams,n",         po::value<int>(&N)->default_value(4), "N for Ngrams (default 5)" );
  po::options_description cmdline_options;
  cmdline_options.add(opts);
  po::store( parse_command_line(argc, argv, cmdline_options), *conf );
  po::notify( *conf );
  if ( ! (conf->count("decoder-config") || conf->count("weights")) ) {
    cerr << cmdline_options << endl;
    return false;
  }
  return true;
}


/*
 * main
 *
 */
int
main(int argc, char** argv)
{
  SetSilent(true);
  po::variables_map conf;
  if (!init(argc, argv, &conf)) return 1;
  register_feature_functions();
  size_t k = 1;
  ReadFile ini_rf(conf["decoder-config"].as<string>());
  Decoder decoder(ini_rf.stream());
  KBestGetter observer(k);
  size_t N = conf["ngrams"].as<int>();

  Weights weights;
  weights.InitFromFile(conf["weights"].as<string>());
  vector<double> w;
  weights.InitVector(&w);
  decoder.SetWeights(w);
 
  vector<string> strs, ref_strs;
  vector<WordID> ref_ids;
  string in, psg;
  size_t sid = 0;
  double overall = 0.0;
  cerr << "(1 dot equals 100 lines of input)" << endl;
  while( getline(cin, in) ) {
    if ( (sid+1) % 100 == 0 ) {
        cerr << ".";
        if ( (sid+1)%1000 == 0 ) cerr << endl;
    }
    if ( sid > 5000 ) break;
    strs.clear();
    boost::split( strs, in, boost::is_any_of("\t") );
    // grammar
    psg = boost::replace_all_copy( strs[2], " __NEXT_RULE__ ", "\n" ); psg += "\n";
    decoder.SetSentenceGrammar( psg );
    decoder.Decode( strs[0], &observer );
    KBestList* kb = observer.GetKBest();
    // reference
    ref_strs.clear(); ref_ids.clear();
    boost::split( ref_strs, strs[1], boost::is_any_of(" ") );
    register_and_convert( ref_strs, ref_ids );
    // scoring kbest
    double score = 0;
    Scores scores;
    NgramCounts counts = make_ngram_counts( ref_ids, kb->sents[0], 4 );
    score = smooth_bleu( counts,
                         ref_ids.size(),
                         kb->sents[0].size(), N );
    ScorePair sp( kb->scores[0], score );
    scores.push_back( sp );
    //cout << TD::GetString( kb->sents[0] ) << endl;
    overall += score;
    sid += 1;
  }
  cout << "Average score: " << overall/(sid+1) << endl;
  cerr << endl;

  return 0;
}

