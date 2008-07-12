// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2006-2008 Benoit Jacob <jacob@math.jussieu.fr>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#include "main.h"
#include <Eigen/QR>

template<typename Derived1, typename Derived2>
bool areNotApprox(const MatrixBase<Derived1>& m1, const MatrixBase<Derived2>& m2, typename Derived1::RealScalar epsilon = precision<typename Derived1::RealScalar>())
{
  return !((m1-m2).matrixNorm() < epsilon * std::max(m1.matrixNorm(), m2.matrixNorm()));
}

template<typename MatrixType> void product(const MatrixType& m)
{
  /* this test covers the following files:
     Identity.h Product.h
  */

  typedef typename MatrixType::Scalar Scalar;
  typedef typename NumTraits<Scalar>::FloatingPoint FloatingPoint;
  typedef Matrix<Scalar, MatrixType::RowsAtCompileTime, 1> RowVectorType;
  typedef Matrix<Scalar, MatrixType::ColsAtCompileTime, 1> ColVectorType;
  typedef Matrix<Scalar, MatrixType::RowsAtCompileTime, MatrixType::RowsAtCompileTime> RowSquareMatrixType;
  typedef Matrix<Scalar, MatrixType::ColsAtCompileTime, MatrixType::ColsAtCompileTime> ColSquareMatrixType;
  typedef Matrix<Scalar, MatrixType::RowsAtCompileTime, MatrixType::ColsAtCompileTime,
                         MatrixType::RowsAtCompileTime, MatrixType::ColsAtCompileTime,
                         MatrixType::Flags&RowMajorBit ? 0 : RowMajorBit> OtherMajorMatrixType;

  int rows = m.rows();
  int cols = m.cols();

  // this test relies a lot on Random.h, and there's not much more that we can do
  // to test it, hence I consider that we will have tested Random.h
  MatrixType m1 = MatrixType::random(rows, cols),
             m2 = MatrixType::random(rows, cols),
             m3(rows, cols),
             mzero = MatrixType::zero(rows, cols);
  RowSquareMatrixType
             identity = RowSquareMatrixType::identity(rows, rows),
             square = RowSquareMatrixType::random(rows, rows),
             res = RowSquareMatrixType::random(rows, rows);
  ColSquareMatrixType
             square2 = ColSquareMatrixType::random(cols, cols),
             res2 = ColSquareMatrixType::random(cols, cols);
  RowVectorType v1 = RowVectorType::random(rows),
             v2 = RowVectorType::random(rows),
             vzero = RowVectorType::zero(rows);
  ColVectorType vc2 = ColVectorType::random(cols), vcres;
  OtherMajorMatrixType tm1 = m1;

  Scalar s1 = ei_random<Scalar>();

  int r = ei_random<int>(0, rows-1),
      c = ei_random<int>(0, cols-1);

  // begin testing Product.h: only associativity for now
  // (we use Transpose.h but this doesn't count as a test for it)
  VERIFY_IS_APPROX((m1*m1.transpose())*m2,  m1*(m1.transpose()*m2));
  m3 = m1;
  m3 *= m1.transpose() * m2;
  VERIFY_IS_APPROX(m3,                      m1 * (m1.transpose()*m2));
  VERIFY_IS_APPROX(m3,                      m1.lazy() * (m1.transpose()*m2));

  // continue testing Product.h: distributivity
  VERIFY_IS_APPROX(square*(m1 + m2),        square*m1+square*m2);
  VERIFY_IS_APPROX(square*(m1 - m2),        square*m1-square*m2);

  // continue testing Product.h: compatibility with ScalarMultiple.h
  VERIFY_IS_APPROX(s1*(square*m1),          (s1*square)*m1);
  VERIFY_IS_APPROX(s1*(square*m1),          square*(m1*s1));

  // again, test operator() to check const-qualification
  s1 += (square.lazy() * m1)(r,c);

  // test Product.h together with Identity.h
  VERIFY_IS_APPROX(v1,                      identity*v1);
  VERIFY_IS_APPROX(v1.transpose(),          v1.transpose() * identity);
  // again, test operator() to check const-qualification
  VERIFY_IS_APPROX(MatrixType::identity(rows, cols)(r,c), static_cast<Scalar>(r==c));

  if (rows!=cols)
    VERIFY_RAISES_ASSERT(m3 = m1*m1);

  // test the previous tests were not screwed up because operator* returns 0
  // (we use the more accurate default epsilon)
  if (NumTraits<Scalar>::HasFloatingPoint && std::min(rows,cols)>1)
  {
    VERIFY(areNotApprox(m1.transpose()*m2,m2.transpose()*m1));
  }

  // test optimized operator+= path
  res = square;
  res += (m1 * m2.transpose()).lazy();
  VERIFY_IS_APPROX(res, square + m1 * m2.transpose());
  if (NumTraits<Scalar>::HasFloatingPoint && std::min(rows,cols)>1)
  {
    VERIFY(areNotApprox(res,square + m2 * m1.transpose()));
  }
  vcres = vc2;
  vcres += (m1.transpose() * v1).lazy();
  VERIFY_IS_APPROX(vcres, vc2 + m1.transpose() * v1);
  tm1 = m1;
  VERIFY_IS_APPROX(tm1.transpose() * v1, m1.transpose() * v1);
  VERIFY_IS_APPROX(v1.transpose() * tm1, v1.transpose() * m1);

  // test submatrix and matrix/vector product
  for (int i=0; i<rows; ++i)
    res.row(i) = m1.row(i) * m2.transpose();
  VERIFY_IS_APPROX(res, m1 * m2.transpose());
  // the other way round:
  for (int i=0; i<rows; ++i)
    res.col(i) = m1 * m2.transpose().col(i);
  VERIFY_IS_APPROX(res, m1 * m2.transpose());

  res2 = square2;
  res2 += (m1.transpose() * m2).lazy();
  VERIFY_IS_APPROX(res2, square2 + m1.transpose() * m2);
  if (NumTraits<Scalar>::HasFloatingPoint && std::min(rows,cols)>1)
  {
    VERIFY(areNotApprox(res2,square2 + m2.transpose() * m1));
  }
}

void test_product()
{
  for(int i = 0; i < g_repeat; i++) {
    CALL_SUBTEST( product(Matrix3i()) );
    CALL_SUBTEST( product(Matrix<float, 3, 2>()) );
    CALL_SUBTEST( product(Matrix4d()) );
    CALL_SUBTEST( product(Matrix4f()) );
    CALL_SUBTEST( product(MatrixXf(3,5)) );
    CALL_SUBTEST( product(MatrixXi(28,39)) );
  }
  for(int i = 0; i < g_repeat; i++) {
    CALL_SUBTEST( product(MatrixXf(ei_random<int>(1,320), ei_random<int>(1,320))) );
    CALL_SUBTEST( product(MatrixXd(ei_random<int>(1,320), ei_random<int>(1,320))) );
    CALL_SUBTEST( product(MatrixXi(ei_random<int>(1,256), ei_random<int>(1,256))) );
    CALL_SUBTEST( product(MatrixXcf(ei_random<int>(1,50), ei_random<int>(1,50))) );
    #ifndef EIGEN_DEFAULT_TO_ROW_MAJOR
    CALL_SUBTEST( product(Matrix<float,Dynamic,Dynamic,Dynamic,Dynamic,RowMajorBit>(ei_random<int>(1,320), ei_random<int>(1,320))) );
    #else
    CALL_SUBTEST( product(Matrix<float,Dynamic,Dynamic,Dynamic,Dynamic,0>(ei_random<int>(1,320), ei_random<int>(1,320))) );
    #endif
  }
}
