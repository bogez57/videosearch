////////////////////////////////
//~ bgz: HTTP, Windows
//
// WinHTTP, asynchronous: each step (send, receive, wait for data, read) is
// started, then waited for on an event the status callback sets, a slice of
// HTTP_CANCEL_CHECK_MS at a time - so a cancel is seen while a step is still
// waiting on the server. A cancel, or any other end, closes the request
// handle, which is how WinHTTP stops a step in flight, and waits for the
// callback's last call (HANDLE_CLOSING): the callback writes into the
// W32_HttpCall on this stack, so it must be done before HttpRequest returns.

#include <winhttp.h>

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
# define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

#define W32_HTTP_READ_MAX KB(64)

typedef struct W32_HttpCall W32_HttpCall;
struct W32_HttpCall
{
  HANDLE         step_event;   // manual reset; set when the step in flight completes or fails
  HANDLE         closed_event; // set by HANDLE_CLOSING
  volatile DWORD bytes;        // DATA_AVAILABLE and READ_COMPLETE
  volatile DWORD error;        // REQUEST_ERROR
};

internal void CALLBACK
W32_HttpCallback(HINTERNET handle, DWORD_PTR context, DWORD status, void *info, DWORD info_size) {
   W32_HttpCall *call = (W32_HttpCall *)context;
   (void)handle;

   if (call != 0) {
      switch (status) {
         case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
         case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE: {
            SetEvent(call->step_event);
         } break;

         case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE: {
            call->bytes = *(DWORD *)info;
            SetEvent(call->step_event);
         } break;

         case WINHTTP_CALLBACK_STATUS_READ_COMPLETE: {
            call->bytes = info_size;
            SetEvent(call->step_event);
         } break;

         case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR: {
            WINHTTP_ASYNC_RESULT *async = (WINHTTP_ASYNC_RESULT *)info;
            call->error = async->dwError != 0 ? async->dwError : ERROR_WINHTTP_INTERNAL_ERROR;
            SetEvent(call->step_event);
         } break;

         case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING: {
            SetEvent(call->closed_event);
         } break;
      }
   }
}

internal void
W32_HttpStepBegin(W32_HttpCall *call) {
   ResetEvent(call->step_event);
   call->bytes = 0;
   call->error = 0;
}

// started: what the WinHTTP call that starts the step returned. 1 when the
// step completed; 0 when it failed (*error) or was cancelled (*cancelled).
internal B32
W32_HttpStepEnd(W32_HttpCall *call, BOOL started, HttpRequestParams *params, B32 *cancelled, DWORD *error) {
   B32 result = 0;

   if (!started) {
      *error = GetLastError();
   }
   else {
      for (;;) {
         if (WaitForSingleObject(call->step_event, HTTP_CANCEL_CHECK_MS) == WAIT_OBJECT_0) {
            result = call->error == 0;
            *error = call->error;
            break;
         }

         if (params->cancelled != 0 && params->cancelled(params->user)) {
            *cancelled = 1;
            break;
         }
      }
   }

   return result;
}

internal String8
W32_HttpErrorText(Arena *arena, DWORD error) {
   char *text = "WinHTTP failed";

   switch (error) {
      case ERROR_WINHTTP_TIMEOUT:             text = "the server took too long to answer"; break;
      case ERROR_WINHTTP_NAME_NOT_RESOLVED:   text = "the server's name was not found"; break;
      case ERROR_WINHTTP_CANNOT_CONNECT:      text = "could not connect to the server"; break;
      case ERROR_WINHTTP_CONNECTION_ERROR:    text = "the connection was lost"; break;
      case ERROR_WINHTTP_SECURE_FAILURE:      text = "the secure connection failed (the server's certificate)"; break;
      case ERROR_WINHTTP_INVALID_URL:
      case ERROR_WINHTTP_UNRECOGNIZED_SCHEME: text = "not an http or https address"; break;
   }

   return push_str8f(arena, "%s (error %u)", text, (U32)error);
}

