# E08 Contract Net: parallel, with a race of the C# on the shared CNPMessage that the C++ takes under a lock: the total
# of the payoffs printed (at the start and at the end), the largest payoff at the end, and the tasks given.
stat payoffs sum ^Payoff: (-?\d+\.\d+)
stat best_payoff max ^Payoff: (-?\d+\.\d+)
stat given count is given
