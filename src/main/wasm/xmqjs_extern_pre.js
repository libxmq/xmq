/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

// Prepended (emcc --extern-pre-js) outside of the emscripten module of xmqjs, the xmq
// command line compiled to wasm for node.
//
// The wasm code runs on the native stack of the node thread, which is about 1MB for the
// main thread. That is not enough for the parsers, they accept 10000 nested elements
// (tests/test_max_depth_parsers.sh needs about 4MB). node --stack-size cannot be raised
// above the stack of the main thread (8MB on Linux and macOS, 1MB on Windows), so the main
// thread runs this same file in a worker thread with a stack of its own, and exits with
// its exit code. The worker reads and writes the file descriptors directly (NODERAWFS).
//
// The main thread must not touch process.stdout or process.stderr when they are pipes:
// node then makes the file descriptor non-blocking, and the synchronous writes of the
// worker fail when the pipe is full, ie output is lost. For the same reason the stdout
// and stderr streams of the worker (only used by the runtime, eg for an abort message)
// are not piped to the ones of the main thread, but written with fs.writeSync.
var xmqjs_worker_threads = require('worker_threads');
if (xmqjs_worker_threads.isMainThread)
{
    var xmqjs_fs = require('fs');
    var xmqjs_worker = new xmqjs_worker_threads.Worker(__filename,
    {
        argv: process.argv.slice(2),
        // The window size of a terminal is only known in the main thread.
        workerData: require('tty').isatty(1) ? { rows: process.stdout.rows, columns: process.stdout.columns } : {},
        stdout: true,
        stderr: true,
        resourceLimits: { stackSizeMb: 64 }
    });
    xmqjs_worker.stdout.on('data', function(chunk) { xmqjs_fs.writeSync(1, chunk); });
    xmqjs_worker.stderr.on('data', function(chunk) { xmqjs_fs.writeSync(2, chunk); });
    xmqjs_worker.on('error', function(e)
    {
        xmqjs_fs.writeSync(2, 'xmqjs: ' + (e && e.stack ? e.stack : e) + '\n');
        process.exitCode = 1;
    });
    xmqjs_worker.on('exit', function(code)
    {
        if (process.exitCode === undefined) process.exitCode = code;
    });
    return;
}
