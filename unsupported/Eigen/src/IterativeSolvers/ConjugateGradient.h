// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2011 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_CONJUGATE_GRADIENT_H
#define EIGEN_CONJUGATE_GRADIENT_H

namespace internal {

/** \internal Low-level conjugate gradient algorithm
  * \param mat The matrix A
  * \param rhs The right hand side vector b
  * \param x On input and initial solution, on output the computed solution.
  * \param precond A preconditioner being able to efficiently solve for an
  *                approximation of Ax=b (regardless of b)
  * \param iters On input the max number of iteration, on output the number of performed iterations.
  * \param tol_error On input the tolerance error, on output an estimation of the relative error.
  */
template<typename MatrixType, typename Rhs, typename Dest, typename Preconditioner>
void conjugate_gradient(const MatrixType& mat, const Rhs& rhs, Dest& x,
                        const Preconditioner& precond, int& iters,
                        typename Dest::RealScalar& tol_error)
{
  using std::sqrt;
  using std::abs;
  typedef typename Dest::RealScalar RealScalar;
  typedef typename Dest::Scalar Scalar;
  typedef Dest VectorType;
  
  RealScalar tol = tol_error;
  int maxIters = iters;
  
  int n = mat.cols();
  VectorType residual = rhs - mat * x; //initial residual
  VectorType p(n);

  p = precond.solve(residual);      //initial search direction

  VectorType z(n), tmp(n);
  RealScalar absNew = internal::real(residual.dot(p));  // the square of the absolute value of r scaled by invM
  RealScalar absInit = absNew;          // the initial absolute value
  
  int i = 0;
  while ((i < maxIters) && (absNew > tol*tol*absInit))
  {
    tmp.noalias() = mat * p;              // the bottleneck of the algorithm

    Scalar alpha = absNew / p.dot(tmp);   // the amount we travel on dir
    x += alpha * p;                       // update solution
    residual -= alpha * tmp;              // update residue
    z = precond.solve(residual);          // approximately solve for "A z = residual"

    RealScalar absOld = absNew;
    absNew = internal::real(residual.dot(z));     // update the absolute value of r
    RealScalar beta = absNew / absOld;            // calculate the Gram-Schmidit value used to create the new search direction
    p = z + beta * p;                             // update search direction
    i++;
  }

  tol_error = sqrt(abs(absNew / absInit));
  iters = i;
}

}

namespace internal {

template<typename CG, typename Rhs, typename Guess>
class conjugate_gradient_solve_retval_with_guess;

}

/** \brief A conjugate gradient solver for sparse self-adjoint problems
  *
  * This class allows to solve for A.x = b sparse linear problems using a conjugate gradient algorithm.
  * The sparse matrix A must be selfadjoint. The vectors x and b can be either dense or sparse.
  *
  * \tparam _MatrixType the type of the sparse matrix A, can be a dense or a sparse matrix.
  * \tparam _UpLo the triangular part that will be used for the computations. It can be Lower
  *               or Upper. Default is Lower.
  * \tparam _Preconditioner the type of the preconditioner. Default is DiagonalPreconditioner
  *
  * The maximal number of iterations and tolerance value can be controlled via the setMaxIterations()
  * and setTolerance() methods. The default are 1000 max iterations and NumTraits<Scalar>::epsilon()
  * for the tolerance.
  * 
  * This class can be used as the direct solver classes. Here is a typical usage example:
  * \code
  * int n = 10000;
  * VectorXd x(n), b(n);
  * SparseMatrix<double> A(n,n);
  * // fill A and b
  * ConjugateGradient<SparseMatrix<double> > cg;
  * cg(A);
  * x = cg.solve(b);
  * std::cout << "#iterations:     " << cg.iterations() << std::endl;
  * std::cout << "estimated error: " << cg.error()      << std::endl;
  * // update b, and solve again
  * x = cg.solve(b);
  * \endcode
  * 
  * By default the iterations start with x=0 as an initial guess of the solution.
  * One can control the start using the solveWithGuess() method. Here is a step by
  * step execution example starting with a random guess and printing the evolution
  * of the estimated error:
  * * \code
  * x = VectorXd::Random(n);
  * cg.setMaxIterations(1);
  * int i = 0;
  * do {
  *   x = cg.solveWithGuess(b,x);
  *   std::cout << i << " : " << cg.error() << std::endl;
  *   ++i;
  * } while (cg.info()!=Success && i<100);
  * \endcode
  * Note that such a step by step excution is slightly slower.
  * 
  * \sa class SimplicialCholesky, DiagonalPreconditioner, IdentityPreconditioner
  */
template< typename _MatrixType, int _UpLo=Lower,
          typename _Preconditioner = DiagonalPreconditioner<typename _MatrixType::Scalar> >
class ConjugateGradient
{
public:
  typedef _MatrixType MatrixType;
  typedef typename MatrixType::Scalar Scalar;
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::RealScalar RealScalar;
  typedef _Preconditioner Preconditioner;

  enum {
    UpLo = _UpLo
  };

public:

  /** Default constructor. */
  ConjugateGradient()
    : mp_matrix(0)
  {
    init();
  }

  /** Initialize the solver with matrix \a A for further \c Ax=b solving.
    * 
    * This constructor is a shortcut for the default constructor followed
    * by a call to compute().
    * 
    * \warning this class stores a reference to the matrix A as well as some
    * precomputed values that depend on it. Therefore, if \a A is changed
    * this class becomes invalid. Call compute() to update it with the new
    * matrix A, or modify a copy of A.
    */
  ConjugateGradient(const MatrixType& A)
  {
    init();
    compute(A);
  }

  ~ConjugateGradient() {}

