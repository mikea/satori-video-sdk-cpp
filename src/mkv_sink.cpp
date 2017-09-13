#include <stdexcept>

#include "avutils.h"
#include "librtmvideo/data.h"
#include "logging.h"
#include "streams.h"
#include "video_streams.h"

namespace rtm {
namespace video {

// TODO: use AVMEDIA_TYPE_DATA
struct mkv_sink_impl : public streams::subscriber<encoded_packet> {
  mkv_sink_impl(const std::string &filename) : _filename(filename) {
    avutils::init();

    _format_context =
        avutils::format_context("matroska", _filename, [filename](AVFormatContext *ctx) {
          if (ctx->pb != nullptr) {
            LOG_S(INFO) << "Writing trailer section into file " << filename;
            av_write_trailer(ctx);
            LOG_S(INFO) << "Closing file " << filename;
            avio_closep(&ctx->pb);
          }
        });
    if (_format_context == nullptr) {
      throw std::runtime_error{"could not create " + _filename};
    }
  }

  void on_next(encoded_packet &&packet) override {
    if (const encoded_metadata *m = boost::get<encoded_metadata>(&packet)) {
      on_encoded_metadata(*m);
    } else if (const encoded_frame *f = boost::get<encoded_frame>(&packet)) {
      on_encoded_frame(*f);
    } else {
      BOOST_VERIFY_MSG(false, "Bad variant");
    }
    _src->request(1);
  }

  void on_error(std::error_condition ec) override {
    LOG_S(ERROR) << ec.message();
    exit(1);
  }

  void on_complete() override {
    LOG_S(INFO) << "got complete";
    delete this;
  }

  void on_subscribe(streams::subscription &s) override {
    _src = &s;
    _src->request(1);
  }

  void on_encoded_metadata(const encoded_metadata &metadata) {
    BOOST_VERIFY(metadata.image_size);

    if (_video_stream_index < 0) {
      LOG_S(INFO) << "Initializing matroska sink for file " << _filename;
      std::shared_ptr<AVCodecContext> encoder_context =
          avutils::encoder_context(_encoder_id);
      if (_format_context->oformat->flags & AVFMT_GLOBALHEADER) {
        encoder_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
      }
      encoder_context->width = metadata.image_size->width;
      encoder_context->height = metadata.image_size->height;

      LOG_S(INFO) << "Creating video stream for file " << _filename;
      AVStream *video_stream = avformat_new_stream(_format_context.get(), nullptr);
      if (video_stream == nullptr) {
        throw std::runtime_error{"failed to create an output video stream"};
      }
      video_stream->id = _format_context->nb_streams - 1;
      video_stream->time_base = encoder_context->time_base;
      _video_stream_index = video_stream->index;

      LOG_S(INFO) << "Copying encoder parameters into video stream for file "
                  << _filename;
      int ret =
          avcodec_parameters_from_context(video_stream->codecpar, encoder_context.get());
      if (ret < 0) {
        throw std::runtime_error{"failed to copy codec parameters: "
                                 + avutils::error_msg(ret)};
      }

      LOG_S(INFO) << "Opening file " << _filename;
      ret = avio_open(&_format_context->pb, _filename.c_str(), AVIO_FLAG_WRITE);
      if (ret < 0) {
        throw std::runtime_error{"failed to open file: " + avutils::error_msg(ret)};
      }

      LOG_S(INFO) << "Writing header section into file " << _filename;
      ret = avformat_write_header(_format_context.get(), nullptr);
      if (ret < 0) {
        throw std::runtime_error{"failed to write header: " + avutils::error_msg(ret)};
      }

      _first_frame_ts = std::chrono::system_clock::now();
    }
  }

  void on_encoded_frame(const encoded_frame &f) {
    if (_video_stream_index < 0) return;

    const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = (uint8_t *)f.data.data();
    packet.size = f.data.size();
    packet.pts =
        1
        + std::chrono::duration_cast<std::chrono::milliseconds>(now - _first_frame_ts)
              .count();
    packet.stream_index = _video_stream_index;
    int ret = av_write_frame(_format_context.get(), &packet);
    if (ret < 0) {
      throw std::runtime_error{"failed to write packet: " + avutils::error_msg(ret)};
    }
    av_packet_unref(&packet);
  }

  streams::subscription *_src;
  const std::string _filename;
  const AVCodecID _encoder_id{AV_CODEC_ID_VP9};
  std::shared_ptr<AVFormatContext> _format_context{nullptr};
  int _video_stream_index{-1};
  std::chrono::system_clock::time_point _first_frame_ts;
};

streams::subscriber<encoded_packet> &mkv_sink(const std::string &filename) {
  return *(new mkv_sink_impl(filename));
}

}  // namespace video
}  // namespace rtm