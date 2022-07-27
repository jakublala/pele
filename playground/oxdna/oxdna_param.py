from __future__ import division, print_function

import time
from math import pi

import numpy as np
import oxdnagmin_ as GMIN
import parameters
from past.utils import old_div

from pele import takestep
from pele.basinhopping import BasinHopping
from pele.potentials import GMINPotential
from pele.storage.database import Database
from pele.utils import rotations
from pele.utils.rbtools import CoordsAdapter

t0 = time.clock()

# This is the takestep routine for OXDNA. It is a standard rigid body takestep
# routine, but I put it here to be able to start modifying it
class OXDNATakestep(takestep.TakestepInterface):
    def __init__(self, displace=1.0, rotate=1.0):
        self.displace = displace
        self.rotate = rotate

    def takeStep(self, coords, **kwargs):
        # easy access to coordinates
        ca = CoordsAdapter(nrigid=old_div(coords.size, 6), coords=coords)

        # random displacement for positions
        ca.posRigid[:] += (
            2.0 * self.displace * (np.random.random(ca.posRigid.shape) - 0.5)
        )
        # random rotation for angle-axis vectors
        takestep.rotate(self.rotate, ca.rotRigid)

    # this is necessary for adaptive step taking
    def scale(self, factor):
        self.rotate *= factor
        self.displace *= factor

    @property
    def stepsize(self):
        return [self.rotate, self.displace]


# this class should generate a fully random configuration
class OXDNAReseed(takestep.TakestepInterface):
    def __init__(self, radius=3.0):
        self.radius = radius

    def takeStep(self, coords, **kwargs):
        # easy access to coordinates
        ca = CoordsAdapter(nrigid=old_div(coords.size, 6), coords=coords)

        # random displacement for positions
        # ca.posRigid[:] = 2.*self.radius*(np.random.random(ca.posRigid.shape)-0.5)

        # random rotation for angle-axis vectors
        for rot in ca.rotRigid:
            rot[:] = rotations.random_aa()

    def check_converged(E, coords):
        if E < (parameters.TARGET + parameters.EDIFF):
            fl = open("stat.dat", "a")
            print("#found minimum")
            t1 = time.clock()
            timespent = t1 - t0
            fl.write("#quenches, functioncalls, time\n")
            fl.write("%d %d %f\n" % (opt.stepnum, potential.ncalls, timespent))
            fl.close()
            exit()


# initialize GMIN
GMIN.initialize()
# create a potential which calls GMIN
potential = GMINPotential(GMIN)
# get the initial coorinates
coords = potential.getCoords()
coords = np.random.random(coords.shape)
# create takestep routine

# we combine a normal step taking
group = takestep.BlockMoves()

step1 = takestep.AdaptiveStepsize(
    OXDNATakestep(displace=parameters.displace, rotate=0.0), frequency=50
)
step2 = takestep.AdaptiveStepsize(
    OXDNATakestep(displace=0.0, rotate=parameters.rotate), frequency=50
)
group.addBlock(100, step1)
group.addBlock(100, step2)
# with a generate random configuration
genrandom = OXDNAReseed()
# in a reseeding takestep procedure
reseed = takestep.Reseeding(group, genrandom, maxnoimprove=parameters.reseed)

# store all minima in a database
db = Database(db="storage.sqlite", accuracy=1e-2)

# create Basinhopping object
opt = BasinHopping(
    coords, potential, reseed, db.minimum_adder(), temperature=parameters.temperature
)

# run for 100 steps
opt.run(parameters.nsteps)

# now dump all the minima
i = 0
for m in db.minima():
    i += 1
    GMIN.userpot_dump("lowest_%03d.dat" % (i), m.coords)
