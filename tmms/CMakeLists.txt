add_subdirectory(base)
add_subdirectory(network)
add_subdirectory(coro_uring_net)

add_executable(tmms-live-server main.cpp)

add_dependencies(tmms-live-server
    tmms-base
    libyamlcpp
    libspdlog
    libatomic_queue
)

target_link_libraries(tmms-live-server
    tmms-base
    libyaml-cpp.a
    libspdlog.a
)

force_redefine_file_macro_for_sources(tmms-live-server)