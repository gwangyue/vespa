// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.transaction;

/**
 * An auto closeable mutex
 *
 * @author bratseth
 */
public interface Mutex extends AutoCloseable {

    public void close();

}
