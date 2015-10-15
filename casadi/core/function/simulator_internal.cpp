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


#include "simulator_internal.hpp"
#include "integrator_internal.hpp"
#include "../std_vector_tools.hpp"
#include "sx_function.hpp"

INPUTSCHEME(IntegratorInput)

using namespace std;
namespace casadi {


  SimulatorInternal::SimulatorInternal(const std::string& name, const Integrator& integrator,
                                       const Function& output_fcn, const DMatrix& grid)
    : FunctionInternal(name), integrator_(integrator), output_fcn_(output_fcn), grid_(grid.data()) {

    casadi_assert_message(grid.iscolumn(),
                          "Simulator::Simulator: grid must be a column vector, but got "
                          << grid.dim());
    casadi_assert_message(grid.isdense(),
                          "Simulator::Simulator: grid must be dense, but got "
                          << grid.dim());
    addOption("monitor",      OT_STRINGVECTOR, GenericType(),  "", "initial|step", true);
    ischeme_ = IOScheme(SCHEME_IntegratorInput);
  }

  SimulatorInternal::~SimulatorInternal() {
  }

  void SimulatorInternal::init() {
    if (!grid_.empty()) {
      casadi_assert_message(isNonDecreasing(grid_),
                            "The supplied time grid must be non-decreasing.");

      // Create new integrator object
      Function I = Function::create(integrator_->create(integrator_.name(),
                                                        integrator_->f_, integrator_->g_));
      I.setOption(integrator_.dictionary());

      // Let the integration time start from the first point of the time grid.
      I.setOption("t0", grid_[0]);
      // Let the integration time stop at the last point of the time grid.
      I.setOption("tf", grid_[grid_.size()-1]);
      I.init();

      // Overwrite
      integrator_ = shared_cast<Integrator>(I);
    }

    // Generate an output function if there is none (returns the whole state)
    if (output_fcn_.isNull()) {
      SX t = SX::sym("t");
      SX x = SX::sym("x", integrator_.input(INTEGRATOR_X0).sparsity());
      SX z = SX::sym("z", integrator_.input(INTEGRATOR_Z0).sparsity());
      SX p = SX::sym("p", integrator_.input(INTEGRATOR_P).sparsity());

      vector<SX> arg(DAE_NUM_IN);
      arg[DAE_T] = t;
      arg[DAE_X] = x;
      arg[DAE_Z] = z;
      arg[DAE_P] = p;

      vector<SX> out(INTEGRATOR_NUM_OUT);
      out[INTEGRATOR_XF] = x;
      out[INTEGRATOR_ZF] = z;

      // Create the output function
      output_fcn_ = SX::fun("ofcn", arg, out);
      oscheme_ = IOScheme(SCHEME_IntegratorOutput);
    }

    // Allocate inputs
    ibuf_.resize(INTEGRATOR_NUM_IN);
    for (int i=0; i<INTEGRATOR_NUM_IN; ++i) {
      input(i) = integrator_.input(i);
    }

    // Allocate outputs
    obuf_.resize(output_fcn_->n_out());
    for (int i=0; i<n_out(); ++i) {
      output(i) = Matrix<double>::zeros(output_fcn_.output(i).numel(), grid_.size());
      if (!output_fcn_.output(i).isempty()) {
        casadi_assert_message(output_fcn_.output(i).iscolumn(),
                              "SimulatorInternal::init: Output function output #" << i
                              << " has shape " << output_fcn_.output(i).dim()
                              << ", while a column-matrix shape is expected.");
      }
    }

    casadi_assert_message(output_fcn_.input(DAE_T).numel() <=1,
                          "SimulatorInternal::init: output_fcn DAE_T argument must be "
                          "scalar or empty, but got " << output_fcn_.input(DAE_T).dim());

    casadi_assert_message(
        output_fcn_.input(DAE_P).isempty() ||
        integrator_.input(INTEGRATOR_P).sparsity() == output_fcn_.input(DAE_P).sparsity(),
        "SimulatorInternal::init: output_fcn DAE_P argument must be empty or"
        << " have dimension " << integrator_.input(INTEGRATOR_P).dim()
        << ", but got " << output_fcn_.input(DAE_P).dim());

    casadi_assert_message(
        output_fcn_.input(DAE_X).isempty() ||
        integrator_.input(INTEGRATOR_X0).sparsity() == output_fcn_.input(DAE_X).sparsity(),
        "SimulatorInternal::init: output_fcn DAE_X argument must be empty or have dimension "
        << integrator_.input(INTEGRATOR_X0).dim()
        << ", but got " << output_fcn_.input(DAE_X).dim());

    // Call base class method
    FunctionInternal::init();

    // Output iterators
    output_its_.resize(n_out());
  }

  void SimulatorInternal::evaluate() {

    // Pass the parameters and initial state
    integrator_.setInput(input(INTEGRATOR_X0), INTEGRATOR_X0);
    integrator_.setInput(input(INTEGRATOR_Z0), INTEGRATOR_Z0);
    integrator_.setInput(input(INTEGRATOR_P), INTEGRATOR_P);

    if (monitored("initial")) {
      userOut() << "SimulatorInternal::evaluate: initial condition:" << std::endl;
      userOut() << " x0     = "  << input(INTEGRATOR_X0) << std::endl;
      userOut() << " z0     = "  << input(INTEGRATOR_Z0) << std::endl;
      userOut() << " p      = "   << input(INTEGRATOR_P) << std::endl;
    }

    // Reset the integrator_
    integrator_.reset();

    // Iterators to output data structures
    for (int i=0; i<output_its_.size(); ++i) output_its_[i] = output(i).begin();

    // Advance solution in time
    for (int k=0; k<grid_.size(); ++k) {

      if (monitored("step")) {
        userOut() << "SimulatorInternal::evaluate: integrating up to: " <<  grid_[k] << std::endl;
        userOut() << " x0       = "  << integrator_.input(INTEGRATOR_X0) << std::endl;
        userOut() << " z0       = "  << integrator_.input(INTEGRATOR_Z0) << std::endl;
        userOut() << " p        = "   << integrator_.input(INTEGRATOR_P) << std::endl;
      }

      // Integrate to the output time
      integrator_.integrate(grid_[k]);

      if (monitored("step")) {
        userOut() << " xf  = "  << integrator_.output(INTEGRATOR_XF) << std::endl;
        userOut() << " zf  = "  << integrator_.output(INTEGRATOR_ZF) << std::endl;
      }

      // Pass integrator output to the output function
      if (output_fcn_.input(DAE_T).nnz()!=0)
        output_fcn_.setInput(grid_[k], DAE_T);
      if (output_fcn_.input(DAE_X).nnz()!=0)
        output_fcn_.setInput(integrator_.output(INTEGRATOR_XF), DAE_X);
      if (output_fcn_.input(DAE_Z).nnz()!=0)
        output_fcn_.setInput(integrator_.output(INTEGRATOR_ZF), DAE_Z);
      if (output_fcn_.input(DAE_P).nnz()!=0)
        output_fcn_.setInput(input(INTEGRATOR_P), DAE_P);

      // Evaluate output function
      output_fcn_.evaluate();

      // Save the outputs of the function
      for (int i=0; i<n_out(); ++i) {
        const Matrix<double> &res = output_fcn_.output(i);
        copy(res.begin(), res.end(), output_its_.at(i));
        output_its_.at(i) += res.nnz();
      }
    }

    // Consistency check
    for (int i=0; i<output_its_.size(); ++i) {
      casadi_assert(output_its_[i] == output(i).end());
    }
  }

} // namespace casadi