  /** Initializes the iterative solver with the matrix \a A for further solving \c Ax=b problems.
    *
    * Currently, this function mostly initialized/compute the preconditioner. In the future
    * we might, for instance, implement column reodering for faster matrix vector products.
    *
    * \warning this class stores a reference to the matrix A as well as some
    * precomputed values that depend on it. Therefore, if \a A is changed
    * this class becomes invalid. Call compute() to update it with the new
    * matrix A, or modify a copy of A.
    */
  ConjugateGradient& compute(const MatrixType& A)
  {
    mp_matrix = &A;
    m_preconditioner.compute(A);
    m_isInitialized = true;
    return *this;
  }

  /** \internal */
  Index rows() const { return mp_matrix->rows(); }
  /** \internal */
  Index cols() const { return mp_matrix->cols(); }

  /** \returns the tolerance threshold used by the stopping criteria */
  RealScalar tolerance() const { return m_tolerance; }
  
  /** Sets the tolerance threshold used by the stopping criteria */
  ConjugateGradient& setTolerance(RealScalar tolerance)
  {
    m_tolerance = tolerance;
    return *this;
  }

  /** \returns a read-write reference to the preconditioner for custom configuration. */
  Preconditioner& preconditioner() { return m_preconditioner; }
  
  /** \returns a read-only reference to the preconditioner. */
  const Preconditioner& preconditioner() const { return m_preconditioner; }

  /** \returns the max number of iterations */
  int maxIterations() const { return m_maxIterations; }
  
  /** Sets the max number of iterations */
  ConjugateGradient& setMaxIterations(int maxIters)
  {
    m_maxIterations = maxIters;
    return *this;
  }

  /** \returns the number of iterations performed during the last solve */
  int iterations() const
  {
    eigen_assert(m_isInitialized && "ConjugateGradient is not initialized.");
    return m_iterations;
  }

  /** \returns the tolerance error reached during the last solve */
  RealScalar error() const
  {
    eigen_assert(m_isInitialized && "ConjugateGradient is not initialized.");
    return m_error;
  }

  /** \returns the solution x of \f$ A x = b \f$ using the current decomposition of A.
    *
    * \sa compute()
    */
  template<typename Rhs> inline const internal::solve_retval<ConjugateGradient, Rhs>
  solve(const MatrixBase<Rhs>& b) const
  {
    eigen_assert(m_isInitialized && "ConjugateGradient is not initialized.");
    eigen_assert(rows()==b.rows()
              && "ConjugateGradient::solve(): invalid number of rows of the right hand side matrix b");
    return internal::solve_retval<ConjugateGradient, Rhs>(*this, b.derived());
  }

  /** \returns the solution x of \f$ A x = b \f$ using the current decomposition of A
    * \a x0 as an initial solution.
    *
    * \sa compute()
    */
  template<typename Rhs,typename Guess>
  inline const internal::conjugate_gradient_solve_retval_with_guess<ConjugateGradient, Rhs, Guess>
  solveWithGuess(const MatrixBase<Rhs>& b, const Guess& x0) const
  {
    eigen_assert(m_isInitialized && "ConjugateGradient is not initialized.");
    eigen_assert(rows()==b.rows()
              && "ConjugateGradient::solve(): invalid number of rows of the right hand side matrix b");
    return internal::conjugate_gradient_solve_retval_with_guess
            <ConjugateGradient, Rhs, Guess>(*this, b.derived(), x0);
  }

  /** \returns Success if the iterations converged, and NoConvergence otherwise. */
  ComputationInfo info() const
  {
    eigen_assert(m_isInitialized && "ConjugateGradient is not initialized.");
    return m_info;
  }

  /** \internal */
  template<typename Rhs,typename Dest>
  void _solve(const Rhs& b, Dest& x) const
  {
    m_iterations = m_maxIterations;
    m_error = m_tolerance;

    internal::conjugate_gradient(mp_matrix->template selfadjointView<UpLo>(), b, x,
                                 m_preconditioner, m_iterations, m_error);
    
    m_isInitialized = true;
    m_info = m_error <= m_tolerance ? Success : NoConvergence;
  }

protected:
  void init()
  {
    m_isInitialized = false;
    m_maxIterations = 1000;
    m_tolerance = NumTraits<Scalar>::epsilon();
  }
  const MatrixType* mp_matrix;
  Preconditioner m_preconditioner;

  int m_maxIterations;
  RealScalar m_tolerance;
  
  mutable RealScalar m_error;
  mutable int m_iterations;
  mutable ComputationInfo m_info;
  mutable bool m_isInitialized;
};


namespace internal {

  template<typename _MatrixType, int _UpLo, typename _Preconditioner, typename Rhs>
struct solve_retval<ConjugateGradient<_MatrixType,_UpLo,_Preconditioner>, Rhs>
  : solve_retval_base<ConjugateGradient<_MatrixType,_UpLo,_Preconditioner>, Rhs>
{
  typedef ConjugateGradient<_MatrixType,_UpLo,_Preconditioner> Dec;
  EIGEN_MAKE_SOLVE_HELPERS(Dec,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    dst.setZero();
    dec()._solve(rhs(),dst);
  }
};

template<typename CG, typename Rhs, typename Guess>
class conjugate_gradient_solve_retval_with_guess
  : solve_retval_base<CG, Rhs>
{
  typedef Eigen::internal::solve_retval_base<CG,Rhs> Base;
  using Base::dec;
  using Base::rhs;

    conjugate_gradient_solve_retval_with_guess(const CG& cg, const Rhs& rhs, const Guess guess)
      : Base(cg, rhs), m_guess(guess)
    {}

    template<typename Dest> void evalTo(Dest& dst) const
    {
      dst = m_guess;
      dec()._solve(rhs(), dst);
    }
  protected:
    const Guess& m_guess;
    
};

}

#endif // EIGEN_CONJUGATE_GRADIENT_H