// The following code inverts the matrix input using LU-decomposition with
// backsubstitution of unit vectors. Reference: Numerical Recipies in C, 2nd
// ed., by Press, Teukolsky, Vetterling & Flannery. 
// Code written by Fredrik Orderud.
// http://www.crystalclearsoftware.com/cgi-bin/boost_wiki/wiki.pl?LU_Matrix_Inversion

#ifndef INVERT_MATRIX_HPP
#define INVERT_MATRIX_HPP

// REMEMBER to update "lu.hpp" header includes from boost-CVS
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/vector_proxy.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/triangular.hpp>
#include <boost/numeric/ublas/lu.hpp>
#include <boost/numeric/ublas/io.hpp>

namespace ublas = boost::numeric::ublas;

/* Matrix inversion routine.
   Uses lu_factorize and lu_substitute in uBLAS to invert a matrix */
template<class T>
bool invert_matrix(const ublas::matrix<T>& input, ublas::matrix<T>& inverse) 
{
    using namespace boost::numeric::ublas;
    typedef permutation_matrix<std::size_t> pmatrix;
    // create a working copy of the input
    matrix<T> A(input);
    // create a permutation matrix for the LU-factorization
    pmatrix pm(A.size1());

    // perform LU-factorization
    int res = lu_factorize(A,pm);
    if( res != 0 ) return false;

    // create identity matrix of "inverse"
    inverse.assign(ublas::identity_matrix<T>(A.size1()));

    // backsubstitute to get the inverse
    lu_substitute(A, pm, inverse);
    
    return true;
}

#endif //INVERT_MATRIX_HPP
