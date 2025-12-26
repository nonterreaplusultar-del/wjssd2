//
// Created by creeper on 7/20/24.
//
#include <fmt/core.h>
#include <meshark/mesh-simplifier.h>

namespace meshark {

Vertex MeshSimplifier::collapseEdge(Edge e) {
  assert(mesh.isCollapsable(e));

  HalfEdge h = e->halfEdge();
  HalfEdge h_twin = h->twin;
  HalfEdge h_left_next = h->next;
  HalfEdge h_left_prev = h_left_next->next;
  HalfEdge h_right_next = h_twin->next;
  HalfEdge h_right_prev = h_right_next->next;

  Edge e_delete_left = h_left_prev->edge;
  Edge e_delete_right = h_right_next->edge;

  Edge e_keep_left = h_left_next->edge;
  Edge e_keep_right = h_right_prev->edge;

  Vertex v_keep = h->tip;
  Vertex v_remove = h->tail;
  Vertex v_left = h_left_next->tip;
  Vertex v_right = h_right_prev->tail;

  Face f_left = h->face;
  Face f_right = h_twin->face;

  for (HalfEdge h : v_remove->outgoingHalfEdges()) {
    h->tail = v_keep;
    h->twin->tip = v_keep;
  }

  v_keep->halfEdge() = h_right_prev->twin;
  v_left->halfEdge() = h_left_next->twin;
  v_right->halfEdge() = h_right_next->twin;

  h_left_next->twin->twin = h_left_prev->twin;
  h_left_prev->twin->twin = h_left_next->twin;
  h_right_next->twin->twin = h_right_prev->twin;
  h_right_prev->twin->twin = h_right_next->twin;

  h_left_next->twin->edge = h_left_prev->twin->edge = e_keep_left;
  h_right_next->twin->edge = h_right_prev->twin->edge = e_keep_right;

  e_keep_left->halfEdge() = h_left_next->twin;
  e_keep_right->halfEdge() = h_right_prev->twin;

  Q.removeVertexData(v_remove);

  mesh.removeFace(f_left);
  mesh.removeFace(f_right);

  mesh.removeHalfEdge(h);
  mesh.removeHalfEdge(h_twin);
  mesh.removeHalfEdge(h_left_next);
  mesh.removeHalfEdge(h_left_prev);
  mesh.removeHalfEdge(h_right_next);
  mesh.removeHalfEdge(h_right_prev);

  eraseEdgeMapping(e);
  edge_collapse_cost.removeEdgeData(e);
  mesh.removeEdge(e);

  eraseEdgeMapping(e_delete_left);
  edge_collapse_cost.removeEdgeData(e_delete_left);
  mesh.removeEdge(e_delete_left);

  eraseEdgeMapping(e_delete_right);
  edge_collapse_cost.removeEdgeData(e_delete_right);
  mesh.removeEdge(e_delete_right);

  mesh.removeVertex(v_remove);

  return v_keep;
}

MeshSimplifier::MinCostEdgeCollapsingResult
MeshSimplifier::collapseMinCostEdge() {
  auto min_cost_edge = cost_edge_map.begin()->second;

  if (!mesh.isCollapsable(min_cost_edge)) {
    return {min_cost_edge, false};
  }

  glm::vec3 pos_opt = computeOptimalCollapsePosition(min_cost_edge);
  Vertex v_survive = collapseEdge(min_cost_edge);
  updateVertexPos(v_survive, pos_opt);

  return {min_cost_edge, true};
}

Real MeshSimplifier::computeEdgeCost(Edge e) const {
  glm::vec3 v_opt = computeOptimalCollapsePosition(e);
  glm::vec4 v_hom(v_opt, 1);
  Vertex v1 = e->firstVertex();
  Vertex v2 = e->secondVertex();

  glm::mat4 Q1 = Q(v1);
  glm::mat4 Q2 = Q(v2);
  glm::mat4 Q_sum = Q1 + Q2;

  return glm::dot(v_hom, Q_sum * v_hom);
}

void MeshSimplifier::runSimplify(Real alpha) {
  for (auto v : mesh.vertices())
    Q(v) = computeQuadricMatrix(v);
  for (auto e : mesh.edges()) {
    edge_collapse_cost(e) = computeEdgeCost(e);
    cost_edge_map.insert({edge_collapse_cost(e), e});
  }
  int round = 0;
  while (mesh.numEdges() > alpha * num_original_edges) {
    auto result = collapseMinCostEdge();
    round++;
    std::cout << fmt::format("Round {}: ", round);
    if (!result.is_collapsable) {
      auto e = result.failed_edge;
      updateEdgeCost(e, std::numeric_limits<Real>::infinity());
      std::cout << "Min-cost edge is not collapsable, skip\n";
      continue;
    }
    std::cout << fmt::format("{} edges left\n", mesh.numEdges());
  }
}

glm::vec3 MeshSimplifier::computeOptimalCollapsePosition(Edge e) const {
  Vertex v1 = e->firstVertex();
  Vertex v2 = e->secondVertex();

  glm::mat4 Q_sum = Q(v1) + Q(v2);

  glm::mat4 A = Q_sum;
  A[0][3] = 0.0f;
  A[1][3] = 0.0f;
  A[2][3] = 0.0f;
  A[3][3] = 1.0f;

  glm::vec4 b(0.0f, 0.0f, 0.0f, 1.0f);
  if (std::abs(glm::determinant(A)) > 1e-7) {
    glm::mat4 A_inv = glm::inverse(A);
    glm::vec4 v_opt = A_inv * b;
    return glm::vec3(v_opt);
  }

  glm::vec3 p1 = mesh.pos(v1);
  glm::vec3 p2 = mesh.pos(v2);
  glm::vec3 p_mid = (p1 + p2) * 0.5f;

  auto calculate_cost = [&](const glm::vec3 &p) -> double {
    glm::vec4 v(p, 1.0f);
    return glm::dot(v, Q_sum * v);
  };

  double cost1 = calculate_cost(p1);
  double cost2 = calculate_cost(p2);
  double cost_mid = calculate_cost(p_mid);

  if (cost_mid < cost1 && cost_mid < cost2)
    return p_mid;
  if (cost1 < cost2)
    return p1;
  return p2;
}

void MeshSimplifier::updateVertexPos(Vertex v, const glm::vec3 &pos) {
  mesh.setVertexPos(v, pos);

  std::vector<Vertex> vertices_to_update;
  vertices_to_update.push_back(v);
  for (HalfEdge h : v->outgoingHalfEdges()) {
    vertices_to_update.push_back(h->tip);
  }

  auto edgeComparator = [](Edge a, Edge b) { return a.get() < b.get(); };
  std::set<Edge, decltype(edgeComparator)> edges_to_update(edgeComparator);

  for (Vertex u : vertices_to_update) {
    Q(u) = computeQuadricMatrix(u);
    for (HalfEdge h : u->outgoingHalfEdges()) {
      edges_to_update.insert(h->edge);
    }
  }

  for (Edge e : edges_to_update) {
    Real new_cost = computeEdgeCost(e);
    updateEdgeCost(e, new_cost);
  }
}

glm::mat4 MeshSimplifier::computeQuadricMatrix(Vertex v) const {
  glm::mat4 Q(0.0f);
  for (HalfEdge h : v->outgoingHalfEdges()) {
    Face f = h->face;
    if (!f)
      continue;
    glm::vec3 n = mesh.normal(f);
    glm::vec3 v_pos = mesh.pos(v);
    float d = -glm::dot(n, v_pos);
    glm::vec4 p(n.x, n.y, n.z, d);
    Q += glm::outerProduct(p, p);
  }
  return Q;
}

void MeshSimplifier::eraseEdgeMapping(Edge e) {
  Real cost = edge_collapse_cost(e);
  auto range = cost_edge_map.equal_range(cost);
  assert(range.first != cost_edge_map.end());
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == e) {
      cost_edge_map.erase(it);
      break;
    }
  }
}
} // namespace meshark