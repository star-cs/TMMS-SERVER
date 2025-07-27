set(LIB_NAME tmms-mmedia)
# 添加文件，目录下的全部
file(GLOB_RECURSE DIR_LIB_SRCS ./*.cpp ./*.hpp) 
# 要编译成lib的文件，引用目录下的全部文件
add_library(${LIB_NAME} ${DIR_LIB_SRCS})

add_dependencies(${LIB_NAME}
    tmms-base
    tmms-net
)

target_link_libraries(${LIB_NAME} PUBLIC tmms-base tmms-net)

force_redefine_file_macro_for_sources(${LIB_NAME})