set(LIB_NAME tmms-net)
# 添加文件，目录下的全部
file(GLOB_RECURSE DIR_LIB_SRCS ./*.cpp) 
# 要编译成lib的文件，引用目录下的全部文件
add_library(${LIB_NAME} ${DIR_LIB_SRCS})

add_dependencies(${LIB_NAME}
    tmms-base
)

target_link_libraries(tmms-net PUBLIC tmms-base)

force_redefine_file_macro_for_sources(${LIB_NAME})