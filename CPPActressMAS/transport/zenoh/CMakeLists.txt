# Zenoh transport of CPPActressMAS (cam_transport_zenoh), behind CAM_TRANSPORT_ZENOH.
#
# zenoh-c and zenoh-cpp come from the system at one exact pinned version (D22). On macOS the static zenoh-c
# library is linked: the shared one installed here has a broken install name (ENV.md), and a static link keeps
# the executables self-contained. Elsewhere the shared library is preferred when the package provides it:
# ThreadSanitizer then sees the Rust runtime as a module of its own and ignores the synchronisation it cannot
# instrument (tests/tsan/suppressions.txt, T10). Their include directories are imported ones, treated as
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

add_library(cam_transport_zenoh STATIC ${CMAKE_CURRENT_LIST_DIR}/ZenohTransport.cpp ${CMAKE_CURRENT_LIST_DIR}/ZenohTransport.h)
target_link_libraries(cam_transport_zenoh PUBLIC ${CAM_CORE_TARGET} PRIVATE zenohcxx ${CAM_ZENOHC_TARGET})
target_compile_definitions(cam_transport_zenoh PRIVATE ZENOHCXX_ZENOHC)
