# E04 Shapley Value: parallel; the C# picks the workers in the order of a ConcurrentDictionary, whose string hash changes
# from one run to the next: the total of the rewards (the prices of the tasks drawn from the seed) and the largest one.
stat rewards sum ^Agent worker\d{3} has (-?\d+\.\d+)$
stat best_reward max ^Agent worker\d{3} has (-?\d+\.\d+)$
