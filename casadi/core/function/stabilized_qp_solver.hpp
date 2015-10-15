/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010-2014 Joel Andersson, Joris Gillis, Moritz Diehl,
 *                            K.U. Leuven. All rights reserved.
 *    Copyright (C) 2011-2014 Greg Horn
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef CASADI_STABILIZED_QP_HPP
#define CASADI_STABILIZED_QP_HPP

#include "function.hpp"
#include "qp_solver.hpp"

/** \defgroup StabilizedQpSolver_doc

    Solves the following strictly convex problem:

    \verbatim
    min          1/2 x' H x + g' x
    x

    subject to
    LBA <= A x <= UBA
    LBX <= x   <= UBX

    with :
    H sparse (n x n) positive definite
    g dense  (n x 1)

    n: number of decision variables (x)
    nc: number of constraints (A)

    \endverbatim

    If H is not positive-definite, the solver should throw an error.

*/

namespace casadi {
#ifndef SWIG

  /// Input arguments of a QP problem [stabilizedQpIn]
  enum StabilizedQpSolverInput {
    /// The square matrix H: sparse, (n x n). Only the lower triangular part is actually used.
    /// The matrix is assumed to be symmetrical. [h]
    STABILIZED_QP_SOLVER_H,
    /// The vector g: dense,  (n x 1) [g]
    STABILIZED_QP_SOLVER_G,
    /// The matrix A: sparse, (nc x n) - product with x must be dense. [a]
    STABILIZED_QP_SOLVER_A,
    /// dense, (nc x 1) [lba]
    STABILIZED_QP_SOLVER_LBA,
    /// dense, (nc x 1) [uba]
    STABILIZED_QP_SOLVER_UBA,
    /// dense, (n x 1) [lbx]
    STABILIZED_QP_SOLVER_LBX,
    /// dense, (n x 1) [ubx]
    STABILIZED_QP_SOLVER_UBX,
    /// dense, (n x 1) [x0]
    STABILIZED_QP_SOLVER_X0,
    /// dense [lam_x0]
    STABILIZED_QP_SOLVER_LAM_X0,
    /// dense (1 x 1) [muR]
    STABILIZED_QP_SOLVER_MUR,
    /// dense (nc x 1) [muE]
    STABILIZED_QP_SOLVER_MUE,
    /// dense (nc x 1) [mu]
    STABILIZED_QP_SOLVER_MU,
    STABILIZED_QP_SOLVER_NUM_IN};
#endif // SWIG

  // Forward declaration of internal class
  class StabilizedQpSolverInternal;

  /** \brief StabilizedQpSolver


      @copydoc StabilizedQpSolver_doc

      \generalsection{StabilizedQpSolver}
      \pluginssection{StabilizedQpSolver}

      \author Joel Andersson
      \date 2010
  */
  class CASADI_EXPORT StabilizedQpSolver : public Function {
  public:

    /// Default constructor
    StabilizedQpSolver();

    /** \brief Constructor (new syntax, includes initialization)
     *  \param solver \pluginargument{StabilizedQpSolver}
     *  \param st Problem structure
     *  \copydoc scheme_QPStruct
     */
    StabilizedQpSolver(const std::string& name, const std::string& solver,
                       const std::map<std::string, Sparsity>& st, const Dict& opts=Dict());

    /// Access functions of the node
    StabilizedQpSolverInternal* operator->();
    const StabilizedQpSolverInternal* operator->() const;

    /// Check if a plugin is available
    static bool hasPlugin(const std::string& name);

    /// Explicitly load a plugin dynamically
    static void loadPlugin(const std::string& name);

    /// Get solver specific documentation
    static std::string doc(const std::string& name);

    /** Generate native code in the interfaced language for debugging */
    void generateNativeCode(const std::string &filename) const;

    /// Check if a particular cast is allowed
    static bool test_cast(const SharedObjectNode* ptr);
  };

} // namespace casadi

#endif // CASADI_STABILIZED_QP_HPP

