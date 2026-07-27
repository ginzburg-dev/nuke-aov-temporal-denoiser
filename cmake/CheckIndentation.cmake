if(NOT DEFINED ROOT)
    message(FATAL_ERROR "ROOT is required")
endif()

file(
    GLOB_RECURSE checked_files
    LIST_DIRECTORIES false
    "${ROOT}/*.cmake"
    "${ROOT}/*.cpp"
    "${ROOT}/*.h"
    "${ROOT}/*.md"
    "${ROOT}/*.txt"
    "${ROOT}/*.yaml"
    "${ROOT}/*.yml"
)

string(ASCII 9 tab_character)
set(errors)

foreach(path IN LISTS checked_files)
    if(path MATCHES "/\\.git/" OR path MATCHES "/build[^/]*/")
        continue()
    endif()

    file(STRINGS "${path}" lines)
    set(line_number 0)

    foreach(line IN LISTS lines)
        math(EXPR line_number "${line_number} + 1")
        string(REGEX MATCH "^[ \t]*" indentation "${line}")
        string(FIND "${indentation}" "${tab_character}" tab_position)

        if(NOT tab_position EQUAL -1)
            list(APPEND errors "${path}:${line_number}: tab indentation")
            continue()
        endif()

        string(LENGTH "${indentation}" indentation_width)
        math(EXPR indentation_remainder "${indentation_width} % 4")
        if(NOT indentation_remainder EQUAL 0)
            list(
                APPEND errors
                "${path}:${line_number}: ${indentation_width} leading spaces"
            )
        endif()
    endforeach()
endforeach()

if(errors)
    list(JOIN errors "\n" error_report)
    message(
        FATAL_ERROR
        "Indentation must use spaces in multiples of four:\n${error_report}"
    )
endif()

message(STATUS "Indentation uses spaces in multiples of four")
