#ifndef BGZ_BASE_PIPES_H
#define BGZ_BASE_PIPES_H

////////////////////////////////
//~ bgz: Named Pipes
//
// One program asking another on the same machine, a message at a time. The
// server waits for a client, reads its request, writes one reply and lets it
// go; the client connects, sends, reads the reply and hangs up. Message mode:
// each read is one whole message, up to PIPE_MESSAGE_MAX.
//
// name is the part after \\.\pipe\. One server per name: a second process
// opening the same name gets 0. Only clients on this machine can connect, and
// only the same user can send a request.
//
// The server's calls block, so they are for a thread of its own.
// PipeServerStop, from any thread, makes the one waiting give up, so the thread
// can be told to finish; PipeServerClose once it has. A client that connects
// and then goes quiet is given up on after a few seconds.

#define PIPE_MESSAGE_MAX MB(16)

// Opaque; defined by each backend.
typedef struct PipeServer PipeServer;

exported PipeServer *PipeServerOpen(Arena *arena, String8 name);                    // 0 when taken or on failure
exported B32         PipeServerAccept(PipeServer *server);                          // waits for a client; 0 when stopped or on failure
exported B32         PipeServerRead(Arena *arena, PipeServer *server, String8 *out); // the client's message
exported B32         PipeServerWrite(PipeServer *server, String8 message);
exported void        PipeServerDisconnect(PipeServer *server);                      // once the client has the reply; then Accept the next
exported void        PipeServerStop(PipeServer *server);                            // any thread; for good
exported void        PipeServerClose(PipeServer *server);

typedef struct PipeRequestResult PipeRequestResult;
struct PipeRequestResult
{
  B32     connected; // a server was there
  B32     ok;        // the request went and a whole reply came back
  String8 reply;
};

// Connects to name, sends request and waits for the reply. timeout_ms bounds
// each wait - for a busy server, for the send, for the reply; 0 waits as long
// as it takes.
exported PipeRequestResult PipeRequest(Arena *arena, String8 name, String8 request, U64 timeout_ms);

#endif // BGZ_BASE_PIPES_H
