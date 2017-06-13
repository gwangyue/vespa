// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.system;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.StringTokenizer;

import com.yahoo.collections.Pair;

/**
 * Executes a system command synchronously.
 *
 * @author bratseth
 */
public class ProcessExecuter {

    /**
     * Executes the given command synchronously without timeout.
     * 
     * @return Retcode and stdout/stderr merged
     */
    public Pair<Integer, String> exec(String command) throws IOException {
        StringTokenizer tok = new StringTokenizer(command);
        List<String> tokens = new ArrayList<>();
        while (tok.hasMoreElements()) tokens.add(tok.nextToken());
        return exec(tokens.toArray(new String[0]));
    }
    
    /**
     * Executes the given command synchronously without timeout.
     * 
     * @param command tokens
     * @return Retcode and stdout/stderr merged
     */
    public Pair<Integer, String> exec(String[] command) throws IOException {
        ProcessBuilder pb = new ProcessBuilder(command);        
        StringBuilder ret = new StringBuilder();
        pb.environment().remove("VESPA_LOG_TARGET");
        pb.redirectErrorStream(true);
        Process p = pb.start();
        InputStream is = p.getInputStream();
        while (true) {
            int b = is.read();
            if (b==-1) break;
            ret.append((char)b);
        }
        int rc=0;
        try {
            rc = p.waitFor();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
        return new Pair<>(rc, ret.toString());
    }

}
