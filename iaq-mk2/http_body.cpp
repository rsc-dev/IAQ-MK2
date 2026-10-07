#include "http_body.h"

#include <stdlib.h>
#include <string.h>

namespace {

// Reads one line into buf (without the line terminator); extra characters are
// consumed and dropped. False if the source ends before a newline.
bool readLine(ByteSource& src, char* buf, size_t cap) {
  size_t n = 0;
  for (;;) {
    const int c = src.read();
    if (c < 0) return false;
    if (c == '\n') break;
    if (c == '\r') continue;
    if (n + 1 < cap) buf[n++] = (char)c;
  }
  buf[n] = '\0';
  return true;
}

bool startsWithNoCase(const char* s, const char* prefix) {
  for (; *prefix; ++s, ++prefix) {
    const char a = (*s >= 'A' && *s <= 'Z') ? (char)(*s + 32) : *s;
    if (a != *prefix) return false;
  }
  return true;
}

bool containsNoCase(const char* s, const char* needle) {
  for (; *s; ++s)
    if (startsWithNoCase(s, needle)) return true;
  return false;
}

const char* afterColon(const char* line) {
  const char* p = strchr(line, ':');
  if (!p) return "";
  ++p;
  while (*p == ' ' || *p == '\t') ++p;
  return p;
}

}  // namespace

bool httpReadHeaders(ByteSource& src, HttpResponseInfo& info) {
  char line[160];
  if (!readLine(src, line, sizeof line)) return false;
  if (!startsWithNoCase(line, "http/")) return false;
  const char* sp = strchr(line, ' ');
  if (!sp) return false;
  info.status = (int)strtol(sp + 1, nullptr, 10);
  if (info.status < 100 || info.status > 599) return false;
  info.contentLength = -1;
  info.chunked = false;
  for (;;) {
    if (!readLine(src, line, sizeof line)) return false;
    if (line[0] == '\0') return true;
    if (startsWithNoCase(line, "content-length:")) {
      info.contentLength = strtol(afterColon(line), nullptr, 10);
    } else if (startsWithNoCase(line, "transfer-encoding:")) {
      info.chunked = containsNoCase(afterColon(line), "chunked");
    }
  }
}

HttpBody::HttpBody(ByteSource& src, const HttpResponseInfo& info)
    : src_(src), chunked_(info.chunked), remaining_(info.chunked ? 0 : info.contentLength) {
  if (!chunked_ && info.contentLength == 0) {
    complete_ = true;
    ended_ = true;
  }
}

void HttpBody::skipLine() {
  char dummy[4];
  readLine(src_, dummy, sizeof dummy);
}

bool HttpBody::readChunkSize() {
  char line[40];
  if (!readLine(src_, line, sizeof line)) return false;
  if (line[0] == '\0' && !readLine(src_, line, sizeof line)) return false;  // CRLF after previous chunk data
  char* end = nullptr;
  const long size = strtol(line, &end, 16);
  if (end == line || size < 0) return false;
  if (size == 0) {
    // Trailers until the blank line; a missing blank line still counts as complete.
    while (readLine(src_, line, sizeof line) && line[0] != '\0') {
    }
    complete_ = true;
    ended_ = true;
    return false;
  }
  remaining_ = size;
  return true;
}

int HttpBody::read() {
  if (ended_) return -1;
  if (chunked_) {
    if (remaining_ == 0 && !readChunkSize()) {
      ended_ = true;
      return -1;
    }
    const int c = src_.read();
    if (c < 0) {
      ended_ = true;
      return -1;
    }
    --remaining_;
    return c;
  }
  if (remaining_ == 0) {
    complete_ = true;
    ended_ = true;
    return -1;
  }
  const int c = src_.read();
  if (c < 0) {
    complete_ = remaining_ < 0;  // unknown length: end of stream is the end of the body
    ended_ = true;
    return -1;
  }
  if (remaining_ > 0 && --remaining_ == 0) complete_ = true;
  return c;
}
