// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// A small command line around libxmq for javascript, so that the existing test runners
// (tests/test_single.sh, tests/test_pipes.sh, ...) can run with it as the xmq binary.
// It accepts only the subset of the xmq command line that these tests use:
//   FILE|- tokenize --type=debugtokens|debugcontent
//   -i CONTENT tokenize --type=...
//   tokenize --type=...   (reads stdin)
// Anything else is an error with exit code 2, so that a test outside the subset is not
// mistaken for a pass. The package directory is $LIBXMQ_JS, default build/libxmq_js/package.
import fs from "node:fs";
import path from "node:path";

const pkg = path.resolve(process.env.LIBXMQ_JS || "build/libxmq_js/package");
const lib = await import(path.join(pkg, "esm/index.js"));

function unsupported(why)
{
    process.stderr.write("libxmq_js_cli: not supported: " + why + "\n");
    process.exit(2);
}

const args = process.argv.slice(2);
let content = null;
let source = null;
let i = 0;
if (args[i] === "-i")
{
    if (i + 1 >= args.length) unsupported("-i without content");
    content = args[i + 1];
    source = "-i argument";
    i += 2;
}
else if (args[i] === "-" || args[i] === "tokenize")
{
    // Like xmq, the input is stdin when the command comes first.
    if (args[i] === "-") i++;
    content = fs.readFileSync(0, "utf8");
    source = "stdin";
}
else if (args[i] !== undefined && !args[i].startsWith("-"))
{
    source = args[i];
    content = fs.readFileSync(source, "utf8");
    i++;
}
else
{
    unsupported("no input: " + args.join(" "));
}

const cmd = args[i++];
if (cmd === "tokenize")
{
    const opt = args[i++];
    const type = opt === "--type=debugtokens" ? "debugtokens" : opt === "--type=debugcontent" ? "debugcontent" : null;
    if (type === null || i < args.length) unsupported("tokenize " + args.slice(i - 1).join(" "));
    try
    {
        process.stdout.write(lib.tokenize(content, type, source));
    }
    catch (e)
    {
        if (!(e instanceof lib.XMQParseError)) throw e;
        process.stderr.write(e.message + "\n");
        process.exit(1);
    }
}
else
{
    unsupported("command " + (cmd === undefined ? "(none)" : cmd));
}