exported HttpResult
HttpRequest(Arena *arena, HttpRequestParams *params) {
   HttpResult   result = {0};
   Temp         scratch = scratch_begin(&arena, 1);
   String8List  body_pieces = {0};
   W32_HttpCall call = {0};
   HINTERNET    session = 0;
   HINTERNET    connection = 0;
   HINTERNET    request = 0;
   B32          callback_set = 0;
   B32          cancelled = 0;
   DWORD        error = 0;

   call.step_event = CreateEventW(0, TRUE, FALSE, 0);
   call.closed_event = CreateEventW(0, TRUE, FALSE, 0);

   //- the address: host, port and path (with the ?query), null terminated
   String16       url16 = str16_from_8(scratch.arena, params->url);
   URL_COMPONENTS url = {0};
   url.dwStructSize = sizeof(url);
   url.dwSchemeLength = (DWORD)-1;
   url.dwHostNameLength = (DWORD)-1;
   url.dwUrlPathLength = (DWORD)-1;
   url.dwExtraInfoLength = (DWORD)-1;

   B32    url_ok = WinHttpCrackUrl((WCHAR *)url16.str, (DWORD)url16.size, 0, &url) &&
                   (url.nScheme == INTERNET_SCHEME_HTTP || url.nScheme == INTERNET_SCHEME_HTTPS) && url.dwHostNameLength != 0;
   WCHAR *host = 0;
   WCHAR *path = 0;

   if (!url_ok) {
      error = ERROR_WINHTTP_INVALID_URL;
   }
   else {
      DWORD path_size = url.dwUrlPathLength + url.dwExtraInfoLength;

      host = push_array(scratch.arena, WCHAR, url.dwHostNameLength + 1);
      MemoryCopy(host, url.lpszHostName, url.dwHostNameLength * sizeof(WCHAR));

      path = push_array(scratch.arena, WCHAR, path_size + 2);
      if (path_size == 0) {
         path[0] = L'/';
      }
      else {
         MemoryCopy(path, url.lpszUrlPath, path_size * sizeof(WCHAR));
      }
   }

   //- session, connection, request
   if (url_ok) {
      // AUTOMATIC_PROXY is Windows 8.1 and later; DEFAULT_PROXY before.
      session = WinHttpOpen(L"bgz", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);

      if (session == 0) {
         session = WinHttpOpen(L"bgz", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
      }

      if (session != 0 && params->timeout_ms != 0) {
         int timeout = (int)Min(params->timeout_ms, (U64)0x7fffffff);
         WinHttpSetTimeouts(session, timeout, timeout, timeout, timeout);
      }

      connection = session != 0 ? WinHttpConnect(session, host, url.nPort, 0) : 0;

      if (connection != 0) {
         String8  method = params->method.size != 0 ? params->method : str8_lit("GET");
         String16 method16 = str16_from_8(scratch.arena, method);
         DWORD    flags = url.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;

         request = WinHttpOpenRequest(connection, (WCHAR *)method16.str, path, 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
      }

      if (request != 0) {
         DWORD_PTR context = (DWORD_PTR)&call;
         WinHttpSetOption(request, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof(context));
         callback_set = WinHttpSetStatusCallback(request, W32_HttpCallback, WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES, 0) !=
                        WINHTTP_INVALID_STATUS_CALLBACK;
      }

      if (request == 0 || !callback_set) {
         error = GetLastError();
      }
   }

   //- send, and wait for the status
   B32 sent = 0;

   if (request != 0 && callback_set) {
      String8List header_lines = {0};

      for (U64 i = 0; i < params->header_count; i += 1) {
         str8_list_pushf(scratch.arena, &header_lines, "%S: %S\r\n", params->headers[i].name, params->headers[i].value);
      }

      String16 headers16 = str16_from_8(scratch.arena, str8_list_join(scratch.arena, &header_lines, 0));
      WCHAR   *headers = headers16.size != 0 ? (WCHAR *)headers16.str : WINHTTP_NO_ADDITIONAL_HEADERS;
      void    *body = params->body.size != 0 ? (void *)params->body.str : WINHTTP_NO_REQUEST_DATA;
      DWORD    body_size = (DWORD)params->body.size;

      W32_HttpStepBegin(&call);
      sent = W32_HttpStepEnd(&call, WinHttpSendRequest(request, headers, (DWORD)headers16.size, body, body_size, body_size, (DWORD_PTR)&call),
                             params, &cancelled, &error);

      if (sent) {
         W32_HttpStepBegin(&call);
         sent = W32_HttpStepEnd(&call, WinHttpReceiveResponse(request, 0), params, &cancelled, &error);
      }
   }

   if (sent) {
      DWORD status = 0;
      DWORD status_size = sizeof(status);

      WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                          WINHTTP_NO_HEADER_INDEX);
      result.answered = 1;
      result.status = (U32)status;
   }

   //- the body, a piece at a time
   if (result.answered) {
      U8 *buffer = push_array_no_zero(scratch.arena, U8, W32_HTTP_READ_MAX);

      for (;;) {
         W32_HttpStepBegin(&call);
         if (!W32_HttpStepEnd(&call, WinHttpQueryDataAvailable(request, 0), params, &cancelled, &error)) {
            break;
         }

         DWORD available = call.bytes;

         if (available == 0) {
            result.done = 1;
            break;
         }

         W32_HttpStepBegin(&call);
         if (!W32_HttpStepEnd(&call, WinHttpReadData(request, buffer, (DWORD)Min(available, W32_HTTP_READ_MAX), 0), params, &cancelled, &error)) {
            break;
         }

         String8 piece = str8(buffer, call.bytes);

         if (piece.size == 0) {
            result.done = 1;
            break;
         }

         if (params->on_body != 0) {
            if (!params->on_body(params->user, result.status, piece)) {
               cancelled = 1;
               break;
            }
         }
         else {
            str8_list_push(scratch.arena, &body_pieces, push_str8_copy(scratch.arena, piece));
         }
      }
   }

   //- close, and wait for the callback's last call
   if (request != 0) {
      WinHttpCloseHandle(request);

      if (callback_set) {
         WaitForSingleObject(call.closed_event, INFINITE);
      }
   }

   if (connection != 0) {
      WinHttpCloseHandle(connection);
   }

   if (session != 0) {
      WinHttpCloseHandle(session);
   }

   CloseHandle(call.step_event);
   CloseHandle(call.closed_event);

   result.cancelled = cancelled;

   if (params->on_body == 0 && result.done) {
      result.body = str8_list_join(arena, &body_pieces, 0);
   }

   if (!result.done && !cancelled) {
      result.error = W32_HttpErrorText(arena, error);
   }

   scratch_end(scratch);
   return result;
}
