#pragma once
#include <libremidi/backends/alsa_raw/config.hpp>
#include <libremidi/backends/alsa_raw/helpers.hpp>
#include <libremidi/detail/midi_out.hpp>

#include <alsa/asoundlib.h>

#include <atomic>
#include <thread>

namespace libremidi::alsa_raw_ump
{
class midi_out_impl final
    : public midi2::out_api
    , public error_handler
{
public:
  struct
      : output_configuration
      , alsa_raw_output_configuration
  {
  } configuration;

  midi_out_impl(output_configuration&& conf, alsa_raw_output_configuration&& apiconf)
      : configuration{std::move(conf), std::move(apiconf)}
  {
  }

  ~midi_out_impl() override
  {
    // Close a connection if it exists.
    midi_out_impl::close_port();
  }

  libremidi::API get_current_api() const noexcept override { return libremidi::API::ALSA_RAW; }

  bool open_virtual_port(std::string_view) override
  {
    warning(configuration, "midi_out_alsa_raw: open_virtual_port unsupported");
    return false;
  }
  void set_client_name(std::string_view) override
  {
    warning(configuration, "midi_out_alsa_raw: set_client_name unsupported");
  }
  void set_port_name(std::string_view) override
  {
    warning(configuration, "midi_out_alsa_raw: set_port_name unsupported");
  }

  int connect_port(const char* portname)
  {
    constexpr int mode = SND_UMP_SYNC;
    int status = snd_ump_open(NULL, &midiport_, portname, mode);
    if (status < 0)
    {
      error<driver_error>(
          this->configuration, "midi_out_alsa_raw::open_port: cannot open device.");
      return status;
    }
    return status;
  }

  bool open_port(const output_port& p, std::string_view) override
  {
    return connect_port(raw_from_port_handle(p.port).to_string().c_str()) == 0;
  }

  void close_port() override
  {
    if (midiport_)
      snd_ump_close(midiport_);
    midiport_ = nullptr;
  }

  void send_ump(const uint32_t* message, size_t size) override
  {
    if (!midiport_)
      error<invalid_use_error>(
          this->configuration,
          "midi_out_alsa_raw::send_message: trying to send a message without an open "
          "port.");

    if (!this->configuration.chunking || size <= 16)
    {
      write(message, size);
    }
    else
    {
      write_chunked(message, size);
    }
  }

  bool write(const uint32_t* message, size_t size)
  {
    if (snd_ump_write(midiport_, message, size) < 0)
    {
      error<driver_error>(
          this->configuration, "midi_out_alsa_raw::send_message: cannot write message.");
      return false;
    }

    return true;
  }

  std::size_t get_chunk_size() const noexcept
  {
    snd_rawmidi_params_t* param;
    snd_rawmidi_params_alloca(&param);

    auto rawmidi = snd_ump_rawmidi(midiport_);
    snd_rawmidi_params_current(rawmidi, param);

    std::size_t buffer_size = snd_rawmidi_params_get_buffer_size(param);
    return std::min(buffer_size, (std::size_t)configuration.chunking->size) / 4;
  }

  std::size_t get_available_bytes_to_write() const noexcept
  {
    snd_rawmidi_status_t* st{};
    snd_rawmidi_status_alloca(&st);

    auto rawmidi = snd_ump_rawmidi(midiport_);
    snd_rawmidi_status(rawmidi, st);

    return snd_rawmidi_status_get_avail(st);
  }

  // inspired from ALSA amidi.c source code
  void write_chunked(const uint32_t* const begin, size_t size)
  {
    const auto* data = begin;
    const auto* end = begin + size;

    const std::size_t chunk_size_in_words = std::min(get_chunk_size(), size);

    // Send the first buffer
    int len = chunk_size_in_words;

    if (!write(data, len))
      return;

    data += len;

    while (data < end)
    {
      // Wait for the buffer to have some space available
      const std::size_t written_bytes = (data - begin) * sizeof(uint32_t);
      std::size_t available{};
      while ((available = get_available_bytes_to_write() / sizeof(uint32_t)) < chunk_size_in_words)
      {
        if (!configuration.chunking->wait(
                std::chrono::microseconds((chunk_size_in_words - available) * 320), written_bytes))
          return;
      };

      if (!configuration.chunking->wait(configuration.chunking->interval, written_bytes))
        return;

      // Write more data
      int len = end - data;

      // Maybe until the end of the sysex
      // FIXME implement this for UMP (if it makes sense?)
      // if (auto sysex_end = (unsigned char*)memchr(data, 0xf7, len))
      //   len = sysex_end - data + 1;

      if (len > chunk_size_in_words)
        len = chunk_size_in_words;

      if (!write(data, len))
        return;

      data += len;
    }
  }

  alsa_raw_helpers::enumerator get_device_enumerator() const noexcept
  {
    alsa_raw_helpers::enumerator device_list;
    device_list.error_callback
        = [this](std::string_view text) { this->error<driver_error>(this->configuration, text); };
    return device_list;
  }

  snd_ump_t* midiport_{};
};
}
