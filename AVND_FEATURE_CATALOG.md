# Avendish Authoring-API Feature Catalog

Exhaustive catalog of the authoring surface of the Avendish media-plugin library, intended to drive a battery of unit-test plugin objects covering every (feature × format × in/out) combination across every backend.

Two authoring styles are covered throughout:

- **Low-level `avnd`** — write a plain aggregate `struct` whose members conform to the C++20 concepts in `include/avnd/concepts/*.hpp`. No headers strictly required; introspection is structural (Boost.PFR + concepts).
- **High-level `halp`** — ergonomic helper types in `include/halp/*.hpp` that satisfy those concepts with much less boilerplate.

Everything below cites `path:line`. The avnd concept layer is the **ground truth** for what is "supported" — a backend only needs the structural shape the concept checks for.

General object shape (README.md:108+, examples/Raw/AllPortsTypes.hpp:51):

```cpp
struct MyObject {
  static consteval auto name()   { return "My Object"; }   // or halp_meta(name, "...")
  static consteval auto c_name() { return "my_object"; }
  static consteval auto uuid()   { return "....-....-...."; }
  struct inputs  { /* ports */ } inputs;     // or `struct inputs {...};` as a *type*
  struct outputs { /* ports */ } outputs;
  void operator()();                          // many forms — see §12
};
```

`inputs`/`outputs` may be a **value member** (`} inputs;`) or a **type** (`struct inputs {...};`) — both are detected (`avnd/concepts/port.hpp:13,16`; `inputs_is_type`/`inputs_is_value`/`has_inputs`). The same applies to `messages`, `ui`, `layout`, `schedule`, `clocks`, `hardware`, `gpio`/`pwm`/... (concepts use the `type_or_value_qualification` macro family, `avnd/concepts/generic.hpp:215`).

---

## 1. Control / parameter inputs

A **parameter** is anything with a default-constructible `.value` member (`avnd/concepts/parameter.hpp:52`, `parameter_port`). It becomes a **control** (gets a GUI widget) iff it also has a `widget` enum (`avnd/concepts/widget.hpp:12`, `has_widget`; `avnd/concepts/control.hpp:33`, `control_port`). A parameter **without** a widget is a `value_port` (`avnd/concepts/control.hpp:67`). Range comes from a static `range()` fn or nested `range`/`struct range` (`avnd/concepts/range.hpp:12`, `has_range`).

The halp control widgets carry `enum widget { ... }` listing acceptable widget aliases, a `static range()`/`struct range`, a `static name()`, a `value`, and conversion operators. The backend-facing widget vocabulary is enumerated in `avnd/wrappers/widgets.hpp:20` (`widget_type`): bang, button, toggle, slider, spinbox, knob, iknob, lineedit, combobox, choices, xy, xy_spinbox, xyz, xyz_spinbox, xyzw, xyzw_spinbox, color, time_chooser, folder, bargraph, range_slider, range_spinbox, multi_slider, multi_slider_xy, path_generator_xy, log_slider, log_knob, control, no_control.

### Scalar value-type parameter concepts (`avnd/concepts/parameter.hpp`)

| Concept | Line | Detects |
|---|---|---|
| `int_parameter` | 55 | `.value` is integer-ish |
| `float_parameter` | 69 | `.value` is floating-point |
| `bool_parameter` | 74 | `.value` is bool |
| `string_parameter` | 79 | `.value` convertible to `std::string` |
| `enum_parameter` | 60 | `.value` is an enum |
| `enum_ish_parameter` | 64 | enum, or `range.values[0]` exists |
| `xy_parameter`/`xyz_parameter`/`xyzw_parameter` | 89/98/125 | `.value` has `.x .y`(`.z`)(`.w`) |
| `uv_parameter`/`uvw_parameter` | 106/115 | `.value` has `.u .v`(`.w`) |
| `rgb_parameter`/`rgba_parameter` | 134/144 | `.value` has `.r .g .b`(`.a`) |
| `span_parameter_port` | 186 | `.value` is span-like (multi-slider, arrays) |
| `parameter_with_minmax_range` | 152 | has `range.min/max/init` |
| `parameter_with_values_range` | 160 | has `range.values[0]` (enums / combo) |
| `program_parameter` | 166 | string param + static `language()` (code editor) |
| `smooth_parameter_port` | 255 | `has_range && has_smooth` |

Control vs value distinction: `int_control_port`/`float_control_port`/`bool_control_port`/`string_control_port`/`enum_control_port`/`time_control_port` (`control.hpp:40-50`); `*_value_port` mirrors (`control.hpp:53-72`).

### halp range / init primitives (`halp/controls.sliders.hpp`)

- `halp::range_t<T>{min,max,init}`, alias `range` (long double) / `irange` (int) — `controls.sliders.hpp:14-19`.
- Defaults: `default_range<T>` `{0,1,0.5}` / int `{0,127,64}`; `positive_range_min/max`, `free_range_min/max` (`controls.sliders.hpp:22-43`).
- `init_range_t<T>{init}` for init-only controls (`controls.sliders.hpp:46`).
- `halp::ratio{num,denom}`, `halp::combo_pair<T>{first,second}`, `range_slider_value<T>{start,end}` (`halp/value_types.hpp:101-117`).

### halp control widget types

Sliders/knobs/spinboxes/bargraphs (`halp/controls.hpp`, `halp/controls.sliders.hpp`):

| halp type | widget enum | def line |
|---|---|---|
| `hslider_t`/`hslider_f32`/`hslider_i32` | hslider, slider | controls.hpp:109,171,182 |
| `vslider_t`/`vslider_f32`/`vslider_i32` | vslider, slider | controls.hpp:119,185,187 |
| `log_hslider_f32` | log_slider | controls.hpp:173 |
| `spinbox_t`/`spinbox_f32`/`spinbox_i32` | spinbox | controls.hpp:130,189,191 |
| `knob_t`/`knob_f32`/`knob_i32` | knob | controls.hpp:140,200,202 |
| `iknob_t`/`iknob_f32` | iknob (float value, int widget) | controls.hpp:151,205 |
| `hbargraph_t`/`hbargraph_f32/i32` | hbargraph, bargraph | controls.hpp:78,256 |
| `vbargraph_t`/`vbargraph_f32/i32` | vbargraph, bargraph | controls.hpp:93,261 |
| `toggle_t`/`toggle`/`toggle_f32` | toggle, checkbox | controls.hpp:160,212,215 |
| `smooth_knob`/`smooth_slider`/`smooth_control<T>` | + `using smooth=` | halp/smooth_controls.hpp:10-25 |

Snippet: `halp::hslider_f32<"Gain", halp::range{.min=0., .max=2., .init=1.}> gain;` (examples/Raw/WasmWidgetTypes.hpp:30).

Buttons / bangs (`halp/controls.buttons.hpp`):

- `maintained_button_t`/`maintained_button` — widget `button, pushbutton`; `bool value` (buttons.hpp:14,36 in controls.hpp:218).
- `impulse_button_t`/`impulse_button` — widget `bang, impulse`; `std::optional<impulse_type> value` (buttons.hpp:36; controls.hpp:194). Matches `impulse` semantics — used as a momentary trigger.

Enums / combobox (`halp/controls.enums.hpp`):

- `enum_t<Enum, lit>` — widget `enumeration, list, combobox`; uses `avnd::enum_names<Enum>()` reflection; range `{values[], init}` (enums.hpp:17). `static_assert` enforces contiguous, 0-based enums (enums.hpp:31-43).
- `string_enum_t<Enum, lit>` — `std::string value`, assignable from enum/int/string (enums.hpp:74).
- `combobox_t<lit, Enum>` — narrows widget to `combobox` only (enums.hpp:123).
- `halp__enum(Name, default, A, B, ...)` macro — defines `enum_type`, widget enum, and `range.values[]` string array in one go (enums.hpp:227).
- `halp__enum_combobox(Name, default, ...)` — `range.values` is `pair<string_view, enum_type>[]` (enums.hpp:265).
- `combo_pair<T>` form: `struct range { halp::combo_pair<float> values[3]{{"Foo",0.1f},...}; int init; }` (examples/Helpers/Controls.hpp:59).
- `multi_slider` widget: `enum widget { multi_slider }; std::vector<float> value;` (examples/Raw/WasmWidgetTypes.hpp:65).

Vectors / pads / colors (`halp/controls.sliders.hpp`):

| halp type | value_type / widget | line |
|---|---|---|
| `xy_pad_t`/`xy_pad_f32` | `xy_type<T>` / xy | sliders.hpp:124; controls.hpp:242 |
| `xy_spinboxes_t`/`_f32`/`_i32` | `xy_type<T>` / xy_spinbox,spinbox | sliders.hpp:149; controls.hpp:245-247 |
| `xyz_spinboxes_t`/`_f32` | `xyz_type<T>` / xyz,spinbox | sliders.hpp:178; controls.hpp:250 |
| `xyzw_spinboxes_t`/`_f32` | `xyzw_type<T>` / xyzw,spinbox | sliders.hpp:207; controls.hpp:253 |
| `range_slider_t`/`range_slider_f32` | `range_slider_value<T>` / hrange_slider | sliders.hpp:266; controls.hpp:225 |
| `range_spinbox_t`/`range_spinbox_f32` | range_spinbox | sliders.hpp:275; controls.hpp:228 |
| `color_chooser<lit, color_init>` | `color_type{r,g,b,a}` / color | sliders.hpp:287 |
| `time_chooser_t`/`time_chooser` | float / time_chooser, has `bool sync` | sliders.hpp:77; controls.hpp:222 |

`color_type` is `float r,g,b,a` (value_types.hpp:67); discrete color structs `r_color`, `rgb_color`, `rgba_color`, `rgba32f_color` (value_types.hpp:80-98).

String / file / folder / path:

- `lineedit_t`/`lineedit<lit, "init">` — widget `lineedit, textedit, text`; `std::string value` (controls.hpp:35).
- `file_port<lit, FileType>` — FileType ∈ `text_file_view`/`binary_file_view`/`mmap_file_view`; `.file.bytes/.filename` (halp/file_port.hpp:52; concepts `file_port.hpp:16`, `raw_file_port:26`). Tag `file_watch` (`concepts/file_port.hpp:34`) triggers re-read on change (example Helpers/FileChange.hpp).
- `folder_port<lit>` — widget `folder`; `std::string value` (file_port.hpp:65).
- `addr_port<lit,T>` — value port carrying an OSC `path()` (halp/device.hpp:10).
- `gradient_port<lit>` — widget `gradient`; `gradient_t` (flat_map<float, hunter_lab_color>) (halp/gradient_port.hpp:113).
- `curve_port<lit, Curve>` — widget `curve`; `linear_curve`/`power_curve`/`custom_curve` (halp/curve.hpp:145; concepts `curve.hpp:51,58`).

### Mappers & smoothing (per-control modifiers)

- `using mapper = halp::log_mapper<std::ratio<95,100>>;` (or `pow_mapper<N>`, `inverse_mapper<M>`) attaches a nonlinear UI/value map (halp/mappers.hpp:13-37; concept `mapper.hpp:15`, `has_mapper:27`). Used on a control via inheritance (examples/Advanced/Audio/Echo.hpp:30-40).
- `using smooth = halp::milliseconds_smooth<15>;` / `exp_smooth<N>` (halp/smoothers.hpp:11-30; concept `smooth.hpp:12`).

---

## 2. Value outputs

Outputs use the same parameter/control concepts (anything in `inputs` can appear in `outputs`). A pure value port is `halp::val_port<lit, T>` (halp/controls.basic.hpp:13; trivial-T specialization at :35). Bargraphs and the like are common as **output displays** (e.g. `vbargraph_f32` output in examples/Helpers/Controls.hpp:87).

- Value/control ports as outputs: `halp::val_port<"Out", float>`, `hbargraph_f32`, `enum_t`, etc.
- **Callbacks out** — `halp::callback<Name, Args...>` (void), `halp::timed_callback<Name, Args...>` (prepends `int64_t` timestamp) (halp/callback.hpp:40,54). Underlying `basic_callback<R(Args...)>` is a function-view {function ptr + context} (callback.hpp:19). Concepts: `view_callback`, `stored_callback`, `callback` (concepts/callback.hpp:27-36); `function_view_ish:17`. A raw `std::function<void(float)>` member named `call` also satisfies it. Snippet examples/Raw/Callback.hpp:46-62 (one host output created per callback). `reactive_value<T>` wraps value + notify callback (halp/reactive_value.hpp:13).
- Output **messages** — see §9.

---

## 3. Audio I/O

Audio-port concepts (`avnd/concepts/audio_port.hpp`) classify by storage shape; both `float` and `double` are accepted everywhere:

| Concept | Line | Shape |
|---|---|---|
| `audio_sample_port` | 14 | `{ FP sample; }` — one sample (per-sample mono) |
| `audio_frame_port` | 33 | `{ FP* frame; }` — one multichannel frame (per-sample poly) |
| `mono_array_sample_port` | 52 | `{ FP* channel; }` — one channel buffer |
| `poly_array_sample_port` | 69 | `{ FP** samples; }` — a bus |
| `mono_audio_port` / `poly_audio_port` | 101/104 | union of above |
| `fixed_poly_audio_port` | 107 | bus + static `channels()` |
| `dynamic_poly_audio_port` | 114 | bus, runtime channels |
| `variable_poly_audio_port` | 118 | dynamic + `request_channels(int)` |

halp audio port types (`halp/audio.hpp`):

