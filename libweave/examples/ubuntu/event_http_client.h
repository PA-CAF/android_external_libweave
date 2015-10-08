// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_EXAMPLES_UBUNTU_EVENT_HTTP_CLIENT_H_
#define LIBWEAVE_EXAMPLES_UBUNTU_EVENT_HTTP_CLIENT_H_

#include <string>

#include <base/memory/weak_ptr.h>
#include <weave/provider/http_client.h>

namespace weave {
namespace examples {

class EventTaskRunner;

// Basic implementation of weave::HttpClient using libevent.
class EventHttpClient : public provider::HttpClient {
 public:
  explicit EventHttpClient(EventTaskRunner* task_runner);

  std::unique_ptr<Response> SendRequestAndBlock(const std::string& method,
                                                const std::string& url,
                                                const Headers& headers,
                                                const std::string& data,
                                                ErrorPtr* error) override;
  int SendRequest(const std::string& method,
                  const std::string& url,
                  const Headers& headers,
                  const std::string& data,
                  const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback) override;

 private:
  EventTaskRunner* task_runner_{nullptr};
  int request_id_ = 0;

  base::WeakPtrFactory<EventHttpClient> weak_ptr_factory_{this};
};

}  // namespace examples
}  // namespace weave

#endif  // LIBWEAVE_EXAMPLES_UBUNTU_EVENT_HTTP_CLIENT_H_