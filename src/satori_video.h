// RTM video stream is published to multiple RTM channels. The main video
// channel contains video frames along with frames related metadata, like
// timestamps, sequence id, chunk id and number of chunks. A number of RTM
// subchannels is used to send additional information like bot control commands,
// codec metadata, bot output. Subchannels names use the following pattern:
// "<video_channel_name>/<subchannel_suffix>", for example
// "test-camera/analysis".
#pragma once

// RTM channel for bot control commands.
// Bot will be automatically subscribed to this channel.
// A user has to implement bot control callbacks.
constexpr char control_channel_suffix[] = "/control";

// RTM subchannel for frames data.
// Bot will be automatically subscribed to this channel.
constexpr char frames_channel_suffix[] = "";

// RTM subchannel for codec specific metadata.
// In case of h264 metadata it will contain SPS and PPS.
// Infrequent data should be expected.
constexpr char metadata_channel_suffix[] = "/metadata";

// RTM subchannel for bot output.
// Output format is defined by user.
constexpr char analysis_channel_suffix[] = "/analysis";

// RTM subchannel for bot debugging output.
// Output format is defined by user.
constexpr char debug_channel_suffix[] = "/debug";
