# Strict warnings for our own (non-JUCE) code: -Wall -Wextra (+ extras) on
# GCC/Clang and /W4 on MSVC, as required by the project rules. Warnings are
# errors in CI (OE_WARNINGS_AS_ERRORS=ON) so regressions are caught early.
option(OE_WARNINGS_AS_ERRORS "Treat warnings in Orbit Echo sources as errors" OFF)

function(oe_set_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(OE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
            -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused
            -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion)
        if(OE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Lighter set for translation units that include JUCE headers (JUCE itself is
# not -Wconversion clean on every compiler).
function(oe_set_plugin_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()
