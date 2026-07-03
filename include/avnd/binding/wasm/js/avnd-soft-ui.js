// SPDX-License-Identifier: GPL-3.0-or-later
//
// Browser glue for the soft-framebuffer UI shell
// (avnd/binding/ui/soft/wasm.hpp): blits the C++ RGBA framebuffer into a
// <canvas> via putImageData and forwards pointer events in physical canvas
// pixels. Companion of the DOM/Canvas2D path in avnd-ui.js.
//
// The C++ side (module, frame data contract, pointer events) is validated
// headlessly under node (tests/ui/wasm_soft_ui_test.mjs); this DOM glue
// itself still awaits a manual browser run.
//
//   import { attachSoftUI } from "./avnd-soft-ui.js";
//   const handle = attachSoftUI(new Module.SoftUI(), canvas, { width: 640, height: 480 });
//   ...
//   handle.stop();

export function attachSoftUI(ui, canvas, opts = {}) {
  const logicalW = opts.width ?? (ui.logicalWidth ? ui.logicalWidth() : 640);
  const logicalH = opts.height ?? (ui.logicalHeight ? ui.logicalHeight() : 480);
  const dpr = opts.devicePixelRatio ?? (globalThis.devicePixelRatio || 1);

  ui.resize(logicalW, logicalH, dpr);
  const physW = ui.physicalWidth();
  const physH = ui.physicalHeight();

  canvas.width = physW;
  canvas.height = physH;
  canvas.style.width = `${logicalW}px`;
  canvas.style.height = `${logicalH}px`;
  const ctx = canvas.getContext("2d");

  // Pointer events arrive in CSS pixels; the runtime expects physical.
  const toPhysical = (ev) => {
    const rect = canvas.getBoundingClientRect();
    return [
      ((ev.clientX - rect.left) / rect.width) * physW,
      ((ev.clientY - rect.top) / rect.height) * physH,
    ];
  };

  const onMove = (ev) => {
    const [x, y] = toPhysical(ev);
    ui.pointerMove(x, y);
  };
  const onDown = (ev) => {
    canvas.setPointerCapture?.(ev.pointerId);
    const [x, y] = toPhysical(ev);
    ui.pointerButton(x, y, true);
  };
  const onUp = (ev) => {
    const [x, y] = toPhysical(ev);
    ui.pointerButton(x, y, false);
  };
  const onWheel = (ev) => {
    const [x, y] = toPhysical(ev);
    ui.wheel(x, y, -ev.deltaY / 120);
    ev.preventDefault();
  };

  canvas.addEventListener("pointermove", onMove);
  canvas.addEventListener("pointerdown", onDown);
  canvas.addEventListener("pointerup", onUp);
  canvas.addEventListener("wheel", onWheel, { passive: false });

  let raf = 0;
  const tick = () => {
    // frame() returns a zero-copy view over the wasm heap
    const pixels = ui.frame();
    const image = new ImageData(new Uint8ClampedArray(pixels), physW, physH);
    ctx.putImageData(image, 0, 0);
    raf = requestAnimationFrame(tick);
  };
  raf = requestAnimationFrame(tick);

  return {
    stop() {
      cancelAnimationFrame(raf);
      canvas.removeEventListener("pointermove", onMove);
      canvas.removeEventListener("pointerdown", onDown);
      canvas.removeEventListener("pointerup", onUp);
      canvas.removeEventListener("wheel", onWheel);
    },
  };
}
