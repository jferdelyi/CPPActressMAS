# ActressMAS transport of CPPActressMAS, behind CAM_TRANSPORT_ACTRESSMAS (sources joined to the core library).
#
# A client of the ContainerServer of ActressMAS v3.1 (C#): no dependency but the sockets of the system (Winsock on
# Windows). The codec of its frames, ActressMasCodec, is part of the core whatever the option.

set(CAM_ACTRESSMAS_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/ActressMasTransport.cpp ${CMAKE_CURRENT_LIST_DIR}/ActressMasTransport.h
	${CMAKE_CURRENT_LIST_DIR}/Socket.cpp ${CMAKE_CURRENT_LIST_DIR}/Socket.h PARENT_SCOPE)
if (WIN32)
	set(CAM_ACTRESSMAS_LIBRARIES ws2_32 PARENT_SCOPE)
endif()
