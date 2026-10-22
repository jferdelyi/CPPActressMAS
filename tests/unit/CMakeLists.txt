# Unit tests: the pieces of the library alone, one doctest program each (label unit). Their data are under vectors/:
# envelope/<codec>/v1, the binary vectors of the envelope codecs (CAM_WRITE_VECTORS=1 in the environment writes them
# again), and rng/, the series of the seeded System.Random of .NET and the turn orders of the C#, produced by
# tools/csharp-ref.
set(CAM_TEST_TYPE unit)

cam_add_doctest(smoke cam_tests smoke.cpp)

# Transport layer without any environment: codec (round trips, binary vectors, malformed input) and Endpoint
# (sequences, deduplication, reordering, overflows, counters). The ActressMAS wire codec is checked against the bytes
# captured from the C# code.
cam_add_doctest(transport cam_transport_tests envelope_codec.cpp endpoint.cpp actressmas_codec.cpp)
target_compile_definitions(cam_transport_tests PRIVATE CAM_VECTORS_DIR="${CMAKE_CURRENT_LIST_DIR}/vectors")

# Environment::connect(url): the URL kept by the environment, the default factory over the schemes compiled in, a
# custom factory, and the shared loopback of a namespace.
cam_add_doctest(connect cam_connect_tests connect.cpp)

# Queue of the agent messages: arrival order, several producers at once, nodes recycled per thread.
cam_add_doctest(mpsc_queue cam_mpsc_queue_tests mpsc_queue.cpp)

# Blocks recycled per thread: reuse, release from another thread, threads that end, arrays.
cam_add_doctest(recycling_allocator cam_recycling_allocator_tests recycling_allocator.cpp)

# Pointer to a message with its counter inside: copies, moves, kept messages, release across threads.
cam_add_doctest(message_pointer cam_message_pointer_tests message_pointer.cpp)

# Identifier of an agent: size, capacity, literals, comparisons, hash, stream, JSON.
cam_add_doctest(agent_id cam_agent_id_tests agent_id.cpp)

# Removal of agents: deferred to the turn boundary during a turn, at once outside a turn.
cam_add_doctest(removal cam_removal_tests removal.cpp)

# Observables: what an agent sees of the others before each action, as the C#.
cam_add_doctest(observables cam_observables_tests observables.cpp)

# Delivery and turn semantics: the order of the messages of one sender, agents and messages from another thread while
# the environment runs, a shared read-only JSON content, and the turn loop of the C# manual.
cam_add_doctest(delivery cam_delivery_tests delivery.cpp)

# Generator of the seeded System.Random of .NET, against the series of the C#.
cam_add_doctest(dotnet_random cam_dotnet_random_tests dotnet_random.cpp)
target_compile_definitions(cam_dotnet_random_tests PRIVATE CAM_VECTORS_DIR="${CMAKE_CURRENT_LIST_DIR}/vectors")

# Random order of the sequential modes: the turn orders of the C# with the .NET generator, portable Mt19937, seeds.
cam_add_doctest(random_order cam_random_order_tests random_order.cpp)
target_compile_definitions(cam_random_order_tests PRIVATE CAM_VECTORS_DIR="${CMAKE_CURRENT_LIST_DIR}/vectors")

# Text of a message as the C# reads it: Parse, Parse1P, Format with names.
cam_add_doctest(message_parse cam_message_parse_tests message_parse.cpp)

# Catalogue of the agent types of the process: automatic names, CAM_AGENT_TYPE, conflicts, migration.
cam_add_doctest(agent_types cam_agent_types_tests agent_types.cpp)

# Identifiers: version 4 UUIDs, short agent identifiers, uniqueness from several threads.
cam_add_doctest(identifier cam_identifier_tests identifier.cpp)
