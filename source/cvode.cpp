#include "pele/cvode.hpp"
#include "cvode/cvode.h"
#include "cvode/cvode_ls.h"
#include "nvector/nvector_serial.h"
#include "pele/array.hpp"
#include "pele/base_potential.hpp"
#include "pele/optimizer.hpp"
#include "pele/preprocessor_directives.hpp"
#include "sundials/sundials_linearsolver.h"
#include "sundials/sundials_nvector.h"
#include "sunmatrix/sunmatrix_dense.h"
#include <Eigen/src/Core/Matrix.h>
#include <cstddef>
#include <hoomd/HOOMDMath.h>
#include <iostream>
#include <memory>

using namespace std;

#define NUMERICAL_ZERO 1e-15
#define THRESHOLD 1e-9
#define NEWTON_TOL 1e-5
namespace pele {
CVODEBDFOptimizer::CVODEBDFOptimizer(
    std::shared_ptr<pele::BasePotential> potential,
    const pele::Array<double> x0, double tol, double rtol, double atol,
    bool iterative, bool use_newton_stop_criterion)
    : GradientOptimizer(potential, x0, tol),
      cvode_mem(CVodeCreate(CV_BDF)), // create cvode memory
      N_size(x0.size()), hessian(x0.size(), x0.size()), t0(0), tN(10000000.0),
      use_newton_stop_criterion_(use_newton_stop_criterion) {
  // dummy t0
  double t0 = 0;
  Array<double> x0copy = x0.copy();
  x0_N = N_Vector_eq_pele(x0copy);
  // initialization of everything CVODE needs
  int ret = CVodeInit(cvode_mem, f, t0, x0_N);
  // initialize userdata
  udata.rtol = rtol;
  udata.atol = atol;
  udata.nfev = 0;
  udata.nhev = 0;
  udata.pot_ = potential_;
  udata.stored_grad = Array<double>(x0.size(), 0);
  CVodeSStolerances(cvode_mem, udata.rtol, udata.atol);
  ret = CVodeSetUserData(cvode_mem, &udata);
  if (iterative) {
    LS = SUNLinSol_SPGMR(x0_N, PREC_NONE, 0);
    CVodeSetLinearSolver(cvode_mem, LS, NULL);

  } else {

    A = SUNDenseMatrix(N_size, N_size);
    LS = SUNLinSol_Dense(x0_N, A);

    CVodeSetLinearSolver(cvode_mem, LS, A);
    CVodeSetJacFn(cvode_mem, Jac);
  }
  g_ = udata.stored_grad;
  CVodeSetMaxNumSteps(cvode_mem, 1000000);
  CVodeSetStopTime(cvode_mem, tN);
#if PRINT_TO_FILE == 1
  trajectory_file.open("trajectory_cvode.txt");
  // Also saving hessian eigenvalues to understand what's happening
  // along the run
  hessian_eigvals_file.open("hessian_eigvals_cvode.txt");
  grad_file.open("grad_cvode.txt");
  step_file.open("step_cvode.txt");
  newton_step_file.open("newton_step_cvode.txt");
  lbfgs_m_1_step_file.open("lbfgs_m_1_step_cvode.txt");
  g_old = Array<double>(x0.size(), 0);
#endif
};

CVODEBDFOptimizer::~CVODEBDFOptimizer() {
  CVodeFree(&cvode_mem);
  SUNLinSolFree(LS);
  SUNMatDestroy(A);
  N_VDestroy(x0_N);
}

void CVODEBDFOptimizer::one_iteration() {
  /* advance solver just one internal step */
  Array<double> xold = x_;

  int flag = CVode(cvode_mem, tN, x0_N, &t0, CV_ONE_STEP);
  iter_number_ += 1;
  x_ = pele_eq_N_Vector(x0_N);
  g_ = udata.stored_grad;
  rms_ = (norm(g_) / sqrt(x_.size()));
  f_ = udata.stored_energy;
  nfev_ = udata.nfev;
  Array<double> step = xold - x_;

  // really hacky way to output $lambdamin/lambdamax on a low tolerance run
  // simply print the energy and lambdamin/lambdamax as csv values and write
  // stdout to file then plot them using python

#if PRINT_TO_FILE == 1
  // get hessian routine, useful for understanding conditioning near minimum
  Array<double> hess(xold.size() * xold.size());
  Array<double> grad(xold.size());
  double e = potential_->get_energy_gradient_hessian(x_, grad, hess);
  Eigen::MatrixXd hess_dense(xold.size(), xold.size());
  udata.nhev += 1;
  hess_dense.setZero();
  for (size_t i = 0; i < xold.size(); ++i) {
    for (size_t j = 0; j < xold.size(); ++j) {
      hess_dense(i, j) = hess[i + grad.size() * j];
    }
  }
  
  // calculate minimum and maximum eigenvalue
  Eigen::VectorXd eigvals = hess_dense.eigenvalues().real();
  add_translation_offset_2d(hess_dense, 1.0);
  Eigen::VectorXd r(hess_dense.rows());
  Eigen::VectorXd q(hess_dense.rows());
  q.setZero();

  eig_eq_pele(r, g_);

  q = - hess_dense.householderQr().solve(r);




  double minimum = eigvals.minCoeff();
  double maximum = eigvals.maxCoeff();

  Array<double> eigvals_pele = Array<double>(xold.size());
  for (size_t i = 0; i < xold.size(); ++i) {
    eigvals_pele[i] = eigvals(i);
  }

  Array<double> q_pele = Array<double>(xold.size());

  for (size_t i = 0; i < xold.size(); ++i) {
    q_pele[i] = q(i);
  }
  Array<double> dg_old = Array<double>(xold.size());
  Array<double> dx_old = Array<double>(xold.size());
  if (iter_number_ >= 0) {
    dg_old = g_ - g_old;
    dx_old = x_ - xold;
  }
  double dg_dot_dx;
  double dg_dot_dg;
  for (size_t i = 0; i < xold.size(); ++i) {
    dg_dot_dx += dg_old[i] * dx_old[i];
    dg_dot_dg += dg_old[i] * dg_old[i];
  }

  double H0 = dg_dot_dx / dg_dot_dg;

  Array<double> H0_g = Array<double>(xold.size());

  H0_g = H0 * g_;

  



  // write to file
  trajectory_file << x_;
  hessian_eigvals_file << eigvals_pele;
  grad_file << grad;
  step_file << step;
  newton_step_file << q_pele;
  lbfgs_m_1_step_file << H0_g;
  g_old.assign(g_);
#endif

  // print to file

  // double convexity_estimate;

  // if (minimum<0) {
  //     convexity_estimate = std::abs(minimum / maximum);
  // }
  // else {
  //     convexity_estimate = 0;
  // }
  // std::cout << e << "," << convexity_estimate << "," << minimum << "," <<
  // maximum << "\n";
  // // TODO: This is terrible C++ code but write it better
};

void CVODEBDFOptimizer::add_translation_offset_2d(Eigen::MatrixXd &hessian,
                                                       double offset) {

  // factor so that the added translation operators are unitary
  double factor = 2.0 / hessian.rows();
  offset = offset * factor;

  for (size_t i = 0; i < hessian.rows(); ++i) {
    for (size_t j = i + 1; j < hessian.cols(); ++j) {
      if ((i % 2 == 0 && j % 2 == 0) || (i % 2 == 1 && j % 2 == 1)) {
        hessian(i, j) += offset;
        hessian(j, i) += offset;
      }
    }
  }
  hessian.diagonal().array() += offset;
}


bool CVODEBDFOptimizer::stop_criterion_satisfied() {
  if (!func_initialized_) {
    // For some reason clang throws an error here, but gcc compiles properly
    initialize_func_gradient();
  }
  if (rms_ < tol_) {
    if (use_newton_stop_criterion_) {
      if (iter_number_ % 100 == 0) {
        // Check with a more stringent hessian condition
        Array<double> pele_hessian =
            Array<double>(hessian.data(), hessian.size());

        Eigen::Map<Eigen::VectorXd, 0, Eigen::InnerStride<1>> g_eigen(
            g_.data(), int(g_.size()));
        potential_->get_hessian(x_, pele_hessian);

        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es =
            Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd>(hessian);
        Eigen::VectorXd eigenvalues = es.eigenvalues();
        Eigen::MatrixXd eigenvectors = es.eigenvectors();

        // replace eigenvalues with their absolute values
        for (size_t i = 0; i < eigenvalues.size(); ++i) {
          eigenvalues(i) = std::abs(eigenvalues(i));
        }

        // cout << "eigenvalues" << eigenvalues << endl;
        // postprocess inverse Eigenvalues
        Eigen::VectorXd inv_eigenvalues(eigenvalues.size());
        for (size_t i = 0; i < eigenvalues.size(); ++i) {
          double mod_eig = abs(eigenvalues(i));
          if (mod_eig < NUMERICAL_ZERO) {
            // case for translational symmetries
            inv_eigenvalues(i) = 0.0;
          } else if (mod_eig < THRESHOLD) {
            // case for really small eigenvalues
            inv_eigenvalues(i) = 1.0 / THRESHOLD;
          } else {
            // case for normal eigenvalues
            inv_eigenvalues(i) = 1.0 / mod_eig;
          }
        }
        Eigen::VectorXd newton_step(g_.size());
        newton_step.setZero();

        // find the largest eigenvalue
        newton_step =
            -eigenvectors *
            ((eigenvectors.transpose() * g_eigen).cwiseProduct(inv_eigenvalues))
                .matrix();
        if (newton_step.norm() < NEWTON_TOL) {
          std::cout << "converged in " << iter_number_ << " iterations\n";
          std::cout << "rms = " << rms_ << "\n";
          std::cout << "tol = " << tol_ << "\n";
          return true;
        } else {
          return false;
        }
      } else {
        return false;
      }
    } else {
      std::cout << "converged in " << iter_number_ << " iterations\n";
      std::cout << "rms = " << rms_ << "\n";
      std::cout << "tol = " << tol_ << "\n";
      return true;
    }

    // Wrap into a matrix
  } else {
    return false;
  }
}

int f(double t, N_Vector y, N_Vector ydot, void *user_data) {

  UserData udata = (UserData)user_data;
  pele::Array<double> yw = pele_eq_N_Vector(y);
  Array<double> g;
  // double energy = udata->pot_->get_energy_gradient(yw, g);
  double *fdata = NV_DATA_S(ydot);
  g = Array<double>(fdata, NV_LENGTH_S(ydot));

  // calculate negative grad g
  double energy = udata->pot_->get_energy_gradient(yw, g);
  udata->nfev += 1;
#pragma simd
  for (size_t i = 0; i < yw.size(); ++i) {
    fdata[i] = -fdata[i];
  }
  udata->stored_grad = (g);
  udata->stored_energy = energy;
  return 0;
}

int Jac(realtype t, N_Vector y, N_Vector fy, SUNMatrix J, void *user_data,
        N_Vector tmp1, N_Vector tmp2, N_Vector tmp3) {
  UserData udata = (UserData)user_data;

  pele::Array<double> yw = pele_eq_N_Vector(y);
  Array<double> g = Array<double>(yw.size());
  // TODO: don't keep allocating memory for every jaocobian calculation
  Array<double> h = Array<double>(yw.size() * yw.size());
  udata->pot_->get_energy_gradient_hessian(pele_eq_N_Vector(y), g, h);
  udata->nhev += 1;
  double *hessdata = SUNDenseMatrix_Data(J);
  return 0;
};

} // namespace pele
