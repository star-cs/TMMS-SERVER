file(GLOB TEST_SOURCES "base/test_*.cpp" "network/test_*.cpp" "coro_uring_net/test_*.cpp" "mmedia/test_*.cpp")

# 遍历每个测试源文件
foreach(TEST_SOURCE ${TEST_SOURCES})
    # 从文件名提取测试名称（不含路径、不含扩展名）
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    # 创建测试可执行程序
    add_executable(${TEST_NAME} ${TEST_SOURCE})

    add_dependencies(${TEST_NAME} 
        tmms-base
        tmms-net
        tmms-coro_uring_net
        tmms-mmedia
        libyamlcpp
        libspdlog
        libgtest
    )
    
    target_link_libraries(${TEST_NAME}        
        tmms-base
        tmms-net
        tmms-coro_uring_net
        tmms-mmedia
        libyaml-cpp.a
        libspdlog.a
        libgtest.a
    )

    force_redefine_file_macro_for_sources(${TEST_NAME})
endforeach()
