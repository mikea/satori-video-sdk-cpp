#define BOOST_TEST_MODULE FileSourceTest
#include <boost/test/included/unit_test.hpp>

#include "data.h"
#include "video_streams.h"

namespace satori {
namespace video {

namespace {

inline frame_id id(int64_t i1, int64_t i2) { return frame_id{i1, i2}; }

}  // namespace

BOOST_AUTO_TEST_CASE(test_frame_ids) {
  boost::asio::io_service io;

  std::vector<frame_id> ids;
  auto when_done = file_source(io, "test_data/test.mp4", false, true)
                       ->process([&ids](encoded_packet &&pkt) {
                         if (const encoded_frame *f = boost::get<encoded_frame>(&pkt)) {
                           ids.push_back(f->id);
                         }
                       });
  BOOST_TEST(when_done.ok());

  BOOST_TEST(ids.size() == 6);
  CHECK(ids[0] == id(0, 48));
  BOOST_TEST(ids[0] == id(0, 48));
  BOOST_TEST(ids[1] == id(49, 28975));
  BOOST_TEST(ids[2] == id(28976, 32918));
  BOOST_TEST(ids[3] == id(32919, 38321));
  BOOST_TEST(ids[4] == id(38322, 44809));
  BOOST_TEST(ids[5] == id(44810, 47582));
}

BOOST_AUTO_TEST_CASE(test_repeat_metadata) {
  boost::asio::io_service io;
  size_t metadata_count = 0;

  auto when_done =
      (file_source(io, "test_data/test.mp4", false, true)
       >> streams::repeat_if<encoded_packet>(0,
                                             [](const encoded_packet &p) {
                                               return nullptr
                                                      != boost::get<encoded_metadata>(&p);
                                             }))
          ->process([&metadata_count](encoded_packet &&pkt) {
            if (const encoded_metadata *m = boost::get<encoded_metadata>(&pkt)) {
              metadata_count++;
            }
          });
  BOOST_TEST(when_done.ok());
  BOOST_TEST(metadata_count == 7);
}

}  // namespace video
}  // namespace satori
