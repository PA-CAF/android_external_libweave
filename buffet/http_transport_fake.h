// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_HTTP_TRANSPORT_FAKE_H_
#define BUFFET_HTTP_TRANSPORT_FAKE_H_

#include <type_traits>

#include <base/callback.h>
#include <base/values.h>

#include "buffet/http_transport.h"
#include "buffet/http_utils.h"

namespace chromeos {
namespace http {
namespace fake {

class ServerRequest;
class ServerResponse;
class Connection;

///////////////////////////////////////////////////////////////////////////////
// A fake implementation of http::Transport that simulates HTTP communication
// with a server.
///////////////////////////////////////////////////////////////////////////////
class Transport : public http::Transport {
 public:
  Transport();
  virtual ~Transport();

  // Server handler callback signature.
  typedef base::Callback<void(const ServerRequest&, ServerResponse*)>
      HandlerCallback;

  // This method allows the test code to provide a callback to handle requests
  // for specific URL/HTTP-verb combination. When a specific |method| request
  // is made on the given |url|, the |handler| will be invoked and all the
  // request data will be filled in the |ServerRequest| parameter. Any server
  // response should be returned through the |ServerResponse| parameter.
  // Either |method| or |url| (or both) can be specified as "*" to handle
  // any requests. So, ("http://localhost","*") will handle any request type
  // on that URL and ("*","GET") will handle any GET requests.
  // The lookup starts with the most specific data pair to the catch-all (*,*).
  void AddHandler(const std::string& url, const std::string& method,
                  const HandlerCallback& handler);
  // Simple version of AddHandler. AddSimpleReplyHandler just returns the
  // specified text response of given MIME type.
  void AddSimpleReplyHandler(const std::string& url,
                             const std::string& method,
                             int status_code,
                             const std::string& reply_text,
                             const std::string& mime_type);
  // Retrieve a handler for specific |url| and request |method|.
  HandlerCallback GetHandler(const std::string& url,
                             const std::string& method) const;

  // For tests that want to assert on the number of HTTP requests sent,
  // these methods can be used to do just that.
  int GetRequestCount() const { return request_count_; }
  void ResetRequestCount() { request_count_ = 0; }

  // Overload from http::Transport
  virtual std::unique_ptr<http::Connection> CreateConnection(
      std::shared_ptr<http::Transport> transport,
      const std::string& url,
      const std::string& method,
      const HeaderList& headers,
      const std::string& user_agent,
      const std::string& referer,
      std::string* error_msg) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Transport);

  // A list of user-supplied request handlers.
  std::map<std::string, HandlerCallback> handlers_;
  // Counter incremented each time a request is made.
  int request_count_ = 0;
};

///////////////////////////////////////////////////////////////////////////////
// A base class for ServerRequest and ServerResponse. It provides common
// functionality to work with request/response HTTP headers and data.
///////////////////////////////////////////////////////////////////////////////
class ServerRequestResponseBase {
 public:
  ServerRequestResponseBase() = default;

  // Add/retrieve request/response body data.
  void AddData(const void* data, size_t data_size);
  const std::vector<unsigned char>& GetData() const { return data_; }
  std::string GetDataAsString() const;
  std::unique_ptr<base::DictionaryValue> GetDataAsJson() const;

  // Add/retrieve request/response HTTP headers.
  void AddHeaders(const HeaderList& headers);
  std::string GetHeader(const std::string& header_name) const;
  const std::map<std::string, std::string>& GetHeaders() const {
    return headers_;
  }

 protected:
  // Data buffer.
  std::vector<unsigned char> data_;
  // Header map.
  std::map<std::string, std::string> headers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServerRequestResponseBase);
};

///////////////////////////////////////////////////////////////////////////////
// A container class that encapsulates all the HTTP server request information.
///////////////////////////////////////////////////////////////////////////////
class ServerRequest : public ServerRequestResponseBase {
 public:
  ServerRequest(const std::string& url, const std::string& method);

  // Get the actual request URL. Does not include the query string or fragment.
  const std::string& GetURL() const { return url_; }
  // Get the request method.
  const std::string& GetMethod() const { return method_; }
  // Get the POST/GET request parameters. These are parsed query string
  // parameters from the URL. In addition, for POST requests with
  // application/x-www-form-urlencoded content type, the request body is also
  // parsed and individual fields can be accessed through this method.
  std::string GetFormField(const std::string& field_name) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServerRequest);

  // Request URL (without query string or URL fragment).
  std::string url_;
  // Request method
  std::string method_;
  // List of available request data form fields.
  mutable std::map<std::string, std::string> form_fields_;
  // Flag used on first request to GetFormField to parse the body of HTTP POST
  // request with application/x-www-form-urlencoded content.
  mutable bool form_fields_parsed_ = false;
};

///////////////////////////////////////////////////////////////////////////////
// A container class that encapsulates all the HTTP server response information.
// The request handler will use this class to provide a response to the caller.
// Call the Reply() or the approriate ReplyNNN() specialization to provide
// the response data. Additional calls to AddHeaders() can be made to provide
// custom response headers. The Reply-methods will already provide the
// followig response headers:
//    Content-Length
//    Content-Type
///////////////////////////////////////////////////////////////////////////////
class ServerResponse : public ServerRequestResponseBase {
public:
  ServerResponse() = default;

  // Generic reply method.
  void Reply(int status_code, const void* data, size_t data_size,
             const char* mime_type);
  // Reply with text body.
  void ReplyText(int status_code, const std::string& text,
                 const char* mime_type);
  // Reply with JSON object. The content type will be "application/json".
  void ReplyJson(int status_code, const base::Value* json);
  // Special form for JSON response for simple objects that have a flat
  // list of key-value pairs of string type.
  void ReplyJson(int status_code, const FormFieldList& fields);

  // Specialized overload to send the binary data as an array of simple
  // data elements. Only trivial data types (scalars, POD structures, etc)
  // can be used.
  template<typename T>
  void Reply(int status_code, const std::vector<T>& data,
             const char* mime_type) {
    // Make sure T doesn't have virtual functions, custom constructors, etc.
    static_assert(std::is_trivial<T>::value, "Only simple data is supported");
    Reply(status_code, data.data(), data.size() * sizeof(T), mime_type);
  }

  // Specialized overload to send the binary data.
  // Only trivial data types (scalars, POD structures, etc) can be used.
  template<typename T>
  void Reply(int status_code, const T& data, const char* mime_type) {
    // Make sure T doesn't have virtual functions, custom constructors, etc.
    static_assert(std::is_trivial<T>::value, "Only simple data is supported");
    Reply(status_code, &data, sizeof(T), mime_type);
  }

  // For handlers that want to simulate versions of HTTP protocol other
  // than HTTP/1.1, call this method with the custom version string,
  // for example "HTTP/1.0".
  void SetProtocolVersion(const std::string& protocol_version) {
    protocol_version_ = protocol_version;
  }

 protected:
  // These methods are helpers to implement corresponding functionality
  // of fake::Connection.
  friend class Connection;
  // Helper for fake::Connection::GetResponseStatusCode().
  int GetStatusCode() const { return status_code_; }
  // Helper for fake::Connection::GetResponseStatusText().
  std::string GetStatusText() const;
  // Helper for fake::Connection::GetProtocolVersion().
  std::string GetProtocolVersion() const { return protocol_version_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServerResponse);

  int status_code_ = 0;
  std::string protocol_version_ = "HTTP/1.1";
};

} // namespace fake
} // namespace http
} // namespace chromeos

#endif // BUFFET_HTTP_TRANSPORT_FAKE_H_