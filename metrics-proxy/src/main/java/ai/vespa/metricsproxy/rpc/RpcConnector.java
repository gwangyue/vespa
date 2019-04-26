/*
 * Copyright 2019 Oath Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
 */

package ai.vespa.metricsproxy.rpc;

import com.yahoo.component.AbstractComponent;
import com.yahoo.jrt.Acceptor;
import com.yahoo.jrt.ListenFailedException;
import com.yahoo.jrt.Method;
import com.yahoo.jrt.Spec;
import com.yahoo.jrt.Supervisor;
import com.yahoo.jrt.Transport;

import java.util.logging.Logger;

import static com.yahoo.log.LogLevel.DEBUG;
import static java.util.logging.Level.INFO;

/**
 * Contains the connector for the rpc server, to prevent it from going down after component reconfiguration.
 * This will only be recreated if the rpc port changes, which should never happen under normal circumstances.
 *
 * @author gjoranv
 */
public class RpcConnector extends AbstractComponent {
    private static final Logger log = Logger.getLogger(RpcConnector.class.getName());

    private final Supervisor supervisor = new Supervisor(new Transport());
    private final Acceptor acceptor;

    public RpcConnector(RpcConnectorConfig config) {
        Spec spec = new Spec(config.port());
        try {
            acceptor = supervisor.listen(spec);
            log.log(DEBUG, "Listening on " + spec.host() + ":" + spec.port());
        } catch (ListenFailedException e) {
            stop();
            log.log(INFO, "Failed listening at " + spec.host() + ":" + spec.port());
            throw new RuntimeException("Could not listen at " + spec, e);
        }
    }

    /**
     * Adds a method. If a method with the same name already exists, it will be replaced.
     * @param method The method to add.
     */
    public void addMethod(Method method) {
        supervisor.addMethod(method);
    }

    public void stop() {
        acceptor.shutdown().join();
        supervisor.transport().shutdown().join();
    }

    @Override
    public void deconstruct() {
        stop();
        super.deconstruct();
    }
}
