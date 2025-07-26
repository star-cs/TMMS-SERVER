include(ExternalProject)
include_directories(${PROJECT_BINARY_DIR}/include)
link_directories(${PROJECT_BINARY_DIR}/lib)
link_directories(${PROJECT_BINARY_DIR}/lib64)

ExternalProject_Add(libspdlog
    EXCLUDE_FROM_ALL 1
    GIT_REPOSITORY https://gitee.com/jbl19860422/spdlog.git
    GIT_TAG v1.14.0
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make -j4 -C build 
    INSTALL_COMMAND make -C build install
)

ExternalProject_Add(libyamlcpp
    EXCLUDE_FROM_ALL 1
    URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND cmake -B build -DYAML_CPP_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make -C build  -j4
    INSTALL_COMMAND make -C build install -j4
)

ExternalProject_Add(libjsoncpp
    EXCLUDE_FROM_ALL 1
    URL https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.8.4.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make -C build -j4
    INSTALL_COMMAND make -C build install -j4
)

ExternalProject_Add(libgtest
    EXCLUDE_FROM_ALL 1
    URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make -C build -j4
    INSTALL_COMMAND make -C build install -j4 
)

ExternalProject_Add(libatomic_queue
    EXCLUDE_FROM_ALL 1
    URL https://github.com/max0x7ba/atomic_queue/archive/refs/tags/v1.7.1.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)

include_directories(${PROJECT_BINARY_DIR}/libatomic_queue-prefix/src/libatomic_queue/include)


# 自己编译安装
# ./configure --cc=gcc --cxx=g++;
# make -j$(nproc);
# make liburing.pc
# sudo make install;

ExternalProject_Add(liburing
    EXCLUDE_FROM_ALL 1
    URL https://github.com/axboe/liburing/archive/refs/tags/liburing-2.10.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)


