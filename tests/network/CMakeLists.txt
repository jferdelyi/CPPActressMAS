# Network tests (label network, plus the transport: mqtt or zenoh). Each program skips (exit code 77) when its transport
# is not compiled in or, for MQTT, when no broker is given; ctest -LE network keeps a run off the network.

# MQTT: needs a broker, CAM_MQTT_HOST and CAM_MQTT_PORT in the environment.
add_executable(cam_network_tests network_tests.cpp)
target_link_libraries(cam_network_tests cam_test_support ${CAM_TEST_LIBRARY})
add_test(NAME network.mqtt COMMAND cam_network_tests)
set_tests_properties(network.mqtt PROPERTIES LABELS "network;mqtt" SKIP_RETURN_CODE 77 TIMEOUT 120)
cam_apply_tsan_suppressions(network.mqtt)

# Zenoh: two peers of the process on 127.0.0.1, explicit endpoints, no multicast; opens sockets.
add_executable(cam_zenoh_tests zenoh_tests.cpp)
target_link_libraries(cam_zenoh_tests cam_test_support ${CAM_TEST_LIBRARY})
add_test(NAME network.zenoh COMMAND cam_zenoh_tests)
set_tests_properties(network.zenoh PROPERTIES LABELS "network;zenoh" SKIP_RETURN_CODE 77 TIMEOUT 180)
cam_apply_tsan_suppressions(network.zenoh)
