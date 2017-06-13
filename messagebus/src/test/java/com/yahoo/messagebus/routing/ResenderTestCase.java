// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.messagebus.routing;

import com.yahoo.jrt.ListenFailedException;
import com.yahoo.jrt.slobrok.server.Slobrok;
import com.yahoo.messagebus.*;
import com.yahoo.messagebus.Error;
import com.yahoo.messagebus.network.Identity;
import com.yahoo.messagebus.network.rpc.RPCNetworkParams;
import com.yahoo.messagebus.network.rpc.test.TestServer;
import com.yahoo.messagebus.test.Receptor;
import com.yahoo.messagebus.test.SimpleMessage;
import com.yahoo.messagebus.test.SimpleProtocol;

import java.net.UnknownHostException;

/**
 * @author <a href="mailto:simon@yahoo-inc.com">Simon Thoresen</a>
 */
public class ResenderTestCase extends junit.framework.TestCase {

    ////////////////////////////////////////////////////////////////////////////////
    //
    // Setup
    //
    ////////////////////////////////////////////////////////////////////////////////

    Slobrok slobrok;
    TestServer srcServer, dstServer;
    SourceSession srcSession;
    DestinationSession dstSession;
    RetryTransientErrorsPolicy retryPolicy;

    @Override
    public void setUp() throws ListenFailedException, UnknownHostException {
        slobrok = new Slobrok();
        dstServer = new TestServer(new MessageBusParams().addProtocol(new SimpleProtocol()),
                                   new RPCNetworkParams().setIdentity(new Identity("dst")).setSlobrokConfigId(TestServer.getSlobrokConfig(slobrok)));
        dstSession = dstServer.mb.createDestinationSession(new DestinationSessionParams().setName("session").setMessageHandler(new Receptor()));
        retryPolicy = new RetryTransientErrorsPolicy();
        retryPolicy.setBaseDelay(0);
        srcServer = new TestServer(new MessageBusParams().setRetryPolicy(retryPolicy).addProtocol(new SimpleProtocol()),
                                   new RPCNetworkParams().setSlobrokConfigId(TestServer.getSlobrokConfig(slobrok)));
        srcSession = srcServer.mb.createSourceSession(
                new SourceSessionParams().setTimeout(600.0).setReplyHandler(new Receptor()));
        assertTrue(srcServer.waitSlobrok("dst/session", 1));
    }

    @Override
    public void tearDown() {
        slobrok.stop();
        dstSession.destroy();
        dstServer.destroy();
        srcSession.destroy();
        srcServer.destroy();
    }

    ////////////////////////////////////////////////////////////////////////////////
    //
    // Tests
    //
    ////////////////////////////////////////////////////////////////////////////////

    public void testRetryTag() {
        assertTrue(srcSession.send(createMessage("msg"), Route.parse("dst/session")).isAccepted());
        Message msg = ((Receptor)dstSession.getMessageHandler()).getMessage(60);
        assertNotNull(msg);
        for (int i = 0; i < 5; ++i) {
            assertEquals(i, msg.getRetry());
            assertEquals(true, msg.getRetryEnabled());
            replyFromDestination(msg, ErrorCode.APP_TRANSIENT_ERROR, 0);
            assertNotNull(msg = ((Receptor)dstSession.getMessageHandler()).getMessage(60));
        }
        dstSession.acknowledge(msg);
        Reply reply = ((Receptor)srcSession.getReplyHandler()).getReply(60);
        assertNotNull(reply);
        assertFalse(reply.hasErrors());
        assertNull(((Receptor)dstSession.getMessageHandler()).getMessage(0));
        System.out.println(reply.getTrace());
    }

    public void testRetryEnabledTag() {
        Message msg = createMessage("msg");
        msg.setRetryEnabled(false);
        assertTrue(srcSession.send(msg, Route.parse("dst/session")).isAccepted());
        assertNotNull(msg = ((Receptor)dstSession.getMessageHandler()).getMessage(60));
        assertEquals(false, msg.getRetryEnabled());
        replyFromDestination(msg, ErrorCode.APP_TRANSIENT_ERROR, 0);
        Reply reply = ((Receptor)srcSession.getReplyHandler()).getReply(60);
        assertNotNull(reply);
        assertTrue(reply.hasErrors());
        assertNull(((Receptor)dstSession.getMessageHandler()).getMessage(0));
        System.out.println(reply.getTrace());
    }

    public void testTransientError() {
        assertTrue(srcSession.send(createMessage("msg"), Route.parse("dst/session")).isAccepted());
        Message msg = ((Receptor)dstSession.getMessageHandler()).getMessage(60);
        assertNotNull(msg);
        replyFromDestination(msg, ErrorCode.APP_TRANSIENT_ERROR, 0);
        assertNotNull(msg = ((Receptor)dstSession.getMessageHandler()).getMessage(60));
        replyFromDestination(msg, ErrorCode.APP_FATAL_ERROR, 0);
        Reply reply = ((Receptor)srcSession.getReplyHandler()).getReply(60);
        assertNotNull(reply);
        assertTrue(reply.hasFatalErrors());
        assertNull(((Receptor)dstSession.getMessageHandler()).getMessage(0));
    }

