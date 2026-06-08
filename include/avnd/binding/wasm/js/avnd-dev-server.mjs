/* SPDX-License-Identifier: GPL-3.0-or-later */
// Static file server for the WASM standalone build. Sets COOP/COEP so the page
// is cross-origin isolated and SharedArrayBuffer is available.
// Usage: node avnd-dev-server.mjs [dir] [--port N] [--host H]

import http from "node:http";
import { createReadStream, promises as fs } from "node:fs";
import path from "node:path";

const MIME = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".wasm": "application/wasm",
  ".css": "text/css; charset=utf-8",
  ".map": "application/json; charset=utf-8",
  ".wav": "audio/wav",
  ".mp3": "audio/mpeg",
  ".ogg": "audio/ogg",
  ".png": "image/png",
  ".svg": "image/svg+xml",
  ".ico": "image/x-icon",
};

function parseArgs(argv) {
  const out = { _: [] };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--port" || a === "-p") out.port = +argv[++i];
    else if (a === "--host" || a === "-h") out.host = argv[++i];
    else out._.push(a);
  }
  return out;
}

const args = parseArgs(process.argv.slice(2));
const ROOT = path.resolve(args._[0] ?? process.cwd());
const PORT = args.port ?? 8080;
const HOST = args.host ?? "127.0.0.1";

function setHeaders(res, ext, size) {
  // COOP/COEP: required for cross-origin isolation (SharedArrayBuffer).
  res.setHeader("Cross-Origin-Opener-Policy", "same-origin");
  res.setHeader("Cross-Origin-Embedder-Policy", "require-corp");
  res.setHeader("Cross-Origin-Resource-Policy", "same-origin");
  res.setHeader("Content-Type", MIME[ext] ?? "application/octet-stream");
  if (size != null) res.setHeader("Content-Length", size);
  res.setHeader("Cache-Control", "no-cache");
}

const server = http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host}`);
    let pathname = decodeURIComponent(url.pathname);
    if (pathname === "/") pathname = "/index.html";

    // Contain within ROOT (no path traversal).
    const resolved = path.normalize(path.join(ROOT, pathname));
    if (!resolved.startsWith(ROOT)) {
      res.writeHead(403).end("Forbidden");
      return;
    }

    let stat;
    try {
      stat = await fs.stat(resolved);
    } catch {
      res.writeHead(404, { "Content-Type": "text/plain" }).end("404 Not Found");
      return;
    }

    let filePath = resolved;
    if (stat.isDirectory()) {
      filePath = path.join(resolved, "index.html");
      try {
        stat = await fs.stat(filePath);
      } catch {
        res.writeHead(404, { "Content-Type": "text/plain" }).end("404 Not Found");
        return;
      }
    }

    const ext = path.extname(filePath).toLowerCase();
    setHeaders(res, ext, stat.size);
    res.writeHead(200);
    createReadStream(filePath).pipe(res);
  } catch (e) {
    res.writeHead(500, { "Content-Type": "text/plain" }).end("500 " + e.message);
  }
});

server.listen(PORT, HOST, () => {
  console.log(`Avendish dev server (COOP/COEP isolated) serving:`);
  console.log(`  root: ${ROOT}`);
  console.log(`  url : http://${HOST}:${PORT}/`);
  console.log(`Headers set: Cross-Origin-Opener-Policy: same-origin,`);
  console.log(`             Cross-Origin-Embedder-Policy: require-corp`);
});

export { server };
