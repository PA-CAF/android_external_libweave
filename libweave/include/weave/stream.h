// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_STREAM_H_
#define LIBWEAVE_INCLUDE_WEAVE_STREAM_H_

#include <string>

#include <base/callback.h>
#include <weave/error.h>

namespace weave {

// Interface for async input streaming.
class InputStream {
 public:
  virtual ~InputStream() = default;

  // Callbacks types for Read.
  using ReadSuccessCallback = base::Callback<void(size_t size)>;

  // Implementation should return immediately and post either success_callback
  // or error_callback. Caller guarantees that buffet is alive until either of
  // callback is called.
  virtual void Read(void* buffer,
                    size_t size_to_read,
                    const ReadSuccessCallback& success_callback,
                    const ErrorCallback& error_callback) = 0;
};

// Interface for async input streaming.
class OutputStream {
 public:
  virtual ~OutputStream() = default;

  // Implementation should return immediately and post either success_callback
  // or error_callback. Caller guarantees that buffet is alive until either of
  // callback is called.
  // Success callback must be called only after all data is written.
  virtual void Write(const void* buffer,
                     size_t size_to_write,
                     const SuccessCallback& success_callback,
                     const ErrorCallback& error_callback) = 0;
};

// Interface for async bi-directional streaming.
class Stream : public InputStream, public OutputStream {
 public:
  ~Stream() override = default;

  // Cancels all pending read or write requests. Canceled operations must not
  // call any callbacks.
  virtual void CancelPendingOperations() = 0;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_STREAM_H_
