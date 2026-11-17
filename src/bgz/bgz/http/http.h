////////////////////////////////
//~ bgz: HTTP
//
// One request to a web server, http or https, its answer read as it comes in.
// bgz's own: raddebugger's http layer (rad/http/) is a queue polled from its
// async lanes, sends only three headers and hands the body over once it is all
// in, and it is not built. The names here do not clash with it, so both can be
// built together.
//
// HttpRequest blocks until the answer is all in, so it is for a thread that
// can wait - the AI's, a job's - never the main one. Each piece of the body is
// handed to on_body as it arrives, which is how a streamed answer is shown a
// word at a time. Without on_body the whole body comes back in the result.
//
// Stopping one part way: on_body returning 0, or cancelled returning 1.
// cancelled is asked at least every HTTP_CANCEL_CHECK_MS while it waits, so a
// request stuck on a slow server still stops at once.
//
// Proxies are the system's. https checks the server's certificate the way
// Windows does; there is no way to turn that off.

#ifndef BGZ_HTTP_H
#define BGZ_HTTP_H

#define HTTP_CANCEL_CHECK_MS 50

typedef struct HttpHeader HttpHeader;
struct HttpHeader
{
  String8 name;
  String8 value;
};

// One piece of the body, after status has come back. 0 stops the request.
typedef B32 HttpBodyProc(void *user, U32 status, String8 piece);

// 1 stops the request.
typedef B32 HttpCancelledProc(void *user);

typedef struct HttpRequestParams HttpRequestParams;
struct HttpRequestParams
{
  String8     method; // "GET", "POST", ...; empty is GET
  String8     url;    // http://host[:port]/path or https://...
  HttpHeader *headers;
  U64         header_count;
  String8     body;

  // The longest any one step may take - connecting, sending, the wait for the
  // next piece of the answer - before it fails. 0: the system's (30 s for a
  // piece of the answer).
  U64 timeout_ms;

  HttpBodyProc      *on_body;   // 0: the body comes back in HttpResult.body
  HttpCancelledProc *cancelled; // optional
  void              *user;      // for both
};

typedef struct HttpResult HttpResult;
struct HttpResult
{
  B32     answered;  // the server sent a status; it can still be an error status
  B32     done;      // the whole body was read
  B32     cancelled; // stopped by on_body or cancelled
  U32     status;    // 200, 401, ...; 0 until answered
  String8 body;      // only without on_body
  String8 error;     // what went wrong when not answered or not done (and not cancelled), for the Log
};

exported HttpResult HttpRequest(Arena *arena, HttpRequestParams *params);

#endif // BGZ_HTTP_H
