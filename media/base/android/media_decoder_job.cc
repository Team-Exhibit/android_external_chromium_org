// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_decoder_job.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/bind_to_loop.h"

namespace media {

// Timeout value for media codec operations. Because the first
// DequeInputBuffer() can take about 150 milliseconds, use 250 milliseconds
// here. See http://b/9357571.
static const int kMediaCodecTimeoutInMilliseconds = 250;

MediaDecoderJob::MediaDecoderJob(
    const scoped_refptr<base::MessageLoopProxy>& decoder_loop,
    MediaCodecBridge* media_codec_bridge)
    : ui_loop_(base::MessageLoopProxy::current()),
      decoder_loop_(decoder_loop),
      media_codec_bridge_(media_codec_bridge),
      needs_flush_(false),
      input_eos_encountered_(false),
      weak_this_(this),
      is_decoding_(false) {
}

MediaDecoderJob::~MediaDecoderJob() {}

void MediaDecoderJob::Decode(
    const AccessUnit& unit,
    const base::TimeTicks& start_time_ticks,
    const base::TimeDelta& start_presentation_timestamp,
    const MediaDecoderJob::DecoderCallback& callback) {
  DCHECK(!is_decoding_);
  DCHECK(ui_loop_->BelongsToCurrentThread());
  is_decoding_ = true;
  decoder_loop_->PostTask(FROM_HERE, base::Bind(
      &MediaDecoderJob::DecodeInternal, base::Unretained(this), unit,
      start_time_ticks, start_presentation_timestamp, needs_flush_,
      media::BindToLoop(ui_loop_, callback)));
  needs_flush_ = false;
}

MediaDecoderJob::DecodeStatus MediaDecoderJob::QueueInputBuffer(
    const AccessUnit& unit) {
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      kMediaCodecTimeoutInMilliseconds);
  int input_buf_index = media_codec_bridge_->DequeueInputBuffer(timeout);
  if (input_buf_index == MediaCodecBridge::INFO_MEDIA_CODEC_ERROR)
    return DECODE_FAILED;
  if (input_buf_index == MediaCodecBridge::INFO_TRY_AGAIN_LATER)
    return DECODE_TRY_ENQUEUE_INPUT_AGAIN_LATER;

  // TODO(qinmin): skip frames if video is falling far behind.
  DCHECK(input_buf_index >= 0);
  if (unit.end_of_stream || unit.data.empty()) {
    media_codec_bridge_->QueueEOS(input_buf_index);
    return DECODE_INPUT_END_OF_STREAM;
  }
  if (unit.key_id.empty()) {
    media_codec_bridge_->QueueInputBuffer(
        input_buf_index, &unit.data[0], unit.data.size(), unit.timestamp);
  } else {
    if (unit.iv.empty() || unit.subsamples.empty()) {
      LOG(ERROR) << "The access unit doesn't have iv or subsamples while it "
                 << "has key IDs!";
      return DECODE_FAILED;
    }
    media_codec_bridge_->QueueSecureInputBuffer(
        input_buf_index, &unit.data[0], unit.data.size(),
        reinterpret_cast<const uint8*>(&unit.key_id[0]), unit.key_id.size(),
        reinterpret_cast<const uint8*>(&unit.iv[0]), unit.iv.size(),
        &unit.subsamples[0], unit.subsamples.size(), unit.timestamp);
  }

  return DECODE_SUCCEEDED;
}

