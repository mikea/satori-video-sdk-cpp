#include <satorivideo/video_bot.h>
#include <iostream>
#include <thread>

namespace sv = satori::video;

namespace test_bot {

void process_image(sv::bot_context &context, const sv::image_frame & /*frame*/) {
  std::cout << "got frame " << context.frame_metadata->width << "x"
            << context.frame_metadata->height << "\n";
}

nlohmann::json process_command(sv::bot_context & /*context*/,
                               const nlohmann::json &config) {
  if (config["action"] == "configure") {
    auto &body = config["body"];
    if (body.empty()) {
      std::cout << "got no config\n";
    } else {
      std::cout << "processing config " << body << "\n";
    }
  }

  return nullptr;
}

}  // namespace test_bot

int main(int argc, char *argv[]) {
  sv::bot_register(sv::bot_descriptor{
      sv::image_pixel_format::BGR, &test_bot::process_image, &test_bot::process_command});
  return sv::bot_main(argc, argv);
}
