"""
# distutils: language = C++
"""
import sys

import numpy as np

from pele.potentials import _pele
from pele.potentials cimport _pele
from pele.optimize import Result
from pele.potentials._pythonpotential import as_cpp_potential


cimport numpy as np
cimport pele.optimize._pele_opt as _pele_opt
from pele.optimize._pele_opt cimport shared_ptr
cimport cython
from cpython cimport bool as cbool

from libcpp cimport bool

cdef extern from "pele/mxd_end_only.hpp" namespace "pele":
    cdef cppclass cppMXDOptimizer "pele::MixedDescentEndOnly":
        cppMXDOptimizer(shared_ptr[_pele.cBasePotential], _pele.Array[double],
                             double, double, double, double, double, bool) except +
        double get_nhev() except +


cdef class _Cdef_MXDOptimizer_CPP(_pele_opt.GradientOptimizer):
    """This class is the python interface for the c++ MXDOptimizer implementation
       Not quite an optimizer in the usual sense. Solves for the trajectory taken by
       the steepest descent path using the sundials solver CVODE_BDF. the solver terminates
       when it gets close to a minimum, just like an optimizer
    """
    cdef _pele.BasePotential pot
    def __cinit__(self, potential, x0,  double tol=1e-4, double newton_tol=1e-4, double rtol = 1e-4, double atol = 1e-4, double threshold=1e-16, int nsteps=10000, bool iterative=False, bool use_newton_stop_criterion=False):
        potential = as_cpp_potential(potential, verbose=True)

        self.pot = potential
        cdef np.ndarray[double, ndim=1] x0c = np.array(x0, dtype=float)
        self.thisptr = shared_ptr[_pele_opt.cGradientOptimizer]( <_pele_opt.cGradientOptimizer*>
                new cppMXDOptimizer(self.pot.thisptr,
                             _pele.Array[double](<double*> x0c.data, x0c.size),
                                tol, newton_tol, rtol, atol, threshold, iterative))
        cdef cppMXDOptimizer* mxopt_ptr = <cppMXDOptimizer*> self.thisptr.get()
    
    def get_result(self):
        cdef cppMXDOptimizer* mxopt_ptr = <cppMXDOptimizer*> self.thisptr.get()
        res = super(_Cdef_MXDOptimizer_CPP, self).get_result()
        res["nhev"] = float(mxopt_ptr.get_nhev())
        return res

class MXDOptimizer_CPP(_Cdef_MXDOptimizer_CPP):
    """This class is the python interface for the c++ LBFGS implementation
    """
