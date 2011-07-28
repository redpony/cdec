#include "dcommon.h"




/*
 * init
 *
 */
bool
init(int argc, char** argv, po::variables_map* conf)
{
  po::options_description opts( "Options" );
  opts.add_options()
    ( "decoder-config,c", po::value<string>(), "configuration file for cdec" )
    ( "kbest,k",          po::value<size_t>(), "k for kbest" )
    ( "ngrams,n",         po::value<int>(),    "n for Ngrams" )
    ( "filter,f",         po::value<string>(), "filter kbest list" )
    ( "test",                                       "run tests and exit");
  po::options_description cmdline_options;
  cmdline_options.add(opts);
  po::store( parse_command_line(argc, argv, cmdline_options), *conf );
  po::notify( *conf );
  if ( ! (conf->count("decoder-config") || conf->count("test")) ) {
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
  if ( conf.count("test") ) run_tests(); 
  register_feature_functions();
  size_t k = conf["kbest"].as<size_t>();
  ReadFile ini_rf(conf["decoder-config"].as<string>());
  Decoder decoder(ini_rf.stream());
  KBestGetter observer(k);
  size_t N = 4; // TODO as parameter/in config 

  // TODO scoring metric as parameter/in config 
  // for approx. bleu
  //NgramCounts global_counts;
  //size_t global_hyp_len;
  //size_t global_ref_len;

  Weights weights;
  SparseVector<double> lambdas;
  weights.InitSparseVector(&lambdas);
  vector<double> dense_weights;

  lambdas.set_value(FD::Convert("logp"), 0);

 
  vector<string> strs, ref_strs;
  vector<WordID> ref_ids;
  string in, psg;
  size_t sid = 0;
  cerr << "(1 dot equals 100 lines of input)" << endl;
  while( getline(cin, in) ) {
    //if ( !SILENT )
    //    cerr << endl << endl << "Getting kbest for sentence #" << sid << endl;
    if ( (sid+1) % 100 == 0 ) {
        cerr << ".";
        if ( (sid+1)%1000 == 0 ) cerr << endl;
    }
    if ( sid > 5000 ) break;
    // weights
    dense_weights.clear();
    weights.InitFromVector( lambdas );
    weights.InitVector( &dense_weights );
    decoder.SetWeights( dense_weights );
    //if ( sid > 100 ) break;
    // handling input..
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
    for ( size_t i = 0; i < k; i++ ) {
      NgramCounts counts = make_ngram_counts( ref_ids, kb->sents[i], 4 );
      score = smooth_bleu( counts,
                           ref_ids.size(),
                           kb->sents[i].size(), N );
      ScorePair sp( kb->scores[i], score );
      scores.push_back( sp );
      //cout << "'" << TD::GetString( ref_ids ) << "' vs '" << TD::GetString( kb->sents[i] ) << "' SCORE=" << score << endl;
      //cout << kb->feats[i] << endl;
    }
    //cout << "###" << endl;
    SofiaLearner learner;
    learner.Init( sid, kb->feats, scores );
    learner.Update(lambdas);
    // initializing learner
    // TODO
    // updating weights
    //lambdas.set_value( FD::Convert("use_shell"), 1 );
    //lambdas.set_value( FD::Convert("use_a"), 1 );
    //print_FD();
    sid += 1; // TODO does cdec count this already?
  }

  weights.WriteToFile( "weights-final", true );
  
  cerr << endl;

  return 0;
}

