#include "meshark/geometry-mesh.h"
#include "meshark/mesh-io.h"
#include "meshark/mesh-simplifier.h"
#include <filesystem>
#include <iostream>

int main() {
  namespace fs = std::filesystem;

  fs::path input_path = "assets/armadillo.obj";
  fs::path output_path = "assets/armadillo_simplified.obj";

  // 1. 直接读取 GeometryMesh
  auto mesh_ptr = meshark::readGeometryMeshFromWavefrontObj(input_path);
  if (!mesh_ptr) {
    std::cerr << "Failed to load OBJ file: " << input_path << std::endl;
    return 1;
  }
  auto &mesh = *mesh_ptr;

  std::cout << "Loaded mesh: " << mesh.numVertices() << " vertices, "
            << mesh.numFaces() << " faces." << std::endl;

  // 2. 创建网格简化器
  meshark::MeshSimplifier simplifier(mesh);

  // 3. 运行简化，alpha = 0.5（50%简化）
  simplifier.runSimplify(0.5);

  std::cout << "After simplification: " << mesh.numVertices() << " vertices, "
            << mesh.numFaces() << " faces." << std::endl;

  // 4. 保存简化后的 OBJ
  fs::create_directories(output_path.parent_path());
  mesh.writeWavefrontObj(output_path);

  std::cout << "Simplified mesh saved to " << output_path << std::endl;
  return 0;
}
