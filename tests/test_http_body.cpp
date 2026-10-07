#include <string>
#include "test.h"
#include "host_sources.h"
#include "http_body.h"

static std::string drain(ByteSource& b) {
  std::string s;
  for (int c = b.read(); c >= 0; c = b.read()) s.push_back((char)c);
  return s;
}

TEST(http_content_length_body) {
  StringSource src("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\ncontent-length: 5\r\nExpires: x\r\n\r\nhelloEXTRA");
  HttpResponseInfo info;
  CHECK(httpReadHeaders(src, info));
  CHECK_EQ(info.status, 200);
  CHECK_EQ(info.contentLength, 5);
  CHECK(!info.chunked);
  HttpBody body(src, info);
  CHECK_STR_EQ(drain(body).c_str(), "hello");
  CHECK(body.complete());
}

TEST(http_chunked_body) {
  StringSource src("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n5;ext=1\r\npedia\r\n0\r\nTrailer: x\r\n\r\n");
  HttpResponseInfo info;
  CHECK(httpReadHeaders(src, info));
  CHECK(info.chunked);
  HttpBody body(src, info);
  CHECK_STR_EQ(drain(body).c_str(), "Wikipedia");
  CHECK(body.complete());
}

TEST(http_non_200_status_is_reported) {
  StringSource src("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n");
  HttpResponseInfo info;
  CHECK(httpReadHeaders(src, info));
  CHECK_EQ(info.status, 503);
}

TEST(http_truncated_headers_fail) {
  StringSource src("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n");
  HttpResponseInfo info;
  CHECK(!httpReadHeaders(src, info));
}

TEST(http_truncated_body_is_not_complete) {
  StringSource src("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nhello");
  HttpResponseInfo info;
  CHECK(httpReadHeaders(src, info));
  HttpBody body(src, info);
  CHECK_STR_EQ(drain(body).c_str(), "hello");
  CHECK(!body.complete());
}

TEST(http_bare_lf_line_endings_tolerated) {
  StringSource src("HTTP/1.1 200 OK\nContent-Length: 2\n\nok");
  HttpResponseInfo info;
  CHECK(httpReadHeaders(src, info));
  HttpBody body(src, info);
  CHECK_STR_EQ(drain(body).c_str(), "ok");
}
