/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

// Prepended (emcc --pre-js) to the xmq command line compiled to wasm for node (xmqjs).
// The program runs with NODERAWFS, ie it reads and writes the real file system.

// In the build the executable is build/xmqjs and the wasm file is
// build/xmqjs.wasm/xmqjs.wasm, in the node package xmqjs.wasm is next to xmqjs.
Module['locateFile'] = function(path, dir)
{
    var file = dir + path;
    var fs = require('fs');
    if (fs.existsSync(file) && fs.statSync(file).isDirectory())
    {
        file = file + '/' + path;
    }
    return file;
};

// The program runs in a worker thread (see xmqjs_extern_pre.js). Emscripten ends it by
// throwing an ExitStatus, which a worker reports as an error, so exit the worker with the
// status instead. The main thread exits with the exit code of the worker.
Module['quit'] = function(status, toThrow)
{
    process.exit(status);
};

Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function()
{
    // The emscripten runtime has its own environment, give it the one of the node process.
    Object.assign(ENV, process.env);

    // An xmqjs installed from the node package finds the grammar library next to itself,
    // like the share/xmq/library of make install. XMQ_INSTALLED_LIBRARY can still override it.
    var library = require('path').join(__dirname, 'library');
    if (ENV['XMQ_INSTALLED_LIBRARY'] === undefined && require('fs').existsSync(library))
    {
        ENV['XMQ_INSTALLED_LIBRARY'] = library;
    }

    // NODERAWFS marks stdin, stdout and stderr as terminals unconditionally (emscripten
    // issue 22264), then xmq would print colors into a pipe. Its tty is also just true,
    // without the ops that the terminal ioctls (isatty, tcgetattr) call, which crashes on
    // a real terminal. So keep the tty only for a real terminal, and give it the window
    // size. The terminal settings (raw mode) are not changed. The standard streams are
    // created when the runtime is initialized, which is after preRun.
    addOnInit(function()
    {
        var tty = require('tty');
        for (var fd = 0; fd < 3; fd++)
        {
            var stream = FS.getStream(fd);
            if (!stream) continue;
            // The runtime closes all streams at exit, but stdin, stdout and stderr belong
            // to the main thread, node warns when a worker closes them. An extra reference
            // keeps them open.
            if (stream.shared) stream.shared.refcnt++;
            if (!tty.isatty(fd))
            {
                stream.tty = undefined;
                continue;
            }
            stream.tty =
            {
                ops:
                {
                    ioctl_tiocgwinsz: function()
                    {
                        // Measured by the main thread, see xmqjs_extern_pre.js.
                        var size = require('worker_threads').workerData || {};
                        return [size.rows || 24, size.columns || 80];
                    }
                }
            };
        }
    });
});
