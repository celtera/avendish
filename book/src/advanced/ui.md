# Creating user interfaces

We have seen so far that we can specify widgets for our controls. Multiple back-ends may render these widgets in various ways.
This is already a good start for making user interfaces, but most media systems generally have more specific user interface needs.

Avendish allows three levels of UI definition: 

1. Automatic: nothing to do, all the widgets corresponding to inputs and outputs of the processor will be generated automatically in a list. This is not pretty but sufficient for many simple cases. For instance, here is how some Avendish plug-ins render in *ossia score*.

> Supported bindings: all, not really a feature of Avendish per-se but of the hosts

![Basic UI](images/ui-basic.png)


2. Giving layout hints. A declarative syntax allows to layout said items and text in usual containers, auomatically and with arbitrary nesting: hbox, vbox, tabs, split view... Here is, again, an example in *ossia score*.

> Supported bindings: ossia

![Basic UI](images/ui-layout.png)

3. Creating entirely custom items with a Canvas-based API. It is also possible to load images, make custom animations and handle mouse events.

> Supported bindings: ossia

![Basic UI](images/ui-image.gif)