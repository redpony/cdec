//
// Minimises given functional using the projected gradient method. Based on
// algorithm and demonstration example in Linear and Nonlinear Programming,
// Luenberger and Ye, 3rd ed., p 370.
//

#include "invert.hh"
#include <iostream>

using namespace std;

double 
f(double x1, double x2, double x3, double x4)
{
    return x1 * x1 + x2 * x2 + x3 * x3 + x4 * x4 - 2 * x1 - 3 * x4;
}

ublas::vector<double> 
g(double x1, double x2, double x3, double x4)
{
    ublas::vector<double> v(4);
    v(0) = 2 * x1 - 2;
    v(1) = 2 * x2;
    v(2) = 2 * x3;
    v(3) = 2 * x4 - 3;
    return v;
}

ublas::matrix<double> 
activeConstraints(double x1, double x2, double x3, double x4)
{
    int n = 2;
    if (x1 == 0) ++n;
    if (x2 == 0) ++n;
    if (x3 == 0) ++n;
    if (x4 == 0) ++n;

    ublas::matrix<double> a(n,4);
    a(0, 0) = 2; a(0, 1) = 1; a(0, 2) = 1; a(0, 3) = 4;
    a(1, 0) = 1; a(1, 1) = 1; a(1, 2) = 2; a(1, 3) = 1;

    int c = 2;
    if (x1 == 0) a(c++, 0) = 1;
    if (x2 == 0) a(c++, 1) = 1;
    if (x3 == 0) a(c++, 2) = 1;
    if (x4 == 0) a(c++, 3) = 1;

    return a;
}

ublas::matrix<double>
projection(const ublas::matrix<double> &a)
{
    ublas::matrix<double> aT = ublas::trans(a);
    ublas::matrix<double> inv(a.size1(), a.size1());
    bool ok = invert_matrix(ublas::matrix<double>(ublas::prod(a, aT)), inv);
    assert(ok && "Failed to invert matrix");
    return ublas::identity_matrix<double>(4) - 
        ublas::prod(aT, ublas::matrix<double>(ublas::prod(inv, a)));
}

int main(int argc, char *argv[])
{
    double x1 = 2, x2 = 2, x3 = 1, x4 = 0;

    double fval = f(x1, x2, x3, x4);
    cout << "f = " << fval << endl;
    ublas::vector<double> grad = g(x1, x2, x3, x4);
    cout << "g = " << grad << endl;
    ublas::matrix<double> A = activeConstraints(x1, x2, x3, x4);
    cout << "A = " << A << endl;
    ublas::matrix<double> P = projection(A);
    cout << "P = " << P << endl;
    // the direction of movement
    ublas::vector<double> d = prod(P, grad);
    cout << "d = " << (d / d(0)) << endl;

    // special case for d = 0

    // next solve for limits on the line search

    // then use golden rule technique between these values (if bounded)

    // or simple Armijo's rule technique

    return 0;
}
