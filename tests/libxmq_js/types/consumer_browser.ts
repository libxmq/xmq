// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
// A typescript user of libxmq in a browser: the DOM of the browser, no dom option.
// Only compiled (not run) by tests/libxmq_js/test_009_types.mjs.
import { parse, print, toJson } from "libxmq";

export function show(xmq: string): string
{
    // A Document of the browser.
    const doc: Document = parse(xmq);
    const body: HTMLElement = document.body;
    body.textContent = JSON.stringify(toJson(doc));
    // The page itself as xmq.
    return print(document) + print(doc.documentElement);
}
