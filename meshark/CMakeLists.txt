project(meshark) # 建议与库名一致

# 自动收集源码
file(GLOB_RECURSE SOURCES src/*.cpp src/*.cc include/*.h)

# 定义库
add_library(meshark ${SOURCES})



target_include_directories(meshark PUBLIC include)
target_link_libraries(meshark PUBLIC glm)
target_link_libraries(meshark PUBLIC glm fmt::fmt)

# 定义可执行文件
add_executable(simplify apps/simplify.cc)
target_link_libraries(simplify PRIVATE meshark)

