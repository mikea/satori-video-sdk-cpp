#include <boost/program_options.hpp>
#include <iostream>

#include "cli_streams.h"
#include "logging_impl.h"
#include "rtm_client.h"
#include "streams/asio_streams.h"
#include "video_streams.h"

using namespace satori::video;

namespace {

struct rtm_error_handler : rtm::error_callbacks {
  void on_error(std::error_condition ec) override { LOG(ERROR) << ec.message(); }
};

}  // namespace

// TODO: handle SIGINT, SIGKILL, etc
int main(int argc, char *argv[]) {
  namespace po = boost::program_options;

  po::options_description generic("Generic options");
  generic.add_options()("help", "produce help message");
  generic.add_options()(",v", po::value<std::string>(),
                        "log verbosity level (INFO, WARNING, ERROR, FATAL, OFF, 1-9)");

  satori::video::cli_streams::configuration cli_cfg;
  cli_cfg.enable_file_input = true;
  cli_cfg.enable_camera_input = true;
  cli_cfg.enable_rtm_output = true;
  cli_cfg.enable_generic_output_options = true;

  po::options_description cli_options = cli_cfg.to_boost();
  cli_options.add(generic);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, cli_options), vm);
  po::notify(vm);

  if (argc == 1 || vm.count("help") > 0) {
    std::cerr << cli_options << "\n";
    exit(1);
  }

  if (!cli_cfg.validate(vm)) {
    return -1;
  }

  init_logging(argc, argv);

  boost::asio::io_service io_service;
  boost::asio::ssl::context ssl_context{boost::asio::ssl::context::sslv23};
  rtm_error_handler error_handler;

  std::shared_ptr<rtm::client> rtm_client =
      cli_cfg.rtm_client(vm, io_service, ssl_context, error_handler);

  std::string rtm_channel = cli_cfg.rtm_channel(vm);

  if (auto ec = rtm_client->start()) {
    ABORT() << "error starting rtm client: " << ec.message();
  }

  streams::publisher<satori::video::encoded_packet> source =
      cli_cfg.encoded_publisher(vm, io_service, rtm_client, rtm_channel);

  source = std::move(source) >> streams::do_finally([&rtm_client]() {
             if (auto ec = rtm_client->stop()) {
               LOG(ERROR) << "error stopping rtm client: " << ec.message();
             }
           });

  source->subscribe(cli_cfg.encoded_subscriber(vm, rtm_client, rtm_channel));

  io_service.run();
}
