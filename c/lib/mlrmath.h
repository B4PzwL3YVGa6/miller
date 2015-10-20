#ifndef MLRMATH_H
#define MLRMATH_H

void mlr_get_real_symmetric_eigensystem(
	double  matrix[2][2],      // Input
	double* peigenvalue_1,     // Output: dominant eigenvalue
	double* peigenvalue_2,     // Output: less-dominant eigenvalue
	double  eigenvector_1[2],  // Output: corresponding to dominant eigenvalue
	double  eigenvector_2[2]); // Output: corresponding to less-dominant eigenvalue

double qnorm(double x);

double invqnorm(double x);

void mlr_logistic_regression(double* xs, double* ys, int n, double* pm, double* pb);

#endif // MLRMATH_H
