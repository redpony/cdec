#include "Hildreth.h"
#include "sparse_vector.h"

using namespace std;

namespace Mira {
   vector<double> Hildreth::optimise (vector< SparseVector<double> >& a, vector<double>& b) {

    size_t i;
    int max_iter = 10000;
    double eps = 0.00000001;
    double zero = 0.000000000001;

    vector<double> alpha ( b.size() );
    vector<double> F ( b.size() );
    vector<double> kkt ( b.size() );

    double max_kkt = -1e100;

    size_t K = b.size();

    double A[K][K];
    bool is_computed[K];
    for ( i = 0; i < K; i++ )
    {
      A[i][i] = a[i].dot(a[i]);
      is_computed[i] = false;
    }

    int max_kkt_i = -1;


    for ( i = 0; i < b.size(); i++ )
    {
      F[i] = b[i];
      kkt[i] = F[i];
      if ( kkt[i] > max_kkt )
      {
        max_kkt = kkt[i];
        max_kkt_i = i;
      }
    }

    int iter = 0;
    double diff_alpha;
    double try_alpha;
    double add_alpha;

    while ( max_kkt >= eps && iter < max_iter )
    {

      diff_alpha = A[max_kkt_i][max_kkt_i] <= zero ? 0.0 : F[max_kkt_i]/A[max_kkt_i][max_kkt_i];
      try_alpha = alpha[max_kkt_i] + diff_alpha;
      add_alpha = 0.0;

      if ( try_alpha < 0.0 )
        add_alpha = -1.0 * alpha[max_kkt_i];
      else
        add_alpha = diff_alpha;

      alpha[max_kkt_i] = alpha[max_kkt_i] + add_alpha;

      if ( !is_computed[max_kkt_i] )
      {
        for ( i = 0; i < K; i++ )
        {
          A[i][max_kkt_i] = a[i].dot(a[max_kkt_i] ); // for version 1
          //A[i][max_kkt_i] = 0; // for version 1
          is_computed[max_kkt_i] = true;
        }
      }

      for ( i = 0; i < F.size(); i++ )
      {
        F[i] -= add_alpha * A[i][max_kkt_i];
        kkt[i] = F[i];
        if ( alpha[i] > zero )
          kkt[i] = abs ( F[i] );
      }
      max_kkt = -1e100;
      max_kkt_i = -1;
      for ( i = 0; i < F.size(); i++ )
        if ( kkt[i] > max_kkt )
        {
          max_kkt = kkt[i];
          max_kkt_i = i;
        }

      iter++;
    }

    return alpha;
  }

  vector<double> Hildreth::optimise (vector< SparseVector<double> >& a, vector<double>& b, double C) {

    size_t i;
    int max_iter = 10000;
    double eps = 0.00000001;
    double zero = 0.000000000001;

    vector<double> alpha ( b.size() );
    vector<double> F ( b.size() );
    vector<double> kkt ( b.size() );

    double max_kkt = -1e100;

    size_t K = b.size();

    double A[K][K];
    bool is_computed[K];
    for ( i = 0; i < K; i++ )
    {
      A[i][i] = a[i].dot(a[i]);
      is_computed[i] = false;
    }

    int max_kkt_i = -1;


    for ( i = 0; i < b.size(); i++ )
    {
      F[i] = b[i];
      kkt[i] = F[i];
      if ( kkt[i] > max_kkt )
      {
        max_kkt = kkt[i];
        max_kkt_i = i;
      }
    }

    int iter = 0;
    double diff_alpha;
    double try_alpha;
    double add_alpha;

    while ( max_kkt >= eps && iter < max_iter )
    {

      diff_alpha = A[max_kkt_i][max_kkt_i] <= zero ? 0.0 : F[max_kkt_i]/A[max_kkt_i][max_kkt_i];
      try_alpha = alpha[max_kkt_i] + diff_alpha;
      add_alpha = 0.0;

      if ( try_alpha < 0.0 )
        add_alpha = -1.0 * alpha[max_kkt_i];
      else if (try_alpha > C)
				add_alpha = C - alpha[max_kkt_i];
      else
        add_alpha = diff_alpha;

      alpha[max_kkt_i] = alpha[max_kkt_i] + add_alpha;

      if ( !is_computed[max_kkt_i] )
      {
        for ( i = 0; i < K; i++ )
        {
          A[i][max_kkt_i] = a[i].dot(a[max_kkt_i] ); // for version 1
          //A[i][max_kkt_i] = 0; // for version 1
          is_computed[max_kkt_i] = true;
        }
      }

      for ( i = 0; i < F.size(); i++ )
      {
        F[i] -= add_alpha * A[i][max_kkt_i];
        kkt[i] = F[i];
        if (alpha[i] > C - zero)
					kkt[i]=-kkt[i];
				else if (alpha[i] > zero)
					kkt[i] = abs(F[i]);

      }
      max_kkt = -1e100;
      max_kkt_i = -1;
      for ( i = 0; i < F.size(); i++ )
        if ( kkt[i] > max_kkt )
        {
          max_kkt = kkt[i];
          max_kkt_i = i;
        }

      iter++;
    }

    return alpha;
  }
}
