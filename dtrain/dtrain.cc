#include "common.h"
#include "kbestget.h"
#include "util.h"
#include "sample.h"

#include "ksampler.h"

// boost compression
#include <boost/iostreams/device/file.hpp> 
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
//#include <boost/iostreams/filter/zlib.hpp>
//#include <boost/iostreams/filter/bzip2.hpp>
using namespace boost::iostreams;


#ifdef DTRAIN_DEBUG
#include "tests.h"
#endif


/*
 * init
 *
 */
bool
init(int argc, char** argv, po::variables_map* cfg)
{
  po::options_description conff( "Configuration File Options" );
  size_t k, N, T, stop;
  string s, f;
  conff.add_options()
    ( "decoder_config", po::value<string>(),                            "configuration file for cdec" )
    ( "kbest",          po::value<size_t>(&k)->default_value(DTRAIN_DEFAULT_K),         "k for kbest" )
    ( "ngrams",         po::value<size_t>(&N)->default_value(DTRAIN_DEFAULT_N),        "N for Ngrams" )
    ( "filter",         po::value<string>(&f)->default_value("unique"),           "filter kbest list" )
    ( "epochs",         po::value<size_t>(&T)->default_value(DTRAIN_DEFAULT_T),   "# of iterations T" ) 
    ( "input",          po::value<string>(),                                             "input file" )
    ( "scorer",         po::value<string>(&s)->default_value(DTRAIN_DEFAULT_SCORER), "scoring metric" )
    ( "output",         po::value<string>(),                                    "output weights file" )
    ( "stop_after",     po::value<size_t>(&stop)->default_value(0),    "stop after X input sentences" )
    ( "weights_file",   po::value<string>(),      "input weights file (e.g. from previous iteration)" )
    ( "wprint",         po::value<string>(),                     "weights to print on each iteration" )
    ( "noup",           po::value<bool>()->zero_tokens(),                     "do not update weights" );

  po::options_description clo("Command Line Options");
  clo.add_options()
    ( "config,c",         po::value<string>(),              "dtrain config file" )
    ( "quiet,q",          po::value<bool>()->zero_tokens(),           "be quiet" )
    ( "verbose,v",        po::value<bool>()->zero_tokens(),         "be verbose" )
#ifndef DTRAIN_DEBUG
    ;
#else
    ( "test", "run tests and exit");
#endif
  po::options_description config_options, cmdline_options;

  config_options.add(conff);
  cmdline_options.add(clo);
  cmdline_options.add(conff);

  po::store( parse_command_line(argc, argv, cmdline_options), *cfg );
  if ( cfg->count("config") ) {
    ifstream config( (*cfg)["config"].as<string>().c_str() );
    po::store( po::parse_config_file(config, config_options), *cfg );
  }
  po::notify(*cfg);

  if ( !cfg->count("decoder_config") || !cfg->count("input") ) { 
    cerr << cmdline_options << endl;
    return false;
  }
  if ( cfg->count("noup") && cfg->count("decode") ) {
    cerr << "You can't use 'noup' and 'decode' at once." << endl;
    return false;
  }
  if ( cfg->count("filter") && (*cfg)["filter"].as<string>() != "unique"
       && (*cfg)["filter"].as<string>() != "no" ) {
    cerr << "Wrong 'filter' type: '" << (*cfg)["filter"].as<string>() << "'." << endl;
  }
  #ifdef DTRAIN_DEBUG       
  if ( !cfg->count("test") ) {
    cerr << cmdline_options << endl;
    return false;
  }
  #endif
  return true;
}


// output formatting
ostream& _nopos( ostream& out ) { return out << resetiosflags( ios::showpos ); }
ostream& _pos( ostream& out ) { return out << setiosflags( ios::showpos ); }
ostream& _prec2( ostream& out ) { return out << setprecision(2); }
ostream& _prec5( ostream& out ) { return out << setprecision(5); }




/*
 * dtrain
 *
 */
int
main( int argc, char** argv )
{
  cout << setprecision( 5 );
  // handle most parameters
  po::variables_map cfg;
  if ( ! init(argc, argv, &cfg) ) exit(1); // something is wrong 
#ifdef DTRAIN_DEBUG
  if ( cfg.count("test") ) run_tests(); // run tests and exit 
#endif
  bool quiet = false;
  if ( cfg.count("quiet") ) quiet = true;
  bool verbose = false;  
  if ( cfg.count("verbose") ) verbose = true;
  bool noup = false;
  if ( cfg.count("noup") ) noup = true;
  const size_t k = cfg["kbest"].as<size_t>();
  const size_t N = cfg["ngrams"].as<size_t>(); 
  const size_t T = cfg["epochs"].as<size_t>();
  const size_t stop_after = cfg["stop_after"].as<size_t>();
  const string filter_type = cfg["filter"].as<string>();
  if ( !quiet ) {
    cout << endl << "dtrain" << endl << "Parameters:" << endl;
    cout << setw(25) << "k " << k << endl;
    cout << setw(25) << "N " << N << endl;
    cout << setw(25) << "T " << T << endl;
    if ( cfg.count("stop-after") )
      cout << setw(25) << "stop_after " << stop_after << endl;
    if ( cfg.count("weights") )
      cout << setw(25) << "weights " << cfg["weights"].as<string>() << endl;
    cout << setw(25) << "input " << "'" << cfg["input"].as<string>() << "'" << endl;
    cout << setw(25) << "filter " << "'" << filter_type << "'" << endl;
  }

  vector<string> wprint;
  if ( cfg.count("wprint") ) {
    boost::split( wprint, cfg["wprint"].as<string>(), boost::is_any_of(" ") );
  }

  // setup decoder, observer
  register_feature_functions();
  SetSilent(true);
  ReadFile ini_rf( cfg["decoder_config"].as<string>() );
  if ( !quiet )
    cout << setw(25) << "cdec cfg " << "'" << cfg["decoder_config"].as<string>() << "'" << endl;
  Decoder decoder( ini_rf.stream() );
  //KBestGetter observer( k, filter_type );
  MT19937 rng;
  KSampler observer( k, &rng );

  // scoring metric/scorer
  string scorer_str = cfg["scorer"].as<string>();
  double (*scorer)( NgramCounts&, const size_t, const size_t, size_t, vector<float> );
  if ( scorer_str == "bleu" ) {
    scorer = &bleu;
  } else if ( scorer_str == "stupid_bleu" ) {
    scorer = &stupid_bleu;
  } else if ( scorer_str == "smooth_bleu" ) {
    scorer = &smooth_bleu;
  } else if ( scorer_str == "approx_bleu" ) {
    scorer = &approx_bleu;
  } else {
    cerr << "Don't know scoring metric: '" << scorer_str << "', exiting." << endl;
    exit(1);
  }
  // for approx_bleu
  NgramCounts global_counts( N ); // counts for 1 best translations
  size_t global_hyp_len = 0;      // sum hypothesis lengths
  size_t global_ref_len = 0;      // sum reference lengths
  // this is all BLEU implmentations
  vector<float> bleu_weights; // we leave this empty -> 1/N; TODO? 
  if ( !quiet ) cout << setw(26) << "scorer '" << scorer_str << "'" << endl << endl;

  // init weights
  Weights weights;
  if ( cfg.count("weights") ) weights.InitFromFile( cfg["weights"].as<string>() );
  SparseVector<double> lambdas;
  weights.InitSparseVector( &lambdas );
  vector<double> dense_weights;

  // input
  if ( !quiet && !verbose )
    cout << "(a dot represents " << DTRAIN_DOTS << " lines of input)" << endl;
  string input_fn = cfg["input"].as<string>();
  ifstream input;
  if ( input_fn != "-" ) input.open( input_fn.c_str() );
  string in;
  vector<string> in_split; // input: src\tref\tpsg
  vector<string> ref_tok;  // tokenized reference
  vector<WordID> ref_ids;  // reference as vector of WordID
  string grammar_str;

  // buffer input for t > 0
  vector<string> src_str_buf;           // source strings, TODO? memory
  vector<vector<WordID> > ref_ids_buf;  // references as WordID vecs
  filtering_ostream grammar_buf;        // written to compressed file in /tmp
  // this is for writing the grammar buffer file
  grammar_buf.push( gzip_compressor() );
  char grammar_buf_tmp_fn[] = DTRAIN_TMP_DIR"/dtrain-grammars-XXXXXX";
  mkstemp( grammar_buf_tmp_fn );
  grammar_buf.push( file_sink(grammar_buf_tmp_fn, ios::binary | ios::trunc) );
  
  size_t sid = 0, in_sz = 99999999; // sentence id, input size
  double acc_1best_score = 0., acc_1best_model = 0.;
  vector<vector<double> > scores_per_iter;
  double max_score = 0.;
  size_t best_t = 0;
  bool next = false, stop = false;
  double score = 0.;
  size_t cand_len = 0;
  double overall_time = 0.;

  // for the perceptron/SVM; TODO as params
  double eta = 0.0005;
  double gamma = 0.01; // -> SVM
  lambdas.add_value( FD::Convert("__bias"), 0 );
  
  // for random sampling
  srand ( time(NULL) );


  for ( size_t t = 0; t < T; t++ ) // T epochs
  {

  time_t start, end;  
  time( &start );

  // actually, we need only need this if t > 0 FIXME
  ifstream grammar_file( grammar_buf_tmp_fn, ios_base::in | ios_base::binary );
  filtering_istream grammar_buf_in;
  grammar_buf_in.push( gzip_decompressor() );
  grammar_buf_in.push( grammar_file );

  // reset average scores
  acc_1best_score = acc_1best_model = 0.;
  
  // reset sentence counter
  sid = 0;
  
  if ( !quiet ) cout << "Iteration #" << t+1 << " of " << T << "." << endl;
  
  while( true )
  {

    // get input from stdin or file
    in.clear();
    next = stop = false; // next iteration, premature stop
    if ( t == 0 ) {    
      if ( input_fn == "-" ) {
        if ( !getline(cin, in) ) next = true;
      } else {
        if ( !getline(input, in) ) next = true; 
      }
    } else {
      if ( sid == in_sz ) next = true; // stop if we reach the end of our input
    }
    // stop after X sentences (but still iterate for those)
    if ( stop_after > 0 && stop_after == sid && !next ) stop = true;
    
    // produce some pretty output
    if ( !quiet && !verbose ) {
        if ( sid == 0 ) cout << " ";
        if ( (sid+1) % (DTRAIN_DOTS) == 0 ) {
            cout << ".";
            cout.flush();
        }
        if ( (sid+1) % (20*DTRAIN_DOTS) == 0) {
            cout << " " << sid+1 << endl;
            if ( !next && !stop ) cout << " ";
        }
        if ( stop ) {
          if ( sid % (20*DTRAIN_DOTS) != 0 ) cout << " " << sid << endl;
          cout << "Stopping after " << stop_after << " input sentences." << endl;
        } else {
          if ( next ) {
            if ( sid % (20*DTRAIN_DOTS) != 0 ) {
              cout << " " << sid << endl;
            }
          }
        }
    }
    
    // next iteration
    if ( next || stop ) break;

    // weights
    dense_weights.clear();
    weights.InitFromVector( lambdas );
    weights.InitVector( &dense_weights );
    decoder.SetWeights( dense_weights );

    if ( t == 0 ) {
      // handling input
      in_split.clear();
      boost::split( in_split, in, boost::is_any_of("\t") ); // in_split[0] is id
      // getting reference
      ref_tok.clear(); ref_ids.clear();
      boost::split( ref_tok, in_split[2], boost::is_any_of(" ") );
      register_and_convert( ref_tok, ref_ids );
      ref_ids_buf.push_back( ref_ids );
      // process and set grammar
      bool broken_grammar = true;
      for ( string::iterator ti = in_split[3].begin(); ti != in_split[3].end(); ti++ ) {
        if ( !isspace(*ti) ) {
          broken_grammar = false;
          break;
        }
      }
      if ( broken_grammar ) continue;
      grammar_str = boost::replace_all_copy( in_split[3], " __NEXT__RULE__ ", "\n" ) + "\n"; // FIXME copy, __
      grammar_buf << grammar_str << DTRAIN_GRAMMAR_DELIM << endl;
      decoder.SetSentenceGrammarFromString( grammar_str );
      // decode, kbest
      src_str_buf.push_back( in_split[1] );
      decoder.Decode( in_split[1], &observer );
    } else {
      // get buffered grammar
      grammar_str.clear();
      int i = 1;
      while ( true ) {
        string g;  
        getline( grammar_buf_in, g );
        if ( g == DTRAIN_GRAMMAR_DELIM ) break;
        grammar_str += g+"\n";
        i += 1;
      }
      decoder.SetSentenceGrammarFromString( grammar_str );
      // decode, kbest
      decoder.Decode( src_str_buf[sid], &observer );
    }

    // get kbest list
    KBestList* kb;
    //if ( ) { // TODO get from forest
      kb = observer.GetKBest();
    //}

    // scoring kbest
    if ( t > 0 ) ref_ids = ref_ids_buf[sid];
    for ( size_t i = 0; i < kb->GetSize(); i++ ) {
      NgramCounts counts = make_ngram_counts( ref_ids, kb->sents[i], N );
      // this is for approx bleu
      if ( scorer_str == "approx_bleu" ) {
        if ( i == 0 ) { // 'context of 1best translations'
          global_counts  += counts;
          global_hyp_len += kb->sents[i].size();
          global_ref_len += ref_ids.size();
          counts.reset();
          cand_len = 0;
        } else {
            cand_len = kb->sents[i].size();
        }
        NgramCounts counts_tmp = global_counts + counts;
        // TODO as param
        score = 0.9 * scorer( counts_tmp,
                              global_ref_len,
                              global_hyp_len + cand_len, N, bleu_weights );
      } else {
        // other scorers
        cand_len = kb->sents[i].size();
        score = scorer( counts,
                        ref_ids.size(),
                        kb->sents[i].size(), N, bleu_weights );
      }

      kb->scores.push_back( score );

      if ( i == 0 ) {
        acc_1best_score += score;
        acc_1best_model += kb->model_scores[i];
      }

      if ( verbose ) {
        if ( i == 0 ) cout << "'" << TD::GetString( ref_ids ) << "' [ref]" << endl;
        cout << _prec5 << _nopos << "[hyp " << i << "] " << "'" << TD::GetString( kb->sents[i] ) << "'";
        cout << " [SCORE=" << score << ",model="<< kb->model_scores[i] << "]" << endl;
        cout << kb->feats[i] << endl; // this is maybe too verbose
      }
    } // Nbest loop

    if ( verbose ) cout << endl;


    // UPDATE WEIGHTS
    if ( !noup ) {

      TrainingInstances pairs;

      sample_all_rand(kb, pairs);
      cout << pairs.size() << endl;
            
      for ( TrainingInstances::iterator ti = pairs.begin();
            ti != pairs.end(); ti++ ) {

        SparseVector<double> dv;
        if ( ti->first_score - ti->second_score < 0 ) {
          dv = ti->second - ti->first;
      //} else {
        //dv = ti->first - ti->second;
      //}
        dv.add_value( FD::Convert("__bias"), -1 );
        
        SparseVector<double> reg;
        reg = lambdas * ( 2 * gamma );
        dv -= reg;
        lambdas += dv * eta;

        if ( verbose ) {
          cout << "{{ f("<< ti->first_rank <<") > f(" << ti->second_rank << ") but g(i)="<< ti->first_score <<" < g(j)="<< ti->second_score << " so update" << endl;
          cout << " i  " << TD::GetString(kb->sents[ti->first_rank]) << endl;
          cout << "    " << kb->feats[ti->first_rank] << endl;
          cout << " j  " << TD::GetString(kb->sents[ti->second_rank]) << endl;
          cout << "    " << kb->feats[ti->second_rank] << endl; 
          cout << " diff vec: " << dv << endl;
          cout << " lambdas after update: " << lambdas << endl;
          cout << "}}" << endl;
        }

        } else {
            //if ( 0 ) {
            SparseVector<double> reg;
            reg = lambdas * ( gamma * 2 );
            lambdas += reg * ( -eta );
            //}
        }
      }

      //double l2 = lambdas.l2norm();
      //if ( l2 ) lambdas /= lambdas.l2norm();

    }

    ++sid;

  } // input loop

  if ( t == 0 ) in_sz = sid; // remember size (lines) of input

  // print some stats
  double avg_1best_score = acc_1best_score/(double)in_sz;
  double avg_1best_model = acc_1best_model/(double)in_sz;
  double avg_1best_score_diff, avg_1best_model_diff;
  if ( t > 0 ) {
    avg_1best_score_diff = avg_1best_score - scores_per_iter[t-1][0];
    avg_1best_model_diff = avg_1best_model - scores_per_iter[t-1][1];
  } else {
    avg_1best_score_diff = avg_1best_score;
    avg_1best_model_diff = avg_1best_model;
  }
  cout << _prec5 << _pos << "WEIGHTS" << endl;
  for (vector<string>::iterator it = wprint.begin(); it != wprint.end(); it++) {
    cout << setw(16) << *it << " = " << dense_weights[FD::Convert( *it )] << endl;
  }

  cout << "        ---" << endl;
  cout << _nopos << "      avg score: " << avg_1best_score;
  cout << _pos << " (" << avg_1best_score_diff << ")" << endl;
  cout << _nopos << "avg model score: " << avg_1best_model;
  cout << _pos << " (" << avg_1best_model_diff << ")" << endl;
  vector<double> remember_scores;
  remember_scores.push_back( avg_1best_score );
  remember_scores.push_back( avg_1best_model );
  scores_per_iter.push_back( remember_scores );
  if ( avg_1best_score > max_score ) {
    max_score = avg_1best_score;
    best_t = t;
  }

  // close open files
  if ( input_fn != "-" ) input.close();
  close( grammar_buf );
  grammar_file.close();

  time ( &end );
  double time_dif = difftime( end, start );
  overall_time += time_dif;
  if ( !quiet ) {
    cout << _prec2 << _nopos << "(time " << time_dif/60. << " min, ";
    cout << time_dif/(double)in_sz<< " s/S)" << endl;
  }
  
  if ( t+1 != T ) cout << endl;

  if ( noup ) break;

  } // outer loop

  unlink( grammar_buf_tmp_fn );
  if ( !noup ) {
    if ( !quiet ) cout << endl << "writing weights file '" << cfg["output"].as<string>() << "' ...";
    weights.WriteToFile( cfg["output"].as<string>(), true );
    if ( !quiet ) cout << "done" << endl;
  }
  
  if ( !quiet ) {
    cout << _prec5 << _nopos << endl << "---" << endl << "Best iteration: ";
    cout << best_t+1 << " [SCORE '" << scorer_str << "'=" << max_score << "]." << endl;
    cout << _prec2 << "This took " << overall_time/60. << " min." << endl;
  }

  return 0;
}

