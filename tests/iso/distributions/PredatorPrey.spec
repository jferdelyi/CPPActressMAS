# E11 Predator-Prey, console version: the populations at turns 10, 100, 500 and 1 000. The C# reuses the places of the
# insects that leave in its Dictionary and changes the order of the following turns, which the C++ does not reproduce
# (a known divergence of the library): the traces part after the first turns, the distributions must not.
stat doodlebugs_10 value ^Turn 10: (\d+) doodlebugs
stat ants_10 value ^Turn 10: \d+ doodlebugs, (\d+) ants
stat doodlebugs_100 value ^Turn 100: (\d+) doodlebugs
stat ants_100 value ^Turn 100: \d+ doodlebugs, (\d+) ants
stat doodlebugs_500 value ^Turn 500: (\d+) doodlebugs
stat ants_500 value ^Turn 500: \d+ doodlebugs, (\d+) ants
stat doodlebugs_1000 value ^Turn 1000: (\d+) doodlebugs
stat ants_1000 value ^Turn 1000: \d+ doodlebugs, (\d+) ants
