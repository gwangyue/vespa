// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include <vespa/vespalib/stllike/string.h>

namespace search::attribute {

class IAttributeVector;

/*
 * Interface class for access attribute in correct attribute write
 * thread as async callback from asyncForEachAttribute() call on
 * attribute manager.
 */
class IConstAttributeFunctor
{
public:
    virtual void operator()(const IAttributeVector &attributeVector) = 0;
    virtual ~IConstAttributeFunctor() { }
};

class IAttributeFunctor
{
public:
    virtual void operator()(IAttributeVector &attributeVector) = 0;
    virtual ~IAttributeFunctor() { }
};

class IAttributeExecutor {
public:
    virtual ~IAttributeExecutor() { }
    virtual void asyncForAttribute(const vespalib::string &name, std::unique_ptr<IAttributeFunctor> func) const = 0;
};

}