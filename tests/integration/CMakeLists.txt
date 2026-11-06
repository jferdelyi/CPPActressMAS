# Integration tests: several environments of one process, connected through the loopback transport (label integration).
set(CAM_TEST_TYPE integration)

# Environments connected through the loopback transport, in memory and serializing.
cam_add_doctest(loopback cam_loopback_tests loopback.cpp)

# Three-step agent migration between environments connected through the loopback transport.
cam_add_doctest(migration cam_migration_tests migration.cpp)

# Environments of one process without configuration, reaching each other through the loopback of the process.
cam_add_doctest(transparent_loopback cam_transparent_loopback_tests transparent_loopback.cpp)
