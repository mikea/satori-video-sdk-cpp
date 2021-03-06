// Error code for video sdk
#pragma once

#include <system_error>

namespace satori {
namespace video {

// Video processing error codes.
// Error codes should only be as granular as their processing requires.
// All specifics should be logged at the location the error has happened.
enum class video_error : uint8_t {
  // 0 = success
  STREAM_INITIALIZATION_ERROR = 1,
  FRAME_GENERATION_ERROR = 2,
  ASIO_ERROR = 3,
  END_OF_STREAM_ERROR = 4,
  FRAME_NOT_READY_ERROR = 5,
};

std::error_condition make_error_condition(video_error e);

}  // namespace video
}  // namespace satori

namespace std {
template <>
struct is_error_condition_enum<satori::video::video_error> : std::true_type {};
}  // namespace std
