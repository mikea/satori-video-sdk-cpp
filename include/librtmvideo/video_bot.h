#pragma once
#include <stdint.h>
#include <cbor.h>

#include "rtmvideo.h"

#if defined(BOT_DEBUG)
# define BOT_DEBUG 1
#else
# define BOT_DEBUG 0
#endif

// Video bot API.
extern "C" {
struct bot_context;

using bot_callback_t = void (*)(bot_context &context,
                                const uint8_t *image, uint16_t width,
                                uint16_t height, uint16_t linesize);

struct bot_descriptor {
  uint16_t image_width;
  uint16_t image_height;
  image_pixel_format pixel_format;
  bot_callback_t callback;
};

enum class bot_message_kind { ANALYSIS = 1, DEBUG = 2 };

void rtm_video_bot_message(bot_context &context, const bot_message_kind kind,
                           cbor_item_t *message);
void rtm_video_bot_register(const bot_descriptor &bot);
int rtm_video_bot_main(int argc, char *argv[]);
}
