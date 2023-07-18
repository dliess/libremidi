#pragma once
#include <libremidi/detail/midi_api.hpp>

namespace libremidi
{

class midi_in_api : public midi_api
{
public:
  explicit midi_in_api(void* data)
  {
    cancel_callback();
    inputData_.apiData = data;
  }

  ~midi_in_api() override = default;

  midi_in_api(const midi_in_api&) = delete;
  midi_in_api(midi_in_api&&) = delete;
  midi_in_api& operator=(const midi_in_api&) = delete;
  midi_in_api& operator=(midi_in_api&&) = delete;

  virtual void ignore_types(bool midiSysex, bool midiTime, bool midiSense)
  {
    inputData_.ignoreFlags = 0;
    if (midiSysex)
    {
      inputData_.ignoreFlags = 0x01;
    }
    if (midiTime)
    {
      inputData_.ignoreFlags |= 0x02;
    }
    if (midiSense)
    {
      inputData_.ignoreFlags |= 0x04;
    }
  }

  // FIXME not thread safe
  void set_callback(midi_in::message_callback callback)
  {
    if (!callback)
      cancel_callback();
    else
      inputData_.userCallback = std::move(callback);
  }

  void cancel_callback()
  {
    inputData_.userCallback = [](libremidi::message&& m) {};
  }

  // The in_data structure is used to pass private class data to
  // the MIDI input handling function or thread.
  struct in_data
  {
    libremidi::message message{};
    unsigned char ignoreFlags{7};
    bool firstMessage{true};
    void* apiData{};
    midi_in::message_callback userCallback{};
    bool continueSysex{false};

    void on_message_received(libremidi::message&& message)
    {
      userCallback(std::move(message));
      message.bytes.clear();
    }
  };

protected:
  in_data inputData_{};
};

template <typename T>
class midi_in_default : public midi_in_api
{
  using midi_in_api::midi_in_api;
  void open_virtual_port(std::string_view) override
  {
    using namespace std::literals;
    warning(T::backend + " in: open_virtual_port unsupported"s);
  }
  void set_client_name(std::string_view) override
  {
    using namespace std::literals;
    warning(T::backend + " in: set_client_name unsupported"s);
  }
  void set_port_name(std::string_view) override
  {
    using namespace std::literals;
    warning(T::backend + " in: set_port_name unsupported"s);
  }
};

}