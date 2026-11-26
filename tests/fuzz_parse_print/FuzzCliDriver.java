/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

import java.io.ByteArrayOutputStream;
import java.io.PrintStream;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Base64;
import java.util.List;

/**
 * Runs many xmqj command lines in one JVM, for tests/fuzz_parse_print.mjs: starting the
 * JVM for every case is too slow. Every line of the manifest is one command line, the
 * arguments separated by \u0001. For every line one line is written to the output file:
 * {"rc":N,"out":"base64","err":"base64"}, the exit code and the stdout and stderr of
 * Cli.run, like build/xmqj would print them.
 *   java -cp build/classes tests/fuzz_parse_print/FuzzCliDriver.java MANIFEST OUTPUT
 */
public class FuzzCliDriver
{
    public static void main(String[] args) throws Exception
    {
        List<String> lines = Files.readAllLines(Path.of(args[0]), StandardCharsets.UTF_8);
        Method run = Class.forName("org.libxmq.imp.Cli").getDeclaredMethod("run", String[].class);
        run.setAccessible(true);
        StringBuilder result = new StringBuilder();
        PrintStream real_out = System.out;
        PrintStream real_err = System.err;
        Throwable[] failure = new Throwable[1];
        // Like Main, a large stack for deep documents.
        Thread t = new Thread(null, () ->
        {
            try
            {
                for (String line : lines)
                {
                    if (line.isEmpty()) continue;
                    String[] a = line.split("\u0001", -1);
                    ByteArrayOutputStream out = new ByteArrayOutputStream();
                    ByteArrayOutputStream err = new ByteArrayOutputStream();
                    PrintStream po = new PrintStream(out, false, StandardCharsets.UTF_8);
                    PrintStream pe = new PrintStream(err, false, StandardCharsets.UTF_8);
                    System.setOut(po);
                    System.setErr(pe);
                    int rc;
                    try
                    {
                        rc = (Integer)run.invoke(null, (Object)a);
                    }
                    catch (Throwable e)
                    {
                        Throwable c = e.getCause() != null ? e.getCause() : e;
                        pe.print("JAVA EXCEPTION " + c);
                        rc = 99;
                    }
                    po.flush();
                    pe.flush();
                    result.append("{\"rc\":").append(rc)
                          .append(",\"out\":\"").append(Base64.getEncoder().encodeToString(out.toByteArray()))
                          .append("\",\"err\":\"").append(Base64.getEncoder().encodeToString(err.toByteArray()))
                          .append("\"}\n");
                }
            }
            catch (Throwable e)
            {
                failure[0] = e;
            }
        }, "fuzz", 1L << 30);
        t.start();
        t.join();
        System.setOut(real_out);
        System.setErr(real_err);
        if (failure[0] != null) throw new RuntimeException(failure[0]);
        Files.writeString(Path.of(args[1]), result.toString(), StandardCharsets.UTF_8);
    }
}
