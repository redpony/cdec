#include <cmath>

#include "veb_tree.h"

VEBTree::VEBTree(int size) {
  int num_bits = ceil(log2(size));

  lower_bits = num_bits >> 1;
  upper_size = (size >> lower_bits) + 1;

  clusters.reserve(upper_size);
  clusters.resize(upper_size);
}

int VEBTree::GetNextValue(int value) {
  return value & ((1 << lower_bits) - 1);
}

int VEBTree::GetCluster(int value) {
  return value >> lower_bits;
}

int VEBTree::Compose(int cluster, int value) {
  return (cluster << lower_bits) + value;
}

void VEBTree::Insert(int value) {
  if (min == -1 && max == -1) {
    min = max = value;
    return;
  }

  if (value < min) {
    swap(min, value);
  }

  int cluster = GetCluster(value), next_value = GetNextValue(value);
  if (clusters[cluster] == NULL) {
    clusters[cluster] = VEB::Create(1 << lower_bits);
    if (summary == NULL) {
      summary = VEB::Create(upper_size);
    }
    summary->Insert(cluster);
  }
  clusters[cluster]->Insert(next_value);

  if (value > max) {
    max = value;
  }
}

int VEBTree::GetSuccessor(int value) {
  if (value >= max) {
    return -1;
  }
  if (value < min) {
    return min;
  }

  int cluster = GetCluster(value), next_value = GetNextValue(value);
  if (clusters[cluster] != NULL &&
      next_value < clusters[cluster]->GetMaximum()) {
    return Compose(cluster, clusters[cluster]->GetSuccessor(next_value));
  } else {
    int next_cluster = summary->GetSuccessor(cluster);
    if (next_cluster == -1) {
      return -1;
    }
    return Compose(next_cluster, clusters[next_cluster]->GetMinimum());
  }
}
