from __future__ import print_function

import numpy as np

import pele.potentials.fortran.magnetic_colloids as mc
from pele.potentials import BasePotential


class MagneticColloidPotential(BasePotential):
    """
    potential for calculating the energy and forces for a magnetic colloid system
    """

    def __init__(self, natoms, box):
        self.natoms = natoms
        self.box = box
        self.theta = 90.0 * np.pi / 180.0

    #    def getEnergy(self, coords):
    #        rij = mc.get_rij(coords)
    #        energy = mc.self_consistent_energy(rij)
    #        return energy

    def getEnergy(self, coords):
        coords = coords.reshape(-1, 3)
        e, g = self.getEnergyGradient(coords)
        return e

    def getEnergyGradient(self, coords):
        coords = coords.reshape(-1, 3)
        f, wpot = mc.get_energy_forces(coords, self.box, self.theta)
        gradient = -f
        return wpot, gradient.flatten()


from pele.systems import LJCluster


class MagneticColloidSystem(LJCluster):
    def __init__(self, natoms, box):
        super(MagneticColloidSystem, self).__init__(natoms)
        self.natoms = natoms
        self.box = box

    def get_potential(self):
        return MagneticColloidPotential(self.natoms, self.box)

    def get_compare_exact(self, **kwargs):
        raise NotImplementedError

    def get_mindist(self, **kwargs):
        raise NotImplementedError


if __name__ == "__main__":
    natoms = 10
    box = np.array([10.0, 10.0, 10.0])

    pot = MagneticColloidPotential(natoms, box)
    coords = np.random.uniform(0, 1, 3 * natoms) * 2.0
    print("starting potential")
    e, grad = pot.getEnergyGradient(coords)
    print("finished potential")
    print(e)

    pot.test_potential(coords)

    system = MagneticColloidSystem(natoms, box)
    from pele.gui import run_gui

    run_gui(system)
