#ifndef _CROW_FRACTAL_SERVER_COMPLEX_ARRAY_
#define _CROW_FRACTAL_SERVER_COMPLEX_ARRAY_

// Uncomment this line to use Eigen to implement ComplexArray.
// #define _IMPLEMENT_COMPLEX_ARRAY_WITH_EIGEN_

#ifdef _IMPLEMENT_COMPLEX_ARRAY_WITH_EIGEN_
#include "complex_array_eigen.h"
#else
#include "complex_array_hand_rolled.h"
#endif // _IMPLEMENT_COMPLEX_ARRAY_WITH_EIGEN_

#endif // _CROW_FRACTAL_SERVER_COMPLEX_
