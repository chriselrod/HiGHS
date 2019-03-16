#include "mip/SolveMip.h"

#include <cmath>

// For the moment just return first violated.
NodeIndex NodeStack::chooseBranchingVariable(const Node& node) {
  if (node.integer_variables.size() == 0)
    return kNodeIndexError;
  
  assert(node.integer_variables.size() == node.primal_solution.size());

  for (int col=0; col<node.integer_variables.size(); col++) {
    if (!node.integer_variables[col])
      continue;

    double value = node.primal_solution[col];
    if (std::fabs(value - std::floor(value)) > 0.0000001) {
				// This one is violated.
        return NodeIndex(col);
			}
  }

  return kNoNodeIndex;
}

bool NodeStack::branch(Node& node) {
  NodeIndex branch_col = chooseBranchingVariable(node);
  if (branch_col == kNodeIndexError ||
      branch_col == kNoNodeIndex)
    return false;

  // Pop current node from stack before adding children.
  pop();

  // Branch.
  // Create children and add to node.
  num_nodes++;
  node.left_child = std::unique_ptr<Node>(new Node(node.id, num_nodes, node.level + 1));
  num_nodes++;
  node.right_child = std::unique_ptr<Node>(new Node(node.id, num_nodes, node.level + 1));

  // Copy bounds from parent.
  node.left_child->col_lower_bound = node.col_lower_bound;
  node.left_child->col_upper_bound = node.col_upper_bound;
  node.right_child->col_lower_bound = node.col_lower_bound;
  node.right_child->col_upper_bound = node.col_upper_bound;

  int col = static_cast<int>(branch_col);
  double value = node.primal_solution[col];

  node.left_child->col_upper_bound[col] = std::floor(value);
  node.right_child->col_lower_bound[col] = std::ceil(value);

  // Add to stack.
  stack_.push(*(node.left_child.get()));
  stack_.push(*(node.right_child.get()));

  return true;
}