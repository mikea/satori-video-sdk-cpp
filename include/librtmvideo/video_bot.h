// Video bot API.
// There are two steps to define a video bot: register a bot and launch main
// event loop.
//
// Example:
// int main(int argc, char *argv[]) {
//   rtm_video_bot_register(bot_descriptor{640, 480, image_pixel_format::BGR,
//                                         &transcoder::process_image,
//                                         nullptr});
//   return rtm_video_bot_main(argc, argv);
// }

#pragma once

#include <cbor.h>
#include <stdint.h>

#include "base.h"
#include "rtmvideo.h"

// Every image belongs to a certain time interval, setting values wider
// makes annotation applicable to multiple video frames
EXPORT struct frame_id {
  int64_t i1;
  int64_t i2;
};

// API for image handler callback
// If an image uses packed pixel format like packed RGB or packed YUV,
// then it has only a single plane, e.g. all it's data is within plane_data[0].
// If an image uses planar pixel format like planar YUV or HSV,
// then every component is stored as a separate array (e.g. separate plane),
// for example, for YUV  Y is plane_data[0], U is plane_data[1] and V is
// plane_data[2]. A stride is a plane size with alignment.
EXPORT struct image_frame {
  frame_id id;
  const uint8_t *plane_data[MAX_IMAGE_PLANES];
};

// Metadata contains information which is unchangeable for a channel
EXPORT struct image_metadata {
  uint16_t width;
  uint16_t height;
  uint32_t plane_strides[MAX_IMAGE_PLANES];
};

// Used to store user defined state.
EXPORT struct bot_context {
  void *instance_data{nullptr};
  const image_metadata *metadata;
};

using bot_img_callback_t = void (*)(bot_context &context, const image_frame &frame);

// API for control command callback
// Format of message is defined by user.
// Recommended format is: {"action": "configure", "body":{<configure_parameters
// if specified>}}
using bot_ctrl_callback_t = cbor_item_t *(*)(bot_context &context, cbor_item_t *message);

struct bot_descriptor {
  // If received image's dimensions are greater than specified values,
  // then it will be automatically downscaled to provided values
  uint16_t image_width;
  uint16_t image_height;

  // Pixel format, like RGB0, BGR, etc.
  image_pixel_format pixel_format;

  // Invoked on every received image
  bot_img_callback_t img_callback;

  // Invoked on every received control command, guaranteed to be invoked during
  // initialization
  bot_ctrl_callback_t ctrl_callback;
};

// Used by bot implementation to specify type of output.
EXPORT enum class bot_message_kind { ANALYSIS = 1, DEBUG = 2, CONTROL = 3 };

// Sends bot implementation output to RTM subchannel.
EXPORT void rtm_video_bot_message(bot_context &context, const bot_message_kind kind,
                                  cbor_item_t *message,
                                  const frame_id &id = frame_id{0, 0});

// Registers a bot.
// Should be called by bot implementation before starting a bot.
EXPORT void rtm_video_bot_register(const bot_descriptor &bot);

// Starts a bot (e.g. launches main event loop).
// A bot implementation should be registered before calling this method.
EXPORT int rtm_video_bot_main(int argc, char *argv[]);
