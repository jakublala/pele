 // base imports
#include "pele/array.hpp"
#include "pele/cell_lists.hpp"
#include "pele/utils.hpp"
// optimizer imports
#include "pele/extended_mixed_descent.hpp"

// potential imports
#include "pele/inversepower.hpp"
#include "pele/rosenbrock.hpp"

#include "pele/gradient_descent.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using pele::Array;
using std::cout;

// Test for whether the switch works for mixed descent
TEST(EXTENDED_MXD, TEST_256_RUN) {
  static const size_t _ndim = 2;
  size_t n_particles;
  size_t n_dof;

  double power;
  double eps;

  double phi;

  pele::Array<double> x;
  pele::Array<double> radii;
  pele::Array<double> xfinal;
  pele::Array<double> boxvec;

  eps = 1.0;
  power = 2.5;

  n_particles = 8;
  n_dof = n_particles * _ndim;
  phi = 0.9;

  int n_1 = n_particles / 2;
  int n_2 = n_particles - n_1;

  double r_1 = 1.0;
  double r_2 = 1.0;
  double r_std1 = 0.05;
  double r_std2 = 0.05;

  radii = pele::generate_radii(n_1, n_2, r_1, r_2, r_std1, r_std2);

  double box_length = pele::get_box_length(radii, _ndim, phi);

// required to plot trajectory
#if PRINT_TO_FILE == 1
  std::ofstream radii_file;
  std::ofstream box_length_file;

  radii_file.open("radii.txt");
  box_length_file.open("box_length.txt");

  radii_file << radii;
  box_length_file << box_length << std::endl;
  radii_file.close();
  box_length_file.close();
#endif
  boxvec = {box_length, box_length};

  x = pele::generate_random_coordinates(box_length, n_particles, _ndim);

  //////////// parameters for inverse power potential at packing fraction 0.7 in
  ///3d
  //////////// as generated from code params.py in basinerror library

  double k;
#ifdef _OPENMP
  omp_set_num_threads(1);
#endif

  double ncellsx_scale = get_ncellx_scale(radii, boxvec, 1);
  // std::shared_ptr<pele::InversePowerPeriodicCellLists<_ndim>> potcell =
  //     std::make_shared<pele::InversePowerPeriodicCellLists<_ndim>>(
  //         power, eps, radii, boxvec, ncellsx_scale);
        
  std::shared_ptr<pele::InversePowerPeriodic<_ndim> > pot =
  std::make_shared<pele::InversePowerPeriodic<_ndim> >(power, eps, radii, boxvec);

  std::shared_ptr<pele::InversePowerPeriodic<_ndim>> extension_potential =
      std::make_shared<pele::InversePowerPeriodic<_ndim>>(power, eps*1e-6, radii*2, boxvec);

  // std::shared_ptr<pele::InversePowerPeriodic<_ndim>> extension_potential

  pele::ExtendedMixedOptimizer optimizer(pot, extension_potential, x, 1e-9, 10);
  optimizer.run(500);
  std::cout << optimizer.get_nfev() << "nfev \n";
  std::cout << optimizer.get_nhev() << "nhev \n";
  std::cout << optimizer.get_nhev_extended() << "nhev_extended \n";
  std::cout << optimizer.get_n_phase_2_steps() << "phase 2 steps" << "\n";
  std::cout << optimizer.get_rms() << "rms \n";
  std::cout << optimizer.get_niter() << "rms \n";
}
