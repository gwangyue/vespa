// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#include "floatbucketresultnode.h"
#include "floatresultnode.h"

namespace search {
namespace expression {

const BucketResultNode& FloatResultNode::getNullBucket() const {
    return FloatBucketResultNode::getNull();
}

}
}

