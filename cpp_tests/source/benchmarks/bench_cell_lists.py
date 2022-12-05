from __future__ import division
from __future__ import print_function
from past.utils import old_div
import numpy as np

from pele.potentials._lj_cpp import LJCutCellLists
from pele.optimize import lbfgs_cpp

x0_cpp = np.genfromtxt("coords")

np.random.seed(0)

x = x0_cpp.copy().ravel()
natoms = old_div(x.size, 3)
density = 1.2
L = (old_div(natoms * (4.0 / 3 * np.pi), density)) ** (1.0 / 3)
print("box length", L)
boxvec = np.array([L] * 3)
rcut = 2.0

pot = LJCutCellLists(boxvec=boxvec, rcut=rcut, ncellx_scale=1.0)


if False:
    res = lbfgs_cpp(x, pot, tol=100)
    x = res.coords
    print("coords")
    np.set_printoptions(threshold=np.nan, precision=16, linewidth=100)
    print(repr(res.coords.reshape(-1, 3)))
    raise Exception("stopping early")


print("initial energy", pot.getEnergy(x))
lbfgs_cpp(x, pot, iprint=100)


# for i in xrange(1):
#    print "displacement", i
#    x += np.random.uniform(-.2, .2, x.size)
#    lbfgs_cpp(x, pot, iprint=50)
