#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

namespace avnd
{
template <typename T>
concept painter = requires(T t) {
                    // Paths:
                    t.begin_path();
                    t.close_path();
                    t.stroke();
                    t.fill();

                    //        x , y
                    t.move_to(0., 0.);
                    t.line_to(0., 0.);

                    //       x , y , w , h , startAngle, arcLength
                    t.arc_to(0., 1., 2., 3., 11., 12.);

                    //         c1x, c1y, c2x, c2y, endx, endy
                    t.cubic_to(0., 1., 2., 3., 11., 12.);
                    //        x1, y1, x2, y2
                    t.quad_to(0., 1., 2., 3.);

                    // Transformations:
                    //          x , y
                    t.translate(0., 0.);
                    t.scale(0., 0.);
                    t.rotate(0.);
                    t.reset_transform();

                    // Colors:
                    //                  R    G    B    A
                    t.set_stroke_color({255, 255, 255, 127});
                    t.set_stroke_width(2.);
                    t.set_fill_color({255, 255, 255, 127});

                    // Text:
                    t.set_font("Comic Sans");
                    t.set_font_size(10.0); // In points

                    //          x , y , text
                    t.draw_text(0., 0., "Hello World");

                    // Drawing
                    //          x1, y1, x2 , y2
                    t.draw_line(0., 0., 10., 10.);

                    //          x , y , w  , h
                    t.draw_rect(0., 0., 10., 10.);

                    //                  x , y , w  , h  , r
                    t.draw_rounded_rect(0., 0., 10., 10., 5.);

                    //            x , y , filename
                    t.draw_pixmap(0., 0., "pixmap");

                    //             x , y , w  , h
                    t.draw_ellipse(0., 0., 10., 10.);

                    //            cx, cy, radius
                    t.draw_circle(0., 0., 20.);

                    //           x, y, w, h, bytes, img_w, img_h
                    t.draw_bytes(0., 0., 0., 0., (unsigned char*)nullptr, 0, 0);
                    t.draw_bytes(0., 0., 0., 0., (float*)nullptr, 0, 0);
                  };
}
