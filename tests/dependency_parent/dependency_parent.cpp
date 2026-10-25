#include "dependency_parent.h"

#include <dependency_leaf.h>

int dependency_parent_init() {
    return dependency_leaf_value();
}

void dependency_parent_destroy() {}