| halp type | concept it hits | line |
|---|---|---|
| `audio_sample<Name,FP>` | audio_sample_port | audio.hpp:21 |
| `audio_channel<Name,FP>` | mono_array_sample_port | audio.hpp:38 |
| `fixed_audio_frame<lit,FP,N>` | audio_frame_port (fixed) | audio.hpp:51 |
| `dynamic_audio_frame<Name,FP>` | audio_frame_port (dynamic) | audio.hpp:109 |
| `fixed_audio_bus<lit,FP,N>` | fixed_poly_audio_port | audio.hpp:116 |
| `dynamic_audio_bus<Name,FP>` | dynamic_poly_audio_port | audio.hpp:174 |
| `variable_audio_bus<Name,FP>` (+`request_channels`) | variable_poly_audio_port | audio.hpp:267 |
| `audio_bus<lit,FP>` / `audio_input_bus`/`audio_output_bus` | bus | audio.hpp:913,933 |
| `mimic_audio_bus<lit,&other>` | bus mimicking another port's channel count | audio.hpp:969 |
| `audio_spectrum_channel` / `fixed_/dynamic_audio_spectrum_bus` | FFT spectrum (amplitude/phase) | audio.hpp:181,204,235 |

Processor classification (`avnd/concepts/processor.hpp`): `monophonic_processor:237`, `polyphonic_processor:245`, `sample_port_processor:292`, `frame_port_processor:300`, `channel_port_processor:304`, `bus_port_processor:308`; argument forms `monophonic_arg_audio_effect:109` (`operator()(FP*,FP*,int)`), `polyphonic_arg_audio_effect:115` (`operator()(FP**,FP**,int)`). `cv` and `stateless` tags (`audio_processor.hpp:25,33`).

Per-sample / per-channel / per-bus / per-frame iteration is auto-generated by the process adapters (`avnd/wrappers/process/per_sample_port.hpp`, `per_channel_port.hpp`, `per_frame_port.hpp`, `poly_port.hpp`, plus `process_bus/*`). Examples: Helpers/PerSample.hpp (3 forms), Helpers/PerBus.hpp, Helpers/PerFrame.hpp, Helpers/CustomChannels.hpp.

`prepare(setup)` receives `{input_channels, output_channels, frames, rate, instance, ...}` (avnd/wrappers/prepare.hpp:11; halp/audio.hpp:901 `halp::setup`). Snippet Echo.hpp:49 `void prepare(halp::setup info)`.

---

## 4. MIDI I/O

Message concepts (`avnd/concepts/midi.hpp`): `midi_message:13` (`.bytes` + `.timestamp`), `dynamic_midi_message:19` (resizable bytes), `raw_midi_message:22` (3-byte array). Port concepts (`avnd/concepts/midi_port.hpp`): `midi_port:14` (`.midi_messages`), `dynamic_container_midi_port:17`, `raw_container_midi_port:21`.

halp types (`halp/midi.hpp`):

- `midi_msg{ small_vector<uint8_t,15> bytes; int64_t timestamp; }` (midi.hpp:19); `midi_note_msg{ uint8_t bytes[8]; ... }` (midi.hpp:24).
- `midi_bus<lit, MessageType=midi_msg>` (midi.hpp:31); `midi_note_bus<lit>` (midi.hpp:68).
- `midi_out_bus<lit>` adds `note_on/note_off/control_change/program_change/pitch_bend/poly_pressure/aftertouch` builders (midi.hpp:71-114). Full status-byte enum incl. SysEx/realtime (midi.hpp:117-148).

Snippet pass-through: examples/Helpers/Midi.hpp:26 (`halp::midi_bus<"MIDI">` in and out). Raw byte handling: examples/Raw/Midi.hpp. Examples: Midi/MidiPitch, MidiGlide, MidiSyncInput, MidiHiResInput/Output (timed_callback + accurate), Arpeggiator (tick_musical).

---

## 5. Texture I/O

GFX concepts (`avnd/concepts/gfx.hpp`): `cpu_texture_base:69` (`.bytes .width .height .changed`), `cpu_fixed_format_texture:76` (+ `T::format`), `cpu_dynamic_format_texture:80` (`.format`/`.request_format`), `gpu_dynamic_format_texture:87` (`.handle`), `cpu_texture:92`, `gpu_texture:96`, `cpu_texture_port:99` / `gpu_texture_port:104` (`.texture` member). Also `sampler_port:111`, `image_port:114`, `attachment_port:117`, `gpu_render_target_output_port:120`, `texture_port:124`. Introspection: `avnd/introspection/gfx.hpp`.

### CPU texture formats — `halp/texture_formats.hpp`

Fixed-format CPU textures (each: `bytes`, `width`, `height`, `changed`, `bytes_per_pixel`, `enum format`, `allocate()`, `update()`):

| Type | `enum format` | bytes/pixel | line |
|---|---|---|---|
| `r8_texture` | `R8` | 1 | texture_formats.hpp:14 |
| `r32f_texture` | `R32F` | 4 (float) | texture_formats.hpp:69 |
| `rgba_texture` | `RGBA` | 4 | texture_formats.hpp:123 |
| `rgba32f_texture` | `RGBA32F` | 16 (4×float) | texture_formats.hpp:184 |
| `rgb_texture` | `RGB` | 3 | texture_formats.hpp:260 |

Runtime/variable-format CPU textures — `custom_texture_base` enumerates the full `texture_format` set (texture_formats.hpp:301-320): **RGBA8, BGRA8, R8, RG8, R16, RG16, RED_OR_ALPHA8, RGBA16F, RGBA32F, R16F, R32F, R8UI, R32UI, RG32UI, RGBA32UI**. `custom_texture` (requests a format from host, `request_format`, texture_formats.hpp:408); `custom_variable_texture` (receives host's chosen `format`, texture_formats.hpp:433). `component_size()`/`components()` lookups at :322/:359.

### halp texture ports — `halp/texture.hpp`

- `texture_input<lit, TextureType=rgba_texture>` — specialized for `rgba_texture`, `rgba32f_texture`, `rgb_texture`, `r32f_texture`, `custom_texture`, `custom_variable_texture` (texture.hpp:19-146); `rgb_texture_input` alias (:119); `fixed_texture_input` adds `request_width/height` (:148).
- `texture_output<lit, TextureType=rgba_texture>` — owns `storage`, `create(w,h)`, `set(...)`, `upload()` (texture.hpp:155); `rgb_texture_output` alias (:201).

### GPU textures — `halp/texture.hpp`

- `gpu_texture` — `enum format_t` (RGBA8, BGRA8, R8, RG8, R16, RG16, RGBA16F, RGBA32F, R16F, R32F, RGB10A2, R8UI, R32UI, RG32UI, RGBA32UI, R8SI, R32SI, RG32SI, RGBA32SI) (texture.hpp:235-261); `void* handle`, depth attachment (`depth_format_t` D16/D24/D24S8/D32F), `width/height`, `kind`, `layers_or_depth`, `sampler_handle` (texture.hpp:262-289).
- `texture_kind` enum: `texture_2d, texture_array, cubemap, texture_3d` (texture.hpp:205).
- `gpu_texture_input<lit>` / `gpu_texture_output<lit>` (texture.hpp:291,302).
- Layered/cubemap/3d variants via `halp_meta(texture_target, ...)`: `gpu_cubemap_input/output` (texture.hpp:313,326; cubemap sets layers=6), `gpu_texture_array_input/output` (:337,343), `gpu_texture_3d_input/output` (:354,360), `gpu_sampleable_depth_input` (:319), `gpu_render_target_output` (:371).

CPU texture examples: TextureGeneratorExample.hpp (rgba_texture out), TextureFilterExample.hpp (in+out), TouchDesigner InvertTOP/CheckerboardTOP/BrightnessContrastTOP/BlendTOP, Advanced/Image/LightnessComputer (`fixed_texture_input<rgba32f_texture>`), Helpers/SpriteReader (file→texture_output). GPU pipeline examples use the separate `gpp::` library (see §12 / Gpu/).

---

## 6. Buffer / SSBO I/O

GFX buffer concepts (`avnd/concepts/gfx.hpp`): `cpu_raw_buffer:34` (`.raw_data .byte_size .byte_offset .changed`), `cpu_typed_buffer:40` (`.elements .element_count ...`), `cpu_formatted_buffer:49` (+`T::format`), `gpu_buffer:54` (`.handle .byte_offset`); ports `cpu_raw_buffer_port:57`, `cpu_typed_buffer_port:59`, `gpu_buffer_port:64`, umbrella `buffer_port:66`. `matrix_port:128` = `buffer_port || texture_port`.

halp types (`halp/buffer.hpp`):

- `raw_buffer{ unsigned char* raw_data; int64_t byte_size, byte_offset; bool changed; }` (buffer.hpp:14); `raw_output_buffer` adds `upload(...)` (:27); `typed_buffer<T>{ T* elements; int64_t element_count; bool changed; }` (:32); `gpu_buffer{ void* handle; ... }` (:45).
- Ports: `cpu_buffer_input<lit>` (+`cast<T>()`) / `cpu_buffer_output<lit>` (+`create<T>()/upload()`) (buffer.hpp:52,76); `typed_cpu_buffer_input<lit,T>` / `typed_cpu_buffer_output<lit,T>` (:106,118); `gpu_buffer_input/output<lit>` (:142,153).

Examples: Advanced/Geometry/NoiseBuffer (cpu_buffer_output), NoiseBuffer_raw_cpu (`buffer_output`), NoiseBuffer_typed_cpu (`typed_buffer_output`), Gpu/ArrayToBuffer (`cpu_buffer_output`), Gpu/BufferToArray (`cpu_buffer_input`).

---

## 7. Tensor I/O

Concepts (`avnd/concepts/tensor.hpp`): DLPack `dtype_code:62` (Int/UInt/Float/OpaqueHandle/Bfloat/Complex/Bool) + `dtype_descriptor:73`. Customization points `data_of:138`, `shape_of:145`, `strides_of:152`, `resize_to:159`, `set_view_buffer:225`. Tensor concepts: `tensor_like:186` (data ptr + sized shape), `has_tensor_strides:202`, `resizable_tensor_like:208`, `mutable_tensor_like:215`, `view_tensor_like:254`, `dynamic_tensor_like:271` (type-erased runtime dtype), `tensor_port:329` (`.value` is tensor_like). `dynamic_rank:283`, `container_static_rank:295`, `static_port_rank:312`, `has_static_rank:326`. Shim: `avnd/wrappers/tensor_shim.hpp`.

halp (`halp/tensor_port.hpp`):

- `tensor_view<T>` — read-only view {`data_ptr`, `shape_v`, `strides_v`, `keep_alive`} (tensor_port.hpp:62); `is_tensor_view_v` trait (:113).
- `tensor_kind` enum: `generic, vector, matrix, spectrum, spectrogram` (tensor_port.hpp:133); `kind_to_rank` (:142).
- `tensor_port<Name, Container, Kind=generic>` — `Container` any `tensor_like` (e.g. `xt::xarray<double>`, `Eigen::Tensor<float,3>`, `boost::multi_array<int,2>`, or `halp::tensor_view<double>`) (tensor_port.hpp:158). Static rank derived from Kind or container.

Marshalled to numpy in Python backend; Pd/Max/TD lower it to typed-message/jit_matrix/CHOP. View vs resizable vs mutable correspond to the three concepts above.

---

## 8. Geometry I/O

Concepts (`avnd/concepts/gfx.hpp`): `static_geometry_type:134` (compile-time `.buffers`,`.input`, attributes, bindings, vertices/indices), `dynamic_geometry_type:144` (runtime-sized via `.size()`), `geometry_port:154` (geometry at top level **or** in a `.mesh` member).

halp (`halp/geometry.hpp`):

- Enums: `binding_classification` (per_vertex/per_instance, :11), `attribute_semantic` (huge: position, normal, tangent, bitangent, texcoord0-7, color0-3, joints/weights, morph_*, transform/instancing, particle dynamics, PBR material, gaussian-splat, volumetric, topology, per-instance, fx0-7, custom — :16-166), `attribute_format` (float1-4, unormbyte*, uint*, sint*, half*, ushort*, sshort*, :167), `index_format` (uint16/uint32, :197), `primitive_topology` (triangles/strip/fan/lines/line_strip/points, :203), `cull_mode` (:212), `front_face` (:218).
- Building blocks: `geometry_cpu_buffer`/`geometry_gpu_buffer`/`geometry_binding`/`geometry_attribute`/`geometry_input`/`geometry_auxiliary_buffer`/`geometry_auxiliary_texture`/`mesh` (:224-291).
- Static prefab geometries: `position_geometry` (:295), `position_color_packed_geometry` (:360), `position_normals_geometry` (:444), `position_texcoords_geometry` (:535), `position_normals_texcoords_geometry_plane/volume` (:734,744), `position_normals_color_index_geometry` (:758), `position_normals_color_geometry` (:915).
- Runtime: `dynamic_geometry` (:1055), `dynamic_gpu_geometry` (+auxiliary_textures, :1079).

Examples: Tutorial/CubeGenerator (`position_normals_color_index_geometry`), Tutorial/DynamicGeometry, Gpu/GenerateGeometry (interleaved/non-interleaved/indexed), Gpu/FilterGeometry (`dynamic_geometry`), TouchDesigner GridSOP/CubeSOP/SphereSOP, GeoZones, Advanced/AudioParticles (POP-style).

---

## 9. Messages & callbacks

Input **messages** = named member/free functions invoked by name from message-oriented hosts. Concepts (`avnd/concepts/message.hpp`): `message:60` (reflectable + `name()`), `stateless_message:22`, `stateful_message:29`, `stdfunc_message:38`, `function_object_message:47`, `variadic_function_object_message:51`, `unreflectable_message:67`, `any_message_port:143`. Introspection: `avnd/introspection/messages.hpp`. A `messages` sub-struct (or type) holds entries each exposing `name()` + `func()` or `operator()`.

halp (`halp/messages.hpp`): `func_ref<lit, &Member>` (:29); macros `halp_start_messages(T)`/`halp_end_messages`, `halp_mem_fun(M)`, `halp_mem_fun_t(M,<T>)`, `halp_free_fun(F)`, `halp_free_fun_t`, `halp_lambda(Name, lambda)` (:49-74). Annotation form `[[=halp::message]]` / `halp::message_named("!")` (:39-43). Snippet: examples/Helpers/Messages.hpp:51 (member, free, templated, lambda, generic `func_ref`); examples/Raw/Messages.hpp (functor/member/lambda/free).

Output messages/callbacks — see §2 (`callback`, `timed_callback`, `std::function`, `basic_callback`). Examples: Raw/Callback, Complete/CompleteMessageExample (callback + val_port + messages), Advanced/UI/Widgets.

---

## 10. Dictionaries / structured data

The low-level layer accepts a huge range of value types directly as port `.value` — exhaustively listed in **examples/Raw/AllPortsTypes.hpp:51** (the canonical "every container" object):

- Scalars: float, double, int, std::string; `enum Foo` (:65).
- Aggregates / nested structs: `Aggregate{int; string; struct Sub{float; vector<float>;}}` (AllPortsTypes.hpp:26).
- `std::pair`, `std::tuple`, `std::variant`, `std::optional`, `std::bitset<N>` (:67-78).
- Sequence containers: `std::vector`, `list`, `deque`, `array<T,N>`, `boost::container::vector/small_vector/static_vector` (:81-128).
- Associative: `std::set`, `unordered_set`, `std::map<K,V>`, `unordered_map<K,V>` keyed by string or int (:130-170).
- Optional `symbol()` per-port + `class_attribute` tag variants (commented block :173-284).

Generic-container concepts backing this (`avnd/concepts/generic.hpp`): `vector_ish:69`, `set_ish:72`, `map_ish:82`, `dict_ish:200` (string-keyed map), `tuple_ish:108`, `variant_ish:115`, `pair_ish:121`, `optional_ish:127`, `array_ish:143`, `bitset_ish:149`, `string_ish:165`, plus `int_ish`/`fp_ish`/`bool_ish`/`enum_ish`. Field names for aggregates: `halp_field_names(a,b,c)` (controls.hpp:267) / concept `has_field_names` (`concepts/field_names.hpp:12`); introspection `avnd/introspection/field_names.hpp`. Example: Raw/Aggregate.hpp.

---

## 11. Metadata

Object & port metadata resolved via `define_get_property` in `avnd/wrappers/metadatas.hpp` (each as `static name()`/member or `[[=halp::meta(...)]]` annotation):

`name` (194), `c_name` (197), `symbol` (200), `uuid` (203), `vendor`/`product`/`version`/`label`/`short_label`/`category`/`copyright`/`license`/`url`/`email`/`manual_url`/`support_url`/`description`/`short_description`/`module` (206-220), `pixmaps` (222), `path`/`default_osc_path` (225-229). Author resolution accepts `author`/`authors`/`developer(s)`/`creator(s)` (metadatas.hpp:323-373). `unit`/`dataspace`/`type` (376-400). `version` int coercion (402). `keywords`/`tags` (441-509). `c_identifier` fixup for invalid names (273-310).

Declaration helpers (`halp/meta.hpp`): `halp_meta(name, value)` → consteval fn (:5); `halp_flag(flag)` / `halp_flags(...)` enum tags (:11-20); annotation `halp::meta("key","value")` (:38). Port metadata: `name()` on every halp port; `description()` on audio ports (halp/audio.hpp:24); `symbol()` and `class_attribute` enum tag for attribute ports (concepts/attributes.hpp:27, AllPortsTypes.hpp:174). Range/init/min/max as in §1. `field_names()` per §10.

Snippet: examples/Advanced/Audio/Echo.hpp:19-26 (name/c_name/author/category/description/manual_url/uuid).

---

## 12. Lifecycle / processing

`operator()` forms (auto-detected by processor concepts in `avnd/concepts/processor.hpp` and `function_reflection`):

- `void operator()()` — message/tick-driven object (examples/Helpers/Controls.hpp:90).
- `void operator()(int frames)` — buffer with frame count (examples/Raw/WasmCanvasUI.hpp).
- `FP operator()(FP in)` / `FP operator()(FP in, const inputs&, outputs&)` — per-sample mono (`mono_per_sample_arg_processor:131`; Helpers/PerSample.hpp:19,37; Advanced/Audio/Gain.hpp:27, Echo.hpp:58).
- `void operator()(const inputs&, outputs&)` — per-sample with ports (Helpers/PerSample.hpp:60).
- `void operator()(FP** in, FP** out, int frames)` — poly args (`polyphonic_arg_audio_effect:115`; Raw/Presets.hpp).
- `void operator()(int N)` — sample-accurate buffer length (Raw/SampleAccurateControls.hpp:109).

Other lifecycle hooks:

- `prepare()` / `prepare(setup)` (`can_prepare`, audio_processor.hpp:18; wrappers/prepare.hpp). `process_setup{input_channels, output_channels, frames_per_buffer, rate, instance, samples_to_model, model_to_samples}` (prepare.hpp:11).
- `initialize()` (`can_initialize`, concepts/object.hpp:12); tag `skip_init` (concepts/init.hpp:20).
- Tick types: `using tick = halp::tick;` `{int frames;}` (audio.hpp:274); `tick_musical` (tempo, signature, position_in_frames/quarters, quantification & metronome helpers, audio.hpp:279-885); `tick_flicks` (flicks timing, audio.hpp:887). `has_tick` (processor.hpp:22). Example: Raw/TimingSplitter.hpp (`tick_flicks`), Midi/Arpeggiator & Wavecycle (`tick_musical`).
- Scheduling: `schedule` member (concepts/schedule.hpp:18); `clocks`/`clock_spec` periodic polling (schedule.hpp:31-39); `halp::scheduler_fun<Args...>` (halp/schedule.hpp:11). Worker thread: `has_worker`/`port_can_process` (concepts/worker.hpp:9-12; Raw/Shell.hpp). Temporality tags: `single_exec`, `process_exec`, `temporal`, `loops_by_default`, `timestamp` (concepts/temporality.hpp:33-58).
- Bypass / programs: `can_bypass` (audio_processor.hpp:15), `has_programs` (:12); presets example Raw/Presets.hpp; vintage `programs.hpp`.
- Sample-accurate controls — three APIs (concepts/parameter.hpp:201-247): linear (`std::optional<float>* values`, `linear_sample_accurate_parameter_port:239`), span (`span<timestamped_value>`, `span_sample_accurate_parameter_port:243`), dynamic (`std::map<int,float>`, `dynamic_sample_accurate_parameter_port:247`). halp wrapper `accurate<T>` = control + `sample_accurate_values<T>` flat_map (halp/sample_accurate_controls.hpp:23). Example Raw/SampleAccurateControls.hpp demonstrates all three.
- FFT injection — `using fft_type = halp::fft<FP>` etc.; concepts `fft_1d`/`rfft_1d` (concepts/fft.hpp:20,42), spectrum ports (fft.hpp:64-89), `halp::fft<FP>` (halp/fft.hpp:52). Examples Helpers/PeakBandFFTPort, FFTDisplay, Advanced/Audio/Convolver.

GPU pipelines use the **separate `gpp::` helper library** (not under `halp/`): a `layout` struct with `halp_flags(graphics|compute)`, vertex/fragment/compute inputs/outputs, `bindings` (ubo/sampler/image/buffer), shader source via `vertex()/fragment()/compute()`, and coroutine driver fns `gpp::co_update update()`, `co_release release()`, `co_dispatch dispatch()` (examples/Gpu/DrawWithHelpers.hpp:18-147, Gpu/Compute.hpp:33-225). Concepts `gpu_compute_context` (concepts/gpu_compute.hpp:41), `device_memory_port:32`. Largely ossia/score-only.

Hardware ports (bare-metal target) — `gpio_port`, `pwm_port`, `adc_port`, `dac_port`, `i2c_port`, `spi_port`, `uart_port`, `rtc_port`, `watchdog_port` (concepts/hardware.hpp:20-58). No backend consumes these yet (unknown).

---

## 13. Examples inventory

Style key: **H** = halp, **R** = raw avnd, **G** = gpp/GPU. Grouped by directory.

### Tutorial/
| File | Name | Style | Features |
|---|---|---|---|
| EmptyExample.hpp | Hello world | H | minimal tick, logger |
| TrivialGeneratorExample.hpp | trivial generator | H | value output |
| TrivialFilterExample.hpp | trivial filter | H | value in/out |
| AudioEffectExample.hpp | example effect | H | audio bus in/out, knob |
| ZeroDependencyAudioEffect.hpp | zero-dep effect | R | per-sample, no includes |
| Distortion.hpp | distortion | H | fixed stereo, prepare() |
| AudioSidechainExample.hpp | sidechain | H | multiple buses, sidechain |
| Synth.hpp | synth | H | MIDI in, stereo out, unit conv |
| ControlGallery.hpp | control gallery | H | every basic widget |
| SampleAccurateGeneratorExample.hpp | sa generator | H | `accurate<>` out |
| SampleAccurateFilterExample.hpp | sa filter | H | timestamped in/out |
| CubeGenerator.hpp | Cube Generator | H | geometry (pos/normal/color/index) |
| DynamicGeometry.hpp | Dynamic geometry | H | `dynamic_geometry`, attributes |
| TextureGeneratorExample.hpp | texture generator | H | rgba_texture out |
| TextureFilterExample.hpp | texture filter | H | texture in+out |

### Raw/ (low-level avnd)
| File | Name | Features |
|---|---|---|
| Minimal.hpp | Minimal example | metadata only, no deps |
| AllPortsTypes.hpp | AllPortsTypes | **all value/container/aggregate types** |
| Aggregate.hpp | Aggregate Input | nested struct value port |
| Addition.hpp | Addition | value in/out |
| Construct.hpp / Init.hpp | Init | constructor / init + generic message |
| Lowpass.hpp | Lowpass | audio + filter state + prepare |
| Midi.hpp | MIDI example | raw midi bytes, 128-voice poly |
| Messages.hpp | Messages | functor/member/lambda/free messages |
| Callback.hpp | Callback | callback + std::function outputs |
| Optional.hpp | Optional Input | optional = impulse trigger |
| Modular.hpp | Modular example | per-sample modular L/R/sidechain |
| PerSampleProcessor.hpp / PerSampleProcessor2.hpp / MiniPerSample.hpp | per-sample | per-sample arg/port forms |
| SampleAccurateControls.hpp | Sample accurate example | 3 SA APIs (optional*/span/map) |
| SpanControls.hpp | Span example | span/multi-slider control |
| DoubleArray.hpp | Double Array | array control (knob) |
| Variant.hpp | Variant | variant value port |
| Presets.hpp | Presets example | programs/presets, poly args |
| Random.hpp | random | generator |
| Reactive.hpp | Addition | reactive value out |
| Interpolator.hpp | (interpolator) | widget + interpolation |
| Ui.hpp | UI example | inline UI widgets |
| Sines.hpp | Out (sines) | generator |
| Shell.hpp | Shell command | worker thread (single_exec) |
| ProcessLauncher.hpp | Process launcher | external process |
| TimingSplitter.hpp | Beat metronome | `tick_flicks`, enum |
| WasmWidgetTypes.hpp | WasmWidgetTypes | xy/xyz/color/range/multi/enum/combo |
| WasmCanvasUI.hpp | Wasm Canvas UI | canvas UI + audio_channel |

### Helpers/ (halp)
| File | Name | Features |
|---|---|---|
| Logger.hpp | Helpers | logger abstraction (post/printf) |
| Controls.hpp | Controls helpers | sliders/knob/toggle/lineedit/button/enum/combo + bargraph out |
| Messages.hpp | Message helpers | message macros |
| Midi.hpp | MIDI (helpers) | midi_bus pass-through |
| PerSample.hpp | Per-sample (helpers) | 3 per-sample forms |
| PerFrame.hpp | Per-frame | per_frame ports |
| PerBus.hpp | Per-bus | per_bus args/fixed/dynamic |
| CustomChannels.hpp | Multichannel | fixed + variable bus |
| AudioChannelSelector.hpp | channel selector | dynamic + variable bus |
| Lowpass.hpp / SmoothGain.hpp | Lowpass / Smooth Gain | dynamic_audio_bus, smoothing |
| Noise.hpp / Sines.hpp | White noise / Sines | generators |
| Peak.hpp / PeakBand.hpp | Peak value / Peak band | audio_channel → val_port |
| PeakBandFFTPort.hpp | Peak band (FFT port) | FFT/spectrum input |
| FFTDisplay.hpp | FFT display | `dynamic_audio_spectrum_bus` |
| Poles.hpp | Poles | val_port array |
| ValueDelay.hpp | Value delay | val_port + delay |
| FileChange.hpp | CSV reader | file_port + file_watch |
| SpriteReader.hpp | Sprite reader | file_port → texture_output |
| GradientScrubber.hpp | Gradient scrubber | gradient value |
| Attributes.hpp | Attributes | class_attribute parsing |
| Ui.hpp | Main/HBox/VBox/Grid/Group/Tabs/Widget | layout system |
| UiBus.hpp | (ui bus) | UI↔audio message bus |
| ImageUi.hpp | Main | image UI |
| Gpu.hpp | My GPU texture filter | gpp texture filter |

### Midi/
| File | Name | Features |
|---|---|---|
| MidiPitch.hpp | Midi Pitch | note→timed_callback |
| MidiGlide.hpp | Midi Glide | midi_bus glide |
| MidiSyncInput.hpp | MIDI Sync In | midi clock + combobox/spinbox |
| MidiHiResInput.hpp | Midi hi-res input | accurate + timed_callback |
| MidiHiResOutput.hpp | Midi hi-res output | accurate + timed_callback |
| Arpeggiator.hpp | Arpeggiator | midi_bus + tick_musical + time_chooser |

### Gpu/ (gpp + geometry/buffer)
| File | Name | Features |
|---|---|---|
| SolidColor.hpp | Solid color | fragment shader, UBO, color attachment |
| DrawWithHelpers.hpp | Helpers GPU pipeline | graphics layout, uniform_control_port, sampler |
| DrawRaw.hpp | (draw raw) | raw ubo/texture allocation coroutines |
| Compute.hpp | Average color | compute shader, SSBO, readback |
| GenerateGeometry.hpp | Generate Geometry | interleaved/non-interleaved/indexed |
| FilterGeometry.hpp | Filter Geometry | `dynamic_geometry` + attribute_semantic |
| ArrayToBuffer.hpp | Array to buffer | cpu_buffer_output + enum |
| BufferToArray.hpp | Buffer to array | cpu_buffer_input + combobox |

### Advanced/Audio/
Echo, Flanger, Bitcrush (Gamma compat, time_chooser, log_mapper), Dynamics (Compressor/Limiter, dynamic_audio_bus), AudioFilters (Bandpass/Bandshelf..., `tick`), Gain (smooth_knob), AudioSum (fixed+dynamic bus + soundfile), MonoMix ("Mono mix 8"), QuadPan, StereoMixer, Splitter (dynamic_port + audio_channel), Silence (variable_audio_bus + soundfile), Soundfile (soundfile_port + variable bus + tick_musical), Convolver (FFT/kfr + soundfile).

### Advanced/Utilities/ (mostly small value-math objects)
Math.hpp (≈50 unary/binary math ops: Sinus, Cosinus, Power, Log, Clamp, ...), Mapping/MapTool/RangeMapper/RangeFilter/Calibrator/Tweener (value mappers), LFO ("LFO X", waveform enum + S&H + optional out), ADSR (Gamma envelope + buttons), Counter/Accumulator/Trigger/Spigot/Stutter, Logic, Combine/Spread/MultiChoice, Array{Tool,Flattener,Recombiner,Combiner}, Automation/ColorAutomation, ControlToCV, ValueFilter/RepetitionFilter/ValueMixer, TimecodeSynchronizer, Smooth.

### Advanced/ (other)
Spatialization/DBAP (2D/3D, speaker array), MatrixMixer; Granular/Granolette (soundfile + variable bus); Graph/Graph (variant expression graph); Parametriq (ParametricEq + EQDiagram); Patternal/Melodial (patterns); Synth/Wavecycle (curve_port + tick_musical + audio_channel); Kabang/Minibang/Sampler (drum sampler, log_pot); MidiScaler (Scaler/Reader/Filter/Scroller — midifile); NetStream (Sender/Receiver TCP); AudioParticles (POP particles, tick_musical); GeoZones (geometry + layout); Image/LightnessComputer & LightnessSampler (texture in + custom layout); Display/ColorDisplay (custom item); AI/PromptComposer (dynamic_port + lineedit), PromptInterpolator.

### Advanced/TouchDesigner/
TOP: InvertTOP (texture in/out), BrightnessContrastTOP, CheckerboardTOP (texture out + rgb_color), BlendTOP. SOP: GridSOP/CubeSOP/SphereSOP (geometry). DAT: DataGeneratorDAT, TextProcessorDAT, MathProcessorDAT (val_port table data).

### Ports/ (third-party ports), GPT3/, Demos/, Complete/
BarrVerb (reverb, fixed_audio_bus), Essentia (analysis), LitterPower/CCC (accurate + impulse), Puara/Jab (knob+val), WaveDigitalFilters/PassiveLPF & VoltageDivider; GPT3/ResonantBandpass (process()); Demos/GainCHOP (CHOP, dynamic_audio_bus + tick), OSC.cpp/JSONToMaxref.cpp/GeneratePdHelp.cpp (codegen demos); Complete/CompleteMessageExample (callback + val_port + messages, with .pd/.maxhelp patches).

---

## 14. Backend support matrix

Rows = features, cols = backends. ✓ supported · ◐ partial · ✗ unsupported · ? unknown. Based on what each `include/avnd/binding/<backend>/` actually introspects/handles (subagent survey; spot-checks cited in §1-§12).

| Feature | max | pd | td | godot | vst3 | clap | python | wasm/standalone | score/ossia | gstreamer | vintage |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Control params (float/int/bool/string) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ◐ | ✓ | ✓ | ◐ | ✓ |
| Enum / combobox | ✓ | ◐ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ? | ✓ |
| Color param | ✓ | ✗ | ✓ | ✓ | ✗ | ✗ | ✗ | ✓ | ◐ | ✗ | ✗ |
| xy/xyz/xyzw param | ◐ | ✗ | ✓ | ✓ | ✗ | ✗ | ✗ | ✓ | ◐ | ✗ | ✗ |
| Value in/out ports | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ◐ | ✓ |
| Callbacks out | ✓ | ✓ | ✓ | ◐ | ◐ | ◐ | ✓ | ✓ | ✓ | ✗ | ◐ |
| Audio I/O (mono/poly) | ✓ | ✓ | ✓ (CHOP) | ✓ | ✓ | ✓ | ◐ | ✓ | ✓ | ✓ | ✓ |
| Sample-accurate controls | ✓ | ✓ | ◐ | ✗ | ✗ | ◐ | ✗ | ✓ | ✓ | ✗ | ✗ |
| MIDI I/O | ◐ (out) | ✗ | ◐ | ✗ | ✓ | ✓ | ✗ | ✓ | ✓ | ✗ | ✓ |
| Texture CPU | ✓ (jit) | ✗ | ✓ (TOP) | ✓ | ✗ | ✗ | ✗ | ◐ | ✓ | ✓ | ✗ |
| Texture GPU | ✓ | ✗ | ✓ | ✓ | ✗ | ✗ | ✗ | ◐ | ✓ | ✓ | ✗ |
| Buffer / SSBO | ◐ | ✗ | ✓ | ? | ✗ | ✗ | ✗ | ? | ✓ | ? | ✗ |
| Tensor ports | ✓ | ✓ | ✓ | ✗ | ✗ | ✗ | ✓ (numpy) | ? | ✓ | ✗ | ✗ |
| Geometry / mesh / SOP / POP | ✓ (ob3d) | ✗ | ✓ (SOP/POP) | ✓ | ✗ | ✗ | ✗ | ◐ | ✓ | ✗ | ✗ |
| Messages in | ✓ | ✓ | ✓ | ◐ | ◐ | ◐ | ✓ | ✓ | ✓ | ◐ | ◐ |
| Soundfile port | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ | ✗ | ✗ |
| MIDI-file port | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ | ✗ | ✗ |
| File / folder port | ◐? | ✗ | ◐? | ? | ✗ | ✗ | ✗ | ? | ✓ | ✗ | ✗ |
| Dynamic ports | ◐ | ◐ | ✓ | ◐ | ◐ | ◐ | ✗ | ✓ | ✓ | ◐ | ◐ |
| Metadata (uuid/author/category) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ◐ | ✓ | ✓ | ◐ | ✓ |
| GPU compute pipelines (gpp) | ✗ | ✗ | ? | ? | ✗ | ✗ | ✗ | ✗ | ✓ | ? | ✗ |

Extra backends not in the requested columns but present: **liblo** (OSC: messages + numeric params + OSC-MIDI; no audio/texture/geometry), **vintage** (VST2: audio+MIDI+params+programs), **ui/nuklear** + **ui/qml** (control/layout rendering only), **dump**/**example**/**fuzz** (testing/codegen harnesses). Most capable backends are **ossia/score** (nearly everything incl. soundfile/midifile/geometry/GPU) and **wasm** (params/messages/audio/MIDI/sample-accurate). Evidence highlights: max jitter texture/geometry (`binding/max/inputs.hpp`, `jitter_*`), td per-kind processors (`binding/touchdesigner/{top,sop,pop,dat,chop}/`), godot variant mapping (`binding/godot/node.hpp:77-102`), clap MIDI events (`binding/clap/audio_effect.hpp:24-119`), python numpy/pybind (`binding/python/processor.hpp:21`), wasm to_js conversions (`binding/wasm/controls.hpp:47-149`), ossia geometry/soundfile (`binding/ossia/geometry.hpp`, `soundfiles.hpp`), gstreamer audio/video (`binding/gstreamer/{audio,texture}_*.hpp`).

Unsure marks (?): GPU/buffer support in godot/wasm/gstreamer, file ports in max/td, gpp in td — these need deeper per-backend confirmation.

---

## 15. Proposed test-object matrix

Minimal deduplicated set of test objects to cover the whole surface. Each maps to the features it exercises (build both an `avnd`-raw and a `halp` variant where the two styles differ meaningfully).

**Controls (§1):**
- `TestAllScalarControls` — hslider/vslider/knob/iknob/spinbox/bargraph in f32 + i32, toggle, lineedit, maintained_button, impulse_button. Covers every scalar widget_type.
- `TestVectorControls` — xy_pad, xy/xyz/xyzw_spinboxes, range_slider, range_spinbox, color_chooser, multi_slider, time_chooser (sync on/off).
- `TestEnumControls` — `enum_t`, `string_enum_t`, `combobox_t`, `halp__enum`, `halp__enum_combobox`, `combo_pair` array. Covers enum/combobox/choices.
- `TestRangesAndDefaults` — explicit min/max/init, default_range, positive/free ranges, irange, init_range.
- `TestMappersAndSmoothing` — log_mapper / pow_mapper / inverse_mapper; milliseconds_smooth / exp_smooth; smooth_knob/slider.
- `TestSpecialControls` — file_port (text/binary/mmap) + file_watch, folder_port, gradient_port, curve_port (linear/power/custom), addr_port.

**Values & callbacks (§2,§9):**
- `TestValuePorts` — val_port in/out for float/int/bool/string/enum; bargraph display outputs.
- `TestCallbacks` — `callback`, `timed_callback`, `std::function` out, `basic_callback`, `reactive_value`.
- `TestMessagesIn` — member/free/lambda/functor/templated/`func_ref`; `[[=halp::message]]` & `message_named`; variadic message.

**Audio (§3):**
- `TestAudioMono` — audio_sample (per-sample) + audio_channel (per-buffer); float and double.
- `TestAudioPolyFixed` — fixed_audio_bus + fixed_audio_frame, `channels()`.
- `TestAudioPolyDynamic` — dynamic_audio_bus / dynamic_audio_frame.
- `TestAudioPolyVariable` — variable_audio_bus + `request_channels`; mimic_audio_bus.
- `TestAudioArgForms` — `FP op()(FP)`, `op(FP*,FP*,int)`, `op(FP**,FP**,int)`, port forms; prepare(setup).
- `TestAudioSpectrum` — audio_spectrum_channel + fixed/dynamic_audio_spectrum_bus; FFT injection (`fft<FP>`, spectrum split/complex ports).

**MIDI (§4):**
- `TestMidiIO` — midi_bus in/out, midi_note_bus, midi_out_bus builders (note_on/off, cc, pitch_bend, sysex), raw 3-byte messages, timestamps.

**Texture (§5):**
- `TestTextureFormats_CPU` — one in+out per fixed format: R8, R32F, RGB, RGBA, RGBA32F.
- `TestTextureCustom` — custom_texture (request_format) + custom_variable_texture across all `texture_format` enumerators (RGBA8/BGRA8/R8/RG8/R16/RG16/RED_OR_ALPHA8/RGBA16F/RGBA32F/R16F/R32F/R8UI/R32UI/RG32UI/RGBA32UI).
- `TestTextureGPU` — gpu_texture_input/output across `format_t` (incl. SI/UI/RGB10A2); gpu_render_target_output; sampleable_depth.
- `TestTextureKinds` — gpu_texture_2d / array / cubemap / 3d (kind + layers_or_depth).

**Buffer / Tensor / Geometry (§6,§7,§8):**
- `TestBuffersCPU` — cpu_buffer_input/output (raw) + typed_cpu_buffer_input/output<T>; formatted buffer.
- `TestBuffersGPU` — gpu_buffer_input/output.
- `TestTensorIO` — tensor_port with view / resizable / mutable containers; kinds generic/vector/matrix/spectrum/spectrogram; multiple dtypes (int/uint/float/complex/bool).
- `TestGeometryStatic` — each prefab (position, packed color, normals, texcoords, normals+texcoords, normals+color[+index]).
- `TestGeometryDynamic` — dynamic_geometry + dynamic_gpu_geometry exercising attribute_semantic / attribute_format / topology / cull / index_format / auxiliary buffers & textures.

**Structured data (§10):**
- `TestAllPortsTypes` — reuse/extend examples/Raw/AllPortsTypes.hpp: scalars, enum, aggregate(+nested), pair/tuple/variant/optional/bitset, vector/list/deque/array/boost containers, set/unordered_set, map/unordered_map (string- and int-keyed), with `halp_field_names`. Also a `symbol()`/`class_attribute` variant.

**Metadata & lifecycle (§11,§12):**
- `TestMetadataFull` — every metadata key (name/c_name/symbol/uuid/vendor/product/version/label/category/copyright/license/url/email/manual_url/support_url/description/module/path), author/authors/developer/creator forms, unit/dataspace, keywords/tags, pixmaps, `[[=halp::meta]]` annotations.
- `TestLifecycle` — prepare(setup) (all setup fields), initialize()/skip_init, can_bypass, programs/presets, worker thread (single_exec), reactive update.
- `TestTicks` — `tick`, `tick_musical` (tempo/signature/quantization/metronome), `tick_flicks`.
- `TestSampleAccurate` — all three SA control APIs (optional*/span/map) on both inputs and outputs; halp `accurate<>`.
- `TestScheduling` — schedule member, clocks/clock_spec, scheduler_fun, temporality tags (single_exec/process_exec/temporal/loops_by_default).
- `TestDynamicPorts` — dynamic_port<Port> + request_port_resize on controls and audio.

**Backend-specific surfaces:**
- `TestSoundfilePort` / `TestMidifilePort` — soundfile_port (float/double) + midifile_port (simple/full events) — primarily exercises ossia.
- `TestGpuPipeline` — gpp graphics layout (vertex/fragment, ubo, sampler, color attachment) + gpp compute layout (SSBO, image2D, readback, dispatch) — ossia/score.
- `TestLayoutUI` — layouts (hbox/vbox/grid/split/tabs/group/spacing), custom_item/control, painter canvas — ui/wasm/standalone.
- `TestHardwarePorts` — gpio/pwm/adc/dac/i2c/spi/uart/rtc/watchdog (forward-looking; currently no consumer).

Cross-cut: for each test object, provide an **input** and an **output** variant where the feature can be both, and a **raw-avnd** twin of the **halp** version to verify both authoring styles compile against every backend's introspection.
