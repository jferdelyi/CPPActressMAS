# Runs the command given after "--" and reports a non-zero exit code: cmake -P run_checked.cmake -- <command> [args...]
#
# A test that passes on a regular expression ignores the exit code of its program (CTest): a program that prints the
# expected line then exits with an error would pass. Behind this script, its output goes through unchanged and an error
# exit code, or a signal, prints "cam: exit code <code>", which the FAIL_REGULAR_EXPRESSION of the test catches
# (cam_fail_on_sanitizer_report). An argument cannot contain a semicolon: CMake would split it.
set(l_command)
set(l_after_separator FALSE)
math(EXPR l_last "${CMAKE_ARGC} - 1")
foreach (l_index RANGE 1 ${l_last})
	if (l_after_separator)
		list(APPEND l_command "${CMAKE_ARGV${l_index}}")
	elseif (CMAKE_ARGV${l_index} STREQUAL "--")
		set(l_after_separator TRUE)
	endif()
endforeach()
if (NOT l_command)
	message(FATAL_ERROR "usage: cmake -P run_checked.cmake -- <command> [args...]")
endif()

execute_process(COMMAND ${l_command} RESULT_VARIABLE l_result)
if (NOT l_result STREQUAL "0")
	message(FATAL_ERROR "cam: exit code ${l_result}")
endif()
