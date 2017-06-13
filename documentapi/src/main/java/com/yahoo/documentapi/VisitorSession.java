// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.documentapi;

import com.yahoo.messagebus.Trace;

/**
 * A session for tracking progress for and potentially receiving data from a
 * visitor.
 *
 * @author <a href="mailto:thomasg@yahoo-inc.com">Thomas Gundersen</a>
 */
public interface VisitorSession extends VisitorControlSession {
    /**
     * Checks if visiting is done.
     *
     * @return True if visiting is done (either by error or success).
     */
    public boolean isDone();

    /**
     * Retrieves the last progress token gotten for this visitor.
     *
     * @return The progress token.
     */
    public ProgressToken getProgress();

    /**
     * Returns the tracing information so far about the visitor.
     *
     * @return Returns the trace.
     */
    public Trace getTrace();

    /**
     * Waits until visiting is done, or the given timeout (in ms) expires.
     * Will wait forever if timeout is 0.
     *
     * @param timeoutMs The maximum amount of milliseconds to wait.
     * @return True if visiting is done (either by error or success).
     * @throws InterruptedException If an interrupt signal was received while waiting.
     */
    public boolean waitUntilDone(long timeoutMs) throws InterruptedException;
}
