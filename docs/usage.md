# Real-time input/output usage

The required header is `#include <libremidi/libremidi.hpp>`.

## Enumerating input ports

```C++
libremidi::midi_in midi;
for(int i = 0, N = midi.get_port_count(); i < N; i++) {
  // Information on port number i
  std::string name = midi.get_port_name(i);
}
```

## Enumerating output ports

```C++
libremidi::midi_out midi;
for(int i = 0, N = midi.get_port_count(); i < N; i++) {
  // Information on port number i
  std::string name = midi.get_port_name(i);
}
```

## Reading messages from a device through callbacks

```C++
// Create the midi object
libremidi::midi_in midi;

// Open a given midi port. Passing no arguments will open a default port.
midi.open_port(0);

// Setup a callback for incoming messages.
// This should be done immediately after opening the port
// to avoid having incoming messages written to the
// queue instead of sent to the callback function.

// Note that the callback will be invoked from a separate thread,
// it is up to you to protect your data structures afterwards.
// For instance if you are using a GUI toolkit, don't do GUI actions
// in that callback !
midi.set_callback([](const libremidi::message& message) {
  // how many bytes
  message.size();
  // access to the individual bytes
  message[i];
  // access to the timestamp
  message.timestamp;
});
```

## Sending messages to a device

```C++
// Create the midi object
libremidi::midi_out midi;

// Open a given midi port. Passing no arguments will open a default port.
midi.open_port(0);

// Option A: send raw bytes
unsigned char bytes[3] = { 144, 110, 40 };
midi.send_message(bytes, sizeof(bytes));

// Option B: std::vector<unsigned char>
midi.send_message(std::vector<unsigned char>{ 144, 110, 40 });

// Option C: std::span<unsigned char>
midi.send_message(std::span<unsigned char>{ ... your span-compatible data-structure ... });

// Option D: helpers with the message class
// See libremidi/message.hpp for the full list
midi.send_message(libremidi::message::note_on(channel, note, velocity));
midi.send_message(libremidi::message::control_change(channel, control, value));
midi.send_message(libremidi::message::pitch_bend(channel, value));
```

## Device connection / disconnection notification

Note: this has not been implemented for every API so far.

```C++
// The callbacks will be called when the relevant event happens.

libremidi::observer::callbacks cb;
cb.input_added = [] (int index, std::string name) { ... };
cb.input_removed = [] (int index, std::string name) { ... };
cb.output_added = [] (int index, std::string name) { ... };
cb.output_removed = [] (int index, std::string name) { ... };

libremidi::observer observer{
    libremidi::API::WINDOWS_UWP,
    std::move(cb)
};

```
## Error handling

The default error handling is done with exceptions.
If exceptions are undesirable, it is also possible to set a callback function which will be invoked upon error, for the `midi_in` and `midi_out` classes.

(Some classes may still throw, such as when creating invalid MIDI messages with the `libremidi::message` helpers, or the `observer` classes).

```C++
// Create the midi object
libremidi::midi_out midi;

midi.set_error_callback(
  [] (libremidi::midi_error code, std::string_view info) {
  // ... log error however you want
});
```