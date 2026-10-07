#pragma once
#include "byte_source.h"

// Minimal HTTP/1.1 response reader over a ByteSource: status line and the two
// headers that matter for framing, then the body with Content-Length or
// chunked transfer-encoding handled. Pure; no Arduino dependency.
struct HttpResponseInfo {
  int status = 0;
  long contentLength = -1;   // -1 = not given
  bool chunked = false;
};

// Reads the status line and headers up to the blank line. False when the
// input ends first or the status line is malformed.
bool httpReadHeaders(ByteSource& src, HttpResponseInfo& info);

class HttpBody : public ByteSource {
 public:
  HttpBody(ByteSource& src, const HttpResponseInfo& info);
  // Next body byte, or -1 at the end of the body (see complete()) or on error.
  int read() override;
  // True once the whole body was delivered as framed by the headers.
  bool complete() const { return complete_; }

 private:
  bool readChunkSize();
  void skipLine();

  ByteSource& src_;
  bool chunked_;
  long remaining_;   // bytes left in the body or in the current chunk; -1 = read to end of stream
  bool complete_ = false;
  bool ended_ = false;
};
