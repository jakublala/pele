import unittest
from builtins import range

import numpy as np

from pele.optimize import mylbfgs
from pele.potentials import LJ
from pele.systems import LJCluster
from playground.native_code import _lj


class TestLJ_CPP_Ilist(unittest.TestCase):
    def setUp(self):
        self.natoms = 18
        self.ilist = np.array(
            [(i, j) for i in range(self.natoms) for j in range(i + 1, self.natoms)]
        ).reshape(-1)
        assert self.ilist.size == self.natoms * (self.natoms - 1)
        #        print self.ilist
        self.pot = _lj.LJInteractionList(self.ilist)

        self.pot_comp = LJ()
        x = np.random.uniform(-1, 1, 3 * self.natoms)
        ret = mylbfgs(x, self.pot_comp, tol=10.0)
        self.x = ret.coords

    def test(self):
        eonly = self.pot.getEnergy(self.x)
        e, g = self.pot.getEnergyGradient(self.x)
        self.assertAlmostEqual(e, eonly, delta=1e-6)
        et, gt = self.pot_comp.getEnergyGradient(self.x)
        self.assertAlmostEqual(e, et, delta=1e-6)
        self.assertLess(np.max(np.abs(g - gt)), 1e-6)


if __name__ == "__main__":
    unittest.main()
