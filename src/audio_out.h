#pragma once
#include <stdint.h>
#include <stddef.h>

enum class AudioOutState : uint8_t {
  Stopped = 0,
  Playing,
  Paused
};

struct AudioFormat {
  uint32_t sampleRate;
  uint16_t bitsPerSample;
  uint16_t channels;
};

class IAudioOut {
public:
  virtual bool begin() = 0;
  virtual bool setFormat(const AudioFormat& fmt) = 0;

  virtual bool play() = 0;
  virtual bool pause() = 0;
  virtual bool stop() = 0;

  virtual AudioOutState state() const = 0;

  virtual ~IAudioOut() {}
};
