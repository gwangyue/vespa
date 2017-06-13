// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.model.content.storagecluster;

import com.yahoo.vespa.config.content.core.StorVisitorConfig;
import com.yahoo.vespa.model.builder.xml.dom.ModelElement;

/**
 * Serves stor-visitor config for storage clusters.
 */
public class StorVisitorProducer implements StorVisitorConfig.Producer {
    public static class Builder {
        public StorVisitorProducer build(ModelElement element) {
            ModelElement tuning = element.getChild("tuning");
            if (tuning == null) {
                return new StorVisitorProducer();
            }

            ModelElement visitors = tuning.getChild("visitors");
            if (visitors == null) {
                return new StorVisitorProducer();
            }

            return new StorVisitorProducer(visitors.getIntegerAttribute("thread-count"),
                                           visitors.getIntegerAttribute("max-queue-size"),
                                           visitors.childAsInteger("max-concurrent.fixed"),
                                           visitors.childAsInteger("max-concurrent.variable"));
        }
    }

    Integer threadCount;
    Integer maxQueueSize;
    Integer maxConcurrentFixed;
    Integer maxConcurrentVariable;

    public StorVisitorProducer() {}

    StorVisitorProducer(Integer threadCount, Integer maxQueueSize, Integer maxConcurrentFixed, Integer maxConcurrentVariable) {
        this.threadCount = threadCount;
        this.maxQueueSize = maxQueueSize;
        this.maxConcurrentFixed = maxConcurrentFixed;
        this.maxConcurrentVariable = maxConcurrentVariable;
    }

    @Override
    public void getConfig(StorVisitorConfig.Builder builder) {
        if (threadCount != null) {
            builder.visitorthreads(threadCount);
        }
        if (maxQueueSize != null) {
            builder.maxvisitorqueuesize(maxQueueSize);
        }
        if (maxConcurrentFixed != null) {
            builder.maxconcurrentvisitors_fixed(maxConcurrentFixed);
        }
        if (maxConcurrentVariable != null) {
            builder.maxconcurrentvisitors_variable(maxConcurrentVariable);
        }
    }
}
