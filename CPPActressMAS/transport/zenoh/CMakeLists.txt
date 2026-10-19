# Zenoh transport of CPPActressMAS, behind CAM_TRANSPORT_ZENOH (sources joined to the core library).
#
# zenoh-c and zenoh-cpp come from the system at one exact pinned version (D22). On macOS the static zenoh-c
# library is linked: the shared one installed here has a broken install name (ENV.md), and a static link keeps
# the executables self-contained. Elsewhere the shared library is preferred when the package provides it:
# ThreadSanitizer then sees the Rust runtime as a module of its own and ignores the synchronisation it cannot
# instrument (tests/support/tsan_suppressions.txt, T10). Their include directories are imported ones, treated as
# system headers.

find_package(zenohc 1.7.1 EXACT QUIET)
find_package(zenohcxx 1.7.1 EXACT QUIET)
if (NOT zenohc_FOUND OR NOT zenohcxx_FOUND)
	message(FATAL_ERROR "CAM_TRANSPORT_ZENOH needs zenoh-c and zenoh-cpp 1.7.1 (find_package zenohc, zenohcxx): "
		"install them, point zenohc_DIR and zenohcxx_DIR at them, or configure with -DCAM_TRANSPORT_ZENOH=OFF")
endif()

if (APPLE OR NOT TARGET zenohc::shared)
	set(CAM_ZENOHC_TARGET zenohc::static)
else()
	set(CAM_ZENOHC_TARGET zenohc::shared)
endif()

# The imported targets of the packages are local to this directory: the core targets are defined in the parent one.
set_target_properties(zenohcxx PROPERTIES IMPORTED_GLOBAL TRUE)

# Sources and dependencies taken by the core targets (root CMakeLists.txt, cam_add_transport). The zenoh-cpp
# headers need ZENOHCXX_ZENOHC; only the transport source includes them.
set(CAM_ZENOH_SOURCES ${CMAKE_CURRENT_LIST_DIR}/ZenohTransport.cpp ${CMAKE_CURRENT_LIST_DIR}/ZenohTransport.h PARENT_SCOPE)
set(CAM_ZENOH_INCLUDES "" PARENT_SCOPE)
set(CAM_ZENOH_LIBRARIES zenohcxx ${CAM_ZENOHC_TARGET} PARENT_SCOPE)
set(CAM_ZENOH_DEFINITIONS ZENOHCXX_ZENOHC PARENT_SCOPE)
