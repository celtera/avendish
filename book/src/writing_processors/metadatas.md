# Metadatas

> Supported bindings: all

So far the main metadata we saw for our processor is its name: 

```cpp
static consteval auto name() { return "Foo"; }
```

Or with the helper macro:
```cpp
halp_meta(name, "Foo")
```

There are a few more useful metadatas that can be used and which will be used depending on whether the bindings support exposing them.
Here is a list in order of importance; it is recommended that strings are used for all of these and that they are filled as much as possible.

- `name`: the pretty name of the object.
- `c_name`: a C-identifier-compatible name for the object. This is necessary for instance for systems such as Python, PureData or Max which do not support spaces or special characters in names.
- `uuid`: a string such as `8a4be4ec-c332-453a-b029-305444ee97a0`, generated for instance with the `uuidgen` command on Linux, Mac and Windows, or with [uuidgenerator.net](https://www.uuidgenerator.net/) otherwise. This provides a computer-identifiable unique identifier for your plug-in, to ensure that hosts don't have collisions between plug-ins of the same name and different vendors when reloading them (sadly, on some older APIs this is unavoidable).
- `description`: a textual description of the processor.
- `vendor`: who distributes the plug-in.
- `product`: product name if the plug-in is part of a larger product.
- `version`: a version string, ideally convertible to an integer as some older APIs expect integer versions.
- `category`: a category for the plug-in. "Audio", "Synth", "Distortion", "Chorus"... there's no standard, but one can check for instance the names used [in LV2](https://lv2plug.in/ns/lv2core) or the list mentioned by [DISTRHO](https://distrho.github.io/DPF/group__PluginMacros.html).
- `copyright`: `(c) the plug-in authors 2022-xxxx`
- `license`: an [SPDX identifier](https://spdx.org/licenses/) for the license or a link towards a license document
- `url`: URL for the plug-in if any.
- `email`: A contact e-mail if any.
- `manual_url`: an url for a user manual if any.
- `support_url`: an url for user support, a forum, chat etc. if any.