    public void testFatalError() {
        assertTrue(srcSession.send(createMessage("msg"), Route.parse("dst/session")).isAccepted());
        Message msg = ((Receptor)dstSession.getMessageHandler()).getMessage(60);
        assertNotNull(msg);
        replyFromDestination(msg, ErrorCode.APP_FATAL_ERROR, 0);
        Reply reply = ((Receptor)srcSession.getReplyHandler()).getReply(60);
        assertNotNull(reply);
        assertTrue(reply.hasFatalErrors());
        assertNull(((Receptor)dstSession.getMessageHandler()).getMessage(0));
    }

    public void testDisableRetry() {
        retryPolicy.setEnabled(false);
        assertTrue(srcSession.send(createMessage("msg"), Route.parse("dst/session")).isAccepted());
        Message msg = ((Receptor)dstSession.getMessageHandler()).getMessage(60);
        assertNotNull(msg);
        replyFromDestination(msg, ErrorCode.APP_TRANSIENT_ERROR, 0);
        Reply reply = ((Receptor)srcSession.getReplyHandler()).getReply(60);
        assertNotNull(reply);
        assertTrue(reply.hasErrors());
        assertTrue(!reply.hasFatalErrors());
        assertNull(((Receptor)dstSession.getMessageHandler()).getMessage(0));
    }

    public void testRetryDelay() {
        retryPolicy.setBaseDelay(0.01);
        assertTrue(srcSession.send(createMessage("msg"), Route.parse("dst/session")).isAccepted());
        Message msg = ((Receptor)dstSession.getMessageHandler()).getMessage(60);
        assertNotNull(msg);
        for (int i = 0; i < 5; ++i) {
            replyFromDestination(msg, ErrorCode.APP_TRANSIENT_ERROR, -1);
            assertNotNull(msg = ((Receptor)dstSession.getMessageHandler()).getMessage(60));
        }
        replyFromDestination(msg, ErrorCode.APP_FATAL_ERROR, 0);
        Reply reply = ((Receptor)srcSession.getReplyHandler()).getReply(60);
        assertNotNull(reply);
        assertTrue(reply.hasFatalErrors());
        assertNull(((Receptor)dstSession.getMessageHandler()).getMessage(0));

        String trace = reply.getTrace().toString();
        assertTrue(trace.contains("retry 1 in 0.01"));
        assertTrue(trace.contains("retry 2 in 0.02"));
        assertTrue(trace.contains("retry 3 in 0.03"));
        assertTrue(trace.contains("retry 4 in 0.04"));
        assertTrue(trace.contains("retry 5 in 0.05"));
    }

    public void testRequestRetryDelay() {
        assertTrue(srcSession.send(createMessage("msg"), Route.parse("dst/session")).isAccepted());
        Message msg = ((Receptor)dstSession.getMessageHandler()).getMessage(60);
        assertNotNull(msg);
        for (int i = 0; i < 5; ++i) {
            replyFromDestination(msg, ErrorCode.APP_TRANSIENT_ERROR, i / 50.0);
            assertNotNull(msg = ((Receptor)dstSession.getMessageHandler()).getMessage(60));
        }
        replyFromDestination(msg, ErrorCode.APP_FATAL_ERROR, 0);
        Reply reply = ((Receptor)srcSession.getReplyHandler()).getReply(60);
        assertNotNull(reply);
        assertTrue(reply.hasFatalErrors());
        assertNull(((Receptor)dstSession.getMessageHandler()).getMessage(0));

        String trace = reply.getTrace().toString();
        System.out.println(trace);
        assertTrue(trace.contains("retry 1 in 0"));
        assertTrue(trace.contains("retry 2 in 0.02"));
        assertTrue(trace.contains("retry 3 in 0.04"));
        assertTrue(trace.contains("retry 4 in 0.06"));
        assertTrue(trace.contains("retry 5 in 0.08"));
    }

    ////////////////////////////////////////////////////////////////////////////////
    //
    // Utilities
    //
    ////////////////////////////////////////////////////////////////////////////////

    private static Message createMessage(String msg) {
        SimpleMessage ret = new SimpleMessage(msg);
        ret.getTrace().setLevel(9);
        return ret;
    }

    private void replyFromDestination(Message msg, int errorCode, double retryDelay) {
        Reply reply = new EmptyReply();
        reply.swapState(msg);
        if (errorCode != ErrorCode.NONE) {
            reply.addError(new Error(errorCode, "err"));
        }
        reply.setRetryDelay(retryDelay);
        dstSession.reply(reply);
    }
}
