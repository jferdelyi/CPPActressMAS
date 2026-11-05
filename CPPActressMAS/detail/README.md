# CPPActressMAS: internals

The internals of the library, which the user does not handle: the agents of an environment and its turns, the threads of
the `Parallel` mode, the queues and the memory of the messages. Namespace `cam`, no dependency.

| File | Role |
|---|---|
| `AgentCollection.{h,cpp}` | The agents of one environment, in insertion order, with the name index and the agents added during a turn; runs one turn in the chosen mode (`Sequential`, `SequentialRandom`, `Parallel`) and removes the dead agents at the turn boundary: during a turn its collections never change, a stopped or removed agent is only marked, no longer counted. |
| `TurnPool.{h,cpp}` | Persistent threads of the `Parallel` mode: one wake-up per turn, agents taken one by one through an atomic index, the calling thread working too; the exceptions of the agents leave the turn together, in an `AggregateException`. |
| `MPSCQueue.hpp` | Lock-free queue, several producers and one consumer: the messages of each agent, and the agents added during a turn. Its nodes come from `FreeBlocks`, so a warm queue allocates nothing. |
| `RecyclingAllocator.hpp` | `FreeBlocks`: memory blocks of one size kept by each thread, reused at the next request, freed when the thread ends (the messages and the queue nodes take theirs from it); `RecyclingAllocator`, the allocator of the agents, the JSON contents and the name index. |
