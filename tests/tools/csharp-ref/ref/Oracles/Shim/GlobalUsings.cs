// The name Random resolves to the seed shim in every file of an oracle project: a global using alias wins over the
// System.Random imported by `using System;` at file level (see SeededRandom.cs).
global using Random = CamOracle.SeededRandom;
