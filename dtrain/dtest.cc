#include "dcommon.h"




/*
 * init
 *
 */
bool
init(int argc, char** argv, po::variables_map* conf)
{
  int N;
  bool q;
  po::options_description opts( "Options" );
  opts.add_options()
    ( "decoder-config,c", po::value<string>(),                      "configuration file for cdec" )
    ( "weights,w",        po::value<string>(),                      "weights file")
    ( "ngrams,n",         po::value<int>(&N)->default_value(4),     "N for Ngrams (default 5)" )
    ( "quiet,q",          po::value<bool>(&q)->default_value(true), "do not output translations" );
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
  bool quiet = conf["quiet"].as<bool>();

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
  double overall1 = 0.0;
  double overall2 = 0.0;
  cerr << "(a dot equals 100 lines of input)" << endl;
  while( getline(cin, in) ) {
    if ( (sid+1) % 100 == 0 ) {
        cerr << ".";
        if ( (sid+1)%1000 == 0 ) cerr << endl;
    }
    //if ( sid > 5000 ) break;
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
    double score = 0.0;
    double score1 = 0.0;
    double score2 = 0.0;
    NgramCounts counts = make_ngram_counts( ref_ids, kb->sents[0], 4 );
    score = smooth_bleu( counts,
                         ref_ids.size(),
                         kb->sents[0].size(), N );
    score1 = stupid_bleu( counts, ref_ids.size(), kb->sents[0].size(), N) ;
    score2 = bleu( counts, ref_ids.size(), kb->sents[0].size(), N );
    //if ( ! quiet )
    cout << TD::GetString( kb->sents[0] ) << endl;
    overall += score;
    overall1 += score1;
    overall2 += score2;
    sid += 1;
  }
  cerr << "Average score (smooth): " << overall/(double)(sid+1) << endl;
  cerr << "Average score (stupid): " << overall1/(double)(sid+1) << endl;
  cerr << "Average score (normal): " << overall2/(double)(sid+1) << endl;
  cerr << endl;

  return 0;
}

