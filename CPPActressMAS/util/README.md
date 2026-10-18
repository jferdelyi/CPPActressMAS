# CPPActressMAS — utilities

Helpers the library uses but which are not objects of the multi-agent model. Namespace `cam`, no dependency.

| File | Role |
|---|---|
| `Identifier.{h,cpp}` | `generate_uuid` (version 4, RFC 9562) for environments, nodes and transfers; `generate_agent_id` (15 base62 characters) for agents. One generator per thread, seeded once. |
| `TurnPool.{h,cpp}` | Persistent threads of the `Parallel` mode: one wake-up per turn, agents taken one by one through an atomic index, the calling thread working too. |
| `MPSCQueue.hpp` | Lock-free queue, several producers and one consumer: the messages of each agent, and the agents added during a turn. Its nodes come from `FreeBlocks`, so a warm queue allocates nothing. |
| `RecyclingAllocator.hpp` | `FreeBlocks`: memory blocks of one size kept by each thread, reused at the next request, freed when the thread ends; `RecyclingAllocator`, the allocator of the messages (`std::allocate_shared`). |
| `Base64.{h,cpp}` | Base64 (RFC 4648) for the json and ActressMAS codecs. |
