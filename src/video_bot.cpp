#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "bot_environment.h"
#include "librtmvideo/decoder.h"
#include "librtmvideo/rtmvideo.h"
#include "librtmvideo/video_bot.h"
#include "rtmclient.h"

namespace asio = boost::asio;

struct bot_context {
  rtm::video::bot_environment& env;
  std::list<rtm::video::bot_message> message_buffer;
};

using bot_instance = bot_context;

namespace rtm {
namespace video {

static std::string to_string(const rapidjson::Value& d) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  d.Accept(writer);
  return std::string(buffer.GetString());
}

std::string decode64(const std::string& val) {
  using namespace boost::archive::iterators;
  using It =
      transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;
  return boost::algorithm::trim_right_copy_if(
      std::string(It(std::begin(val)), It(std::end(val))),
      [](char c) { return c == '\0'; });
}

bot_environment& bot_environment::instance() {
  static bot_environment env;
  return env;
}

void bot_environment::register_bot(const bot_descriptor* bot) {
  assert(!_bot_descriptor);
  _bot_descriptor = bot;
}

int bot_environment::main(int argc, char* argv[]) {
  namespace po = boost::program_options;
  po::options_description desc("Allowed options");
  desc.add_options()("help", "produce help message")(
      "endpoint", po::value<std::string>(), "app endpoint")(
      "appkey", po::value<std::string>(), "app key")(
      "channel", po::value<std::string>(), "channel")(
      "port", po::value<std::string>(), "port");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  if (!vm.count("endpoint")) {
    std::cerr << "Missing --endpoint argument"
              << "\n";
    return 1;
  }

  if (!vm.count("appkey")) {
    std::cerr << "Missing --appkey argument"
              << "\n";
    return 1;
  }

  if (!vm.count("channel")) {
    std::cerr << "Missing --channel argument"
              << "\n";
    return 1;
  }

  if (!vm.count("port")) {
    std::cerr << "Missing --port argument"
              << "\n";
    return 1;
  }

  const std::string endpoint = vm["endpoint"].as<std::string>();
  const std::string appkey = vm["appkey"].as<std::string>();
  const std::string channel = vm["channel"].as<std::string>();
  const std::string port = vm["port"].as<std::string>();

  decoder_init_library();

  asio::io_service io_service;
  asio::ssl::context ssl_context{asio::ssl::context::sslv23};

  _client = std::move(rtm::new_client(endpoint, port, appkey, io_service,
                                      ssl_context, 1, *this));
  _client->subscribe_channel(channel, _frames_subscription, *this);

  subscription_options metadata_options;
  metadata_options.history.count = 1;
  _client->subscribe_channel(channel + metadata_channel_suffix,
                             _metadata_subscription, *this, &metadata_options);

  _channel = channel;
  _bot_context = new bot_context{*this};

  boost::asio::signal_set signals(io_service);
  signals.add(SIGINT);
  signals.add(SIGTERM);
  signals.add(SIGQUIT);
  signals.async_wait(boost::bind(&boost::asio::io_service::stop, &io_service));

  io_service.run();

  return 0;
}

void bot_environment::on_error(error e, const std::string& msg) {
  std::cerr << "ERROR: " << (int)e << " " << msg << "\n";
}

void bot_environment::on_data(const subscription& sub,
                              const rapidjson::Value& value) {
  if (&sub == &_metadata_subscription)
    on_metadata(value);
  else if (&sub == &_frames_subscription)
    on_frame_data(value);
  else
    BOOST_ASSERT_MSG(false, "Unknown subscription");
}

void bot_environment::on_metadata(const rapidjson::Value& msg) {
  if (!_decoder) {
    _decoder =
        decoder_new(_bot_descriptor->image_width, _bot_descriptor->image_height,
                    _bot_descriptor->pixel_format);
    BOOST_VERIFY(_decoder);
  }

  std::string codec_data = decode64(msg["codecData"].GetString());
  decoder_set_metadata(_decoder, msg["codecName"].GetString(),
                       (const uint8_t*)codec_data.data(), codec_data.size());
  std::cout << "Video decoder initialized\n";
}

void bot_environment::on_frame_data(const rapidjson::Value& msg) {
  if (!_decoder) {
    return;
  }

  std::string frame_data = decode64(msg["d"].GetString());
  decoder_process_frame(_decoder, (const uint8_t*)frame_data.data(),
                        frame_data.size());
  if (decoder_frame_ready(_decoder)) {
    _bot_descriptor->callback(*_bot_context, decoder_image_data(_decoder),
                              decoder_image_width(_decoder),
                              decoder_image_height(_decoder),
                              decoder_image_line_size(_decoder));
    send_messages();
  }
}

void bot_environment::send_message(bot_message message) {
  switch (message.kind) {
    case bot_message_kind::ANALYSIS:
      _client->publish(_channel + analysis_channel_suffix, message.data);
      break;
    case bot_message_kind::DEBUG:
      _client->publish(_channel + debug_channel_suffix, message.data);
      break;
  }
  cbor_decref(&message.data);
}

void bot_environment::send_messages() {
  for (std::list<bot_message>::iterator it =
           _bot_context->message_buffer.begin();
       it != _bot_context->message_buffer.end(); it++)
    send_message(*it);
  _bot_context->message_buffer.clear();
}

void bot_environment::store_bot_message(const bot_message_kind kind,
                                        cbor_item_t* message) {
  bot_message newmsg{message, kind};
  cbor_incref(message);
  _bot_context->message_buffer.push_back(newmsg);
}

}  // namespace video
}  // namespace rtm

void rtm_video_bot_message(bot_instance& ctx, const bot_message_kind kind,
                           cbor_item_t* message) {
  ctx.env.store_bot_message(kind, message);
}

void rtm_video_bot_register(const bot_descriptor& bot) {
  rtm::video::bot_environment::instance().register_bot(&bot);
}

int rtm_video_bot_main(int argc, char* argv[]) {
  return rtm::video::bot_environment::instance().main(argc, argv);
}