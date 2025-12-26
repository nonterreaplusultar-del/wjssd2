cmake_minimum_required(VERSION 3.28)
project(MeshSimplification)
find_package(fmt REQUIRED)


set(CMAKE_CXX_STANDARD 20)
add_subdirectory(external/glm)
add_subdirectory(meshark)