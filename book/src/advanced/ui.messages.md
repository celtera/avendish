# UI message bus

Some UIs may have more complicated logic that cannot just be represented through 
widgets changing single controls: pressing a button may trigger the loading of a file, 
the entire reconfiguration of the UI, etc.

Thus, the engine may have to be notified of such changes happening in the UI. The way this is 
generally done is through thread-safe queues exchanging messages.

An experimental implementation of this has been done in *ossia score*. 
Here is a skeleton for how to write such a plug-in: 

```cpp
using message_to_ui = ...;
using message_to_engine = ...;

struct MyProcessor {
  // Receive a message on the processing thread from the UI
  void process_message(message_to_engine msg)
  {
    // 3. The engine received the message from the UI.
    //    It can for instance send a confirmation that the message has been received:
    send_message(message_to_ui{ ... });
  }

  // Send a message from the processing thread to the ui
  std::function<void(message_to_ui)> send_message;

  struct ui {
    // Define the UI here as seen previously.

    struct bus {
      // 1. Set up connections from UI widgets to the message bus
      void init(ui& ui)
      {
        ui.some.widget.on_some_event = [&] {
          // 2. Some action occured on the UI: this callback is called.
          //    We send a message to the engine: 
          this->send_message(message_to_engine{...});
        };
      }

      // Receive a message on the UI thread from the processing thread
      static void process_message(ui& self, processor_to_ui msg)
      {
        // 4. The UI has received the confirmation from the engine, 
        //    we made a safe round-trip between our threads :-)
      }

      // Send a message from the ui to the processing thread
      std::function<void(ui_to_processor)> send_message;
    }
  };
}
```

Note that `message_to_ui` and `message_to_engine` can be any simple type: 

- Ints, floats, strings, etc.
- `std::vector`
- `std::variant`
- Any combination and nesting of those in an aggregate struct. 

For example, the following type will automatically be serialized & deserialized:

```cpp
struct ui_to_processor {
  int foo;
  std::string bar;
  std::variant<float, double> x;
  std::vector<float> y;
  struct {
    int x, y;
  } baz;
};
```

So far this has only been tested on a single computer but this could be tried over a network too.