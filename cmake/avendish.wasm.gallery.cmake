# SPDX-License-Identifier: GPL-3.0-or-later
# Gallery index generator, run via `cmake -P`. Emits OUT_DIR/index.html linking
# every built plug-in's <c_name>.html.
#   GALLERY_ENTRIES : ";"-separated "c_name|DisplayName" entries
#   OUT_DIR         : wasm/ output directory

if(NOT DEFINED OUT_DIR)
  message(FATAL_ERROR "OUT_DIR not set")
endif()

set(_rows "")
if(DEFINED GALLERY_ENTRIES)
  foreach(_entry IN LISTS GALLERY_ENTRIES)
    if(_entry STREQUAL "")
      continue()
    endif()
    string(REPLACE "|" ";" _pair "${_entry}")
    list(GET _pair 0 _cname)
    list(LENGTH _pair _len)
    if(_len GREATER 1)
      list(GET _pair 1 _display)
    else()
      set(_display "${_cname}")
    endif()
    string(APPEND _rows
      "    <a href=\"${_cname}.html\">${_display} <code>${_cname}</code></a>\n")
  endforeach()
endif()

set(_html
"<!doctype html>
<html><head><meta charset=\"utf-8\"><title>Avendish &#8594; WebAssembly</title>
<style>
 body{font:16px/1.5 system-ui,sans-serif;max-width:680px;margin:40px auto;padding:0 16px}
 h1{font-weight:600}
 a{display:flex;justify-content:space-between;align-items:center;
   padding:12px 16px;margin:8px 0;border:1px solid #d0d7de;border-radius:8px;
   text-decoration:none;color:#0969da;background:#fff}
 a:hover{background:#f6f8fa;border-color:#0969da}
 a code{color:#656d76;font-size:13px}
 .note{color:#656d76;font-size:14px;margin-top:24px}
</style></head>
<body>
 <h1>Avendish &#8594; WebAssembly</h1>
 <p>Plug-ins compiled from C++ Avendish sources. Each page sets up an
    AudioContext + AudioWorklet and an auto-generated UI ; controls are live
    before audio starts.</p>
${_rows}
 <p class=\"note\">Served with <code>Cross-Origin-Opener-Policy: same-origin</code> +
   <code>Cross-Origin-Embedder-Policy: require-corp</code> so
   <code>crossOriginIsolated</code> is true.</p>
</body></html>
")

file(WRITE "${OUT_DIR}/index.html" "${_html}")
message(STATUS "Avendish WASM gallery index: ${OUT_DIR}/index.html")
