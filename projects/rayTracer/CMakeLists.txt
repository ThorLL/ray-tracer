
set(libraries glad glfw imgui )

file(GLOB_RECURSE target_inc "*.h" )
file(GLOB_RECURSE target_src "*.cpp" )

file(GLOB_RECURSE resources/shaders "*.vert" "*.frag" "*.geom" "*.glsl")
source_group("resources/shaders" FILES ${shaders})

add_executable(${TARGETNAME} ${target_inc} ${target_src} ${shaders})
target_link_libraries(${TARGETNAME} ${libraries})