void MediaDecoderJob::DecodeInternal(
    const AccessUnit& unit,
    const base::TimeTicks& start_time_ticks,
    const base::TimeDelta& start_presentation_timestamp,
    bool needs_flush,
    const MediaDecoderJob::DecoderCallback& callback) {
  if (needs_flush) {
    DVLOG(1) << "DecodeInternal needs flush.";
    input_eos_encountered_ = false;
    media_codec_bridge_->Reset();
  }

  DecodeStatus decode_status = DECODE_INPUT_END_OF_STREAM;
  if (!input_eos_encountered_) {
    decode_status = QueueInputBuffer(unit);
    if (decode_status == DECODE_INPUT_END_OF_STREAM) {
      input_eos_encountered_ = true;
    } else if (decode_status != DECODE_SUCCEEDED) {
      callback.Run(decode_status, start_presentation_timestamp, 0);
      return;
    }
  }

  size_t offset = 0;
  size_t size = 0;
  base::TimeDelta presentation_timestamp;
  bool end_of_stream = false;

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      kMediaCodecTimeoutInMilliseconds);
  int output_buffer_index = media_codec_bridge_->DequeueOutputBuffer(
      timeout, &offset, &size, &presentation_timestamp, &end_of_stream);

  if (end_of_stream)
    decode_status = DECODE_OUTPUT_END_OF_STREAM;

  if (output_buffer_index < 0) {
    MediaCodecBridge::DequeueBufferInfo buffer_info =
        static_cast<MediaCodecBridge::DequeueBufferInfo>(output_buffer_index);
    switch (buffer_info) {
      case MediaCodecBridge::INFO_OUTPUT_BUFFERS_CHANGED:
        DCHECK_NE(decode_status, DECODE_INPUT_END_OF_STREAM);
        media_codec_bridge_->GetOutputBuffers();
        break;
      case MediaCodecBridge::INFO_OUTPUT_FORMAT_CHANGED:
        DCHECK_NE(decode_status, DECODE_INPUT_END_OF_STREAM);
        // TODO(qinmin): figure out what we should do if format changes.
        decode_status = DECODE_FORMAT_CHANGED;
        break;
      case MediaCodecBridge::INFO_TRY_AGAIN_LATER:
        decode_status = DECODE_TRY_DEQUEUE_OUTPUT_AGAIN_LATER;
        break;
      case MediaCodecBridge::INFO_MEDIA_CODEC_ERROR:
        decode_status = DECODE_FAILED;
        break;
    }
  } else {
      base::TimeDelta time_to_render;
      DCHECK(!start_time_ticks.is_null());
      if (ComputeTimeToRender()) {
        time_to_render = presentation_timestamp - (base::TimeTicks::Now() -
            start_time_ticks + start_presentation_timestamp);
      }

      // TODO(acolwell): Change to > since the else will never run for audio.
      if (time_to_render >= base::TimeDelta()) {
        base::MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            base::Bind(&MediaDecoderJob::ReleaseOutputBuffer,
                       weak_this_.GetWeakPtr(), output_buffer_index, size,
                       presentation_timestamp, callback, decode_status),
            time_to_render);
      } else {
        // TODO(qinmin): The codec is lagging behind, need to recalculate the
        // |start_presentation_timestamp_| and |start_time_ticks_|.
        DVLOG(1) << "codec is lagging behind :"
                 << time_to_render.InMicroseconds();
        ReleaseOutputBuffer(output_buffer_index, size, presentation_timestamp,
                            callback, decode_status);
      }

      return;
  }
  callback.Run(decode_status, start_presentation_timestamp, 0);
}

void MediaDecoderJob::OnDecodeCompleted() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  is_decoding_ = false;
}

void MediaDecoderJob::Flush() {
  // Do nothing, flush when the next Decode() happens.
  needs_flush_ = true;
}

void MediaDecoderJob::Release() {
  // If |decoding_| is false, there is nothing running on the decoder thread.
  // So it is safe to delete the MediaDecoderJob on the UI thread. However,
  // if we post a task to the decoder thread to delete object, then we cannot
  // immediately pass the surface to a new MediaDecoderJob instance because
  // the java surface is still owned by the old object. New decoder creation
  // will be blocked on the UI thread until the previous decoder gets deleted.
  // This introduces extra latency during config changes, and makes the logic in
  // MediaSourcePlayer more complicated.
  //
  // TODO(qinmin): Figure out the logic to passing the surface to a new
  // MediaDecoderJob instance after the previous one gets deleted on the decoder
  // thread.
  if (is_decoding_ && !decoder_loop_->BelongsToCurrentThread()) {
    DCHECK(ui_loop_->BelongsToCurrentThread());
    decoder_loop_->DeleteSoon(FROM_HERE, this);
    return;
  }

  delete this;
}

}  // namespace media
