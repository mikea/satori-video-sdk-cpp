#pragma once
#include <stdint.h>

// Video bot API.
extern "C" {
using bot_callback_t = void (*)(const uint8_t *image, int width, int height);

struct bot_descriptor {
  int image_width;
  int image_height;
  bot_callback_t callback;
};

void rtm_video_bot_register(const bot_descriptor &bot);
int rtm_video_bot_main(int argc, char *argv[]);
}