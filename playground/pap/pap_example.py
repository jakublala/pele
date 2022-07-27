from __future__ import division, print_function

from builtins import range
from copy import deepcopy
from math import cos, pi, sin

import gmin_ as GMIN
import numpy as np
from past.utils import old_div

from pele.angleaxis import RBSystem, RBTopology, RigidFragment
from pele.angleaxis.aamindist import ExactMatchAACluster, MinPermDistAACluster
from pele.optimize import fire
from pele.potentials import GMINPotential


def create_pap():
    pap = RigidFragment()
    rho = 0.3572
    theta = 104.52 / 180.0 * pi
    pap.add_atom("O", np.array([0.0, 0.0, 0.0]), 1.0)
    pap.add_atom("H", np.array([-0.038490, 0.1204928, 0.3794728]), 1.0)
    pap.add_atom("C", np.array([-0.038490, -0.1204928, -0.3794728]), 1.0)
    pap.finalize_setup(shift_com=False)

    print("inversion:\n", pap.inversion)
    print("symmetry:\n", pap.symmetries)
    pap.inversion = None
    # print pap.S
    # pap.S = 0.3*np.identity(3)
    return pap


class PAPSystem(RBSystem):
    def __init__(self):
        RBSystem.__init__(self)

        NEBparams = self.params.double_ended_connect.local_connect_params.NEBparams
        self.params.double_ended_connect.local_connect_params.NEBparams.distance = (
            self.aasystem.neb_distance
        )

        NEBparams.max_images = 200
        NEBparams.image_density = 10.0
        NEBparams.iter_density = 6
        NEBparams.quenchRoutine = fire
        del NEBparams.NEBquenchParams["maxErise"]
        NEBparams.NEBquenchParams["maxstep"] = 0.01
        fts = self.params.double_ended_connect.local_connect_params.tsSearchParams
        fts["iprint"] = 1
        fts["nsteps"] = 3000
        fts["nsteps_tangent1"] = 3
        fts["nsteps_tangent2"] = 30
        fts["tol"] = 1e-3
        fts["lowestEigenvectorQuenchParams"]["nsteps"] = 200
        fts["lowestEigenvectorQuenchParams"]["tol"] = 0.002
        fts["lowestEigenvectorQuenchParams"]["iprint"] = -1
        self.params.structural_quench_params["tol"] = 1e-7
        self.params.structural_quench_params["nsteps"] = 1e7

    def setup_aatopology(self):
        GMIN.initialize()
        pot = GMINPotential(GMIN)
        coords = pot.getCoords()
        nrigid = old_div(coords.size, 6)

        print("I have %d PAP molecules in the system" % nrigid)
        print("The initial energy is", pot.getEnergy(coords))

        water = create_pap()

        system = RBTopology()
        system.add_sites([deepcopy(water) for i in range(nrigid)])
        self.potential = pot
        self.nrigid = nrigid

        self.render_scale = 0.1
        self.atom_types = system.get_atomtypes()

        self.draw_bonds = []
        for i in range(nrigid):
            self.draw_bonds.append((3 * i, 3 * i + 1))
            self.draw_bonds.append((3 * i, 3 * i + 2))

        return system

    def get_compare_exact(self, **kwargs):
        return ExactMatchAACluster(self.aasystem, accuracy=0.1, tol=0.07, **kwargs)

    def get_mindist(self, **kwargs):
        return MinPermDistAACluster(self.aasystem, accuracy=0.1, tol=0.07, **kwargs)

    def get_potential(self):
        return self.potential


if __name__ == "__main__":
    import pele.gui.run as gr

    gr.run_gui(PAPSystem, db="pap.sqlite")
