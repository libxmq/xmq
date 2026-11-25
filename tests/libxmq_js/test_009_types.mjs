// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// The typescript types (.d.ts) of the package, as a typescript user sees them: the
// consumers in tests/libxmq_js/types are compiled with tsc against the built package,
// found through the exports of its package.json as node_modules/libxmq:
//   consumer_esm.mts      import, in node with @xmldom/xmldom, also @ts-expect-error misuse
//   consumer_cjs.cts      require, in node with @xmldom/xmldom
//   consumer_browser.ts   the DOM of a browser (only compiled)
// The node consumers are then run. Also every name that index.d.ts exports must have a
// doc comment where it is declared.
//   node tests/libxmq_js/test_009_types.mjs build/libxmq_js/package OUTPUT_DIR
// tsc is the one of the build tools, next to the package (build/libxmq_js/tools).
import { spawnSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";

const pkg = path.resolve(process.argv[2]);
const output = path.join(path.resolve(process.argv[3]), "types");
const tools = path.join(path.dirname(pkg), "tools", "node_modules");
const tsc = path.join(tools, ".bin", "tsc");
const src = path.resolve("tests/libxmq_js/types");

let failed = 0;
function err(m)
{
    console.log("ERR: types " + m);
    failed++;
}

fs.rmSync(output, { recursive: true, force: true });
fs.mkdirSync(path.join(output, "node_modules"), { recursive: true });
fs.symlinkSync(pkg, path.join(output, "node_modules", "libxmq"));
fs.symlinkSync(path.join(tools, "@xmldom"), path.join(output, "node_modules", "@xmldom"));
for (const f of fs.readdirSync(src)) fs.copyFileSync(path.join(src, f), path.join(output, f));
const options = { strict: true, target: "es2020", lib: ["es2020", "dom"], skipLibCheck: false };
fs.writeFileSync(path.join(output, "tsconfig.node.json"), JSON.stringify({
    compilerOptions: { ...options, module: "nodenext", moduleResolution: "nodenext", outDir: "out" },
    files: ["consumer_esm.mts", "consumer_cjs.cts"],
}));
fs.writeFileSync(path.join(output, "tsconfig.browser.json"), JSON.stringify({
    compilerOptions: { ...options, module: "esnext", moduleResolution: "bundler", noEmit: true },
    files: ["consumer_browser.ts"],
}));

for (const cfg of ["tsconfig.node.json", "tsconfig.browser.json"])
{
    const r = spawnSync(tsc, ["-p", cfg], { cwd: output, encoding: "utf8" });
    if (r.status !== 0) err("tsc -p " + cfg + "\n" + r.stdout + r.stderr);
}
let runs = 0;
if (failed === 0)
{
    for (const f of ["consumer_esm.mjs", "consumer_cjs.cjs"])
    {
        const r = spawnSync("node", [path.join("out", f)], { cwd: output, encoding: "utf8" });
        const out = (r.stdout + r.stderr).trim();
        if (r.status !== 0 || !out.startsWith("OK")) err("node " + f + "\n" + out);
        else runs++;
    }
}

// Every exported name has a doc comment where it is declared.
const esm = path.join(pkg, "esm");
const index = fs.readFileSync(path.join(esm, "index.d.ts"), "utf8");
const names = new Map(); // name -> d.ts file
for (const m of index.matchAll(/export \{([^}]*)\} from "\.\/([a-z]+)\.js"/g))
{
    for (let n of m[1].split(","))
    {
        n = n.trim().replace(/^type /, "");
        if (n !== "") names.set(n, m[2] + ".d.ts");
    }
}
for (const m of index.matchAll(/^export (?:declare )?(?:function|class|interface|type|enum) ([A-Za-z_]+)/gm)) names.set(m[1], "index.d.ts");
let documented = 0;
for (const [n, file] of names)
{
    const lines = fs.readFileSync(path.join(esm, file), "utf8").split("\n");
    const k = lines.findIndex(l => new RegExp("^export (?:declare )?(?:function|class|interface|type|enum) " + n + "\\b").test(l));
    if (k < 0) err("the declaration of " + n + " not found in " + file);
    else if (k === 0 || !lines[k - 1].trim().endsWith("*/")) err(n + " in " + file + " has no doc comment");
    else documented++;
}

if (failed === 0) console.log("OK: types (3 consumers compile, " + runs + " run, " + documented + " exported names documented)");
process.exit(failed === 0 ? 0 : 1);
