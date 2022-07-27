from __future__ import absolute_import, division

import unittest
from builtins import range

import numpy as np
from past.utils import old_div

from pele.mindist._minpermdist_policies import MeasureAtomicCluster
from pele.mindist.minpermdist_stochastic import MinPermDistCluster
from pele.optimize import mylbfgs

from .testmindist import TestMinDist


class TestMinPermDistStochastic_BLJ(TestMinDist):
    def setUp(self):
        from pele.potentials.ljpshiftfast import LJpshift as BLJ

        self.natoms = 25
        self.ntypeA = int(self.natoms * 0.8)
        self.pot = BLJ(self.natoms, self.ntypeA)
        self.permlist = [
            list(range(self.ntypeA)),
            list(range(self.ntypeA, self.natoms)),
        ]

        self.X1 = old_div(
            np.random.uniform(-1, 1, [self.natoms * 3])
            * (float(self.natoms)) ** (1.0 / 3),
            2,
        )

        # run a quench so the structure is not crazy
        ret = mylbfgs(self.X1, self.pot)
        self.X1 = ret.coords

    def testBLJ(self):
        X1 = np.copy(self.X1)
        X2 = old_div(
            np.random.uniform(-1, 1, [self.natoms * 3])
            * (float(self.natoms)) ** (1.0 / 3),
            2,
        )

        # run a quench so the structure is not crazy
        ret = mylbfgs(X2, self.pot)
        X2 = ret.coords

        self.runtest(
            X1,
            X2,
            MinPermDistCluster(measure=MeasureAtomicCluster(permlist=self.permlist)),
        )

    def testBLJ_isomer(self):
        """
        test with BLJ potential.  We have two classes of permutable atoms

        test case where X2 is an isomer of X1.
        """
        import pele.utils.rotations as rot

        X1i = np.copy(self.X1)
        X1 = np.copy(self.X1)
        X2 = np.copy(X1)

        # rotate X2 randomly
        aa = rot.random_aa()
        rot_mx = rot.aa2mx(aa)
        for j in range(self.natoms):
            i = 3 * j
            X2[i : i + 3] = np.dot(rot_mx, X1[i : i + 3])

        # permute X2
        import copy
        import random

        from pele.mindist.permutational_alignment import permuteArray

        for atomlist in self.permlist:
            perm = copy.copy(atomlist)
            random.shuffle(perm)
            X2 = permuteArray(X2, perm)

        X2i = np.copy(X2)

        # distreturned, X1, X2 = self.runtest(X1, X2)
        distreturned, X1, X2 = self.runtest(
            X1,
            X2,
            MinPermDistCluster(measure=MeasureAtomicCluster(permlist=self.permlist)),
        )

        # it's an isomer, so the distance should be zero
        self.assertTrue(
            abs(distreturned) < 1e-14, "didn't find isomer: dist = %g" % distreturned
        )


if __name__ == "__main__":
    unittest.main()
