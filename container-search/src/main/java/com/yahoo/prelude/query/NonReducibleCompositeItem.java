// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.prelude.query;

/**
 * A composite item which specifies semantics which are not maintained
 * if an instance with a single child is replaced by the single child.
 * <p>
 * Most composites, like AND and OR, are reducible as e.g (AND a) is semantically equal to (a).
 * <p>
 * This type functions as a marked interfaces for query rewriters.
 *
 * @author bratseth
 * @since 5.1.22
 */
public abstract class NonReducibleCompositeItem extends CompositeItem {
}
