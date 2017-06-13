// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#include "intermediatesessionparams.h"

namespace mbus {

IntermediateSessionParams::IntermediateSessionParams() :
    _name("intermediate"),
    _broadcastName(true),
    _msgHandler(nullptr),
    _replyHandler(nullptr)
{ }

} // namespace mbus
