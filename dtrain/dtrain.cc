#include "common.h"
#include "kbestget.h"
#include "learner.h"
#include "util.h"

#ifdef DTRAIN_DEBUG
#include "tests.h"
#endif



/*
 * init
 *
 */
bool
init(int argc, char** argv, po::variables_map* conf)
{
  po::options_description opts( "Options" );
  size_t k, N, T;
  // TODO scoring metric as parameter/in config 
  opts.add_options()
    ( "decoder-config,c", po::value<string>(),                          "configuration file for cdec" )
    ( "kbest,k",          po::value<size_t>(&k)->default_value(DTRAIN_DEFAULT_K),       "k for kbest" )
    ( "ngrams,n",         po::value<size_t>(&N)->default_value(DTRAIN_DEFAULT_N),      "n for Ngrams" )
    ( "filter,f",         po::value<string>(),                                    "filter kbest list" ) // FIXME
    ( "epochs,t",         po::value<size_t>(&T)->default_value(DTRAIN_DEFAULT_T), "# of iterations T" ) 
#ifndef DTRAIN_DEBUG
    ;
#else
    ( "test",                                  "run tests and exit");
#endif
  po::options_description cmdline_options;
  cmdline_options.add(opts);
  po::store( parse_command_line(argc, argv, cmdline_options), *conf );
  po::notify( *conf );
  if ( ! conf->count("decoder-config") ) { 
    cerr << cmdline_options << endl;
    return false;
  }
  #ifdef DTRAIN_DEBUG       
  if ( ! conf->count("test") ) {
    cerr << cmdline_options << endl;
    return false;
  }
  #endif
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
#ifdef DTRAIN_DEBUG
  if ( conf.count("test") ) run_tests(); 
#endif
  register_feature_functions();
  size_t k = conf["kbest"].as<size_t>();
  ReadFile ini_rf( conf["decoder-config"].as<string>() );
  Decoder decoder(ini_rf.stream());
  KBestGetter observer( k );
  size_t N = conf["ngrams"].as<size_t>(); 
  size_t T = conf["epochs"].as<size_t>();

  // for approx. bleu
  //NgramCounts global_counts( N );
  //size_t global_hyp_len = 0;
  //size_t global_ref_len = 0;

  Weights weights;
  SparseVector<double> lambdas;
  weights.InitSparseVector(&lambdas);
  vector<double> dense_weights;

  vector<string> strs, ref_strs;
  vector<WordID> ref_ids;
  string in, psg;
  size_t sn = 0;
  cerr << "(A dot equals " << DTRAIN_DOTOUT << " lines of input.)" << endl;

  for ( size_t t = 0; t < T; t++ )
  {

  while( getline(cin, in) ) {
    if ( (sn+1) % DTRAIN_DOTOUT == 0 ) {
        cerr << ".";
        if ( (sn+1) % (20*DTRAIN_DOTOUT) == 0 ) cerr << endl;
    }
    //if ( sn > 5000 ) break;
    // weights
    dense_weights.clear();
    weights.InitFromVector( lambdas );
    weights.InitVector( &dense_weights );
    decoder.SetWeights( dense_weights );
    // handling input
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
    //size_t cand_len = 0;
    Scores scores;
    for ( size_t i = 0; i < kb->sents.size(); i++ ) {
      NgramCounts counts = make_ngram_counts( ref_ids, kb->sents[i], N );
      /*if ( i == 0 ) {
        global_counts += counts;
        global_hyp_len += kb->sents[i].size();
        global_ref_len += ref_ids.size();
        cand_len = 0;
      } else {
        cand_len = kb->sents[i].size();
      }
      score = bleu( global_counts,
                    global_ref_len,
                     global_hyp_len + cand_len, N );*/
      score = bleu ( counts, ref_ids.size(), kb->sents[i].size(), N );
      ScorePair sp( kb->scores[i], score );
      scores.push_back( sp );
      //cout << "'" << TD::GetString( ref_ids ) << "' vs '";
      //cout << TD::GetString( kb->sents[i] ) << "' SCORE=" << score << endl;
      //cout << kb->feats[i] << endl;
    }
    // learner
    SofiaLearner learner;
    learner.Init( sn, kb->feats, scores );
    learner.Update(lambdas);
    //print_FD();
    sn += 1;
  }

  } // outer loop

  cerr << endl;
  weights.WriteToFile( "data/weights-vanilla", false );

  return 0;
}

