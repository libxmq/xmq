// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// The xmq inputs of the libxmq for javascript tests that compare with the C xmq: every
// tracked .xmq and .htmq file (not the copied libxml2/libxslt suites) and the START
// section of every tests/*.test file, also those that are not xmq.
import { execFileSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";

/** The inputs as [name, text]. */
export function xmq_inputs()
{
    const list = [];
    const files = execFileSync("git", ["ls-files", "*.xmq", "*.htmq"], { encoding: "utf8" }).split("\n")
        .filter(f => f !== "" && !f.startsWith("tests/libxml2") && !f.startsWith("tests/libxslt"));
    for (const f of files) list.push([f, fs.readFileSync(f, "utf8")]);
    for (const f of fs.readdirSync("tests").filter(f => f.endsWith(".test")).sort())
    {
        const lines = fs.readFileSync(path.join("tests", f), "utf8").split("\n");
        const s = lines.indexOf("START");
        if (s < 0) continue;
        let e = s + 1;
        while (e < lines.length && !/^[A-Z_]+$/.test(lines[e])) e++;
        list.push([f.replace(/\.test$/, ""), lines.slice(s + 1, e).join("\n") + "\n"]);
    }
    return list;
}
