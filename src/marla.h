#ifndef marla_INCLUDED
#define marla_INCLUDED

#include <sys/epoll.h>
#include <openssl/ssl.h>

#define marla_BUFSIZE 1024
#define marla_LOGBUFSIZE 32*1024

// ring.c
typedef struct {
char* buf;
unsigned int read_index;
unsigned int write_index;
size_t capacity;
} marla_Ring;

marla_Ring* marla_Ring_new(size_t capacity);
void marla_Ring_free(marla_Ring* ring);
size_t marla_Ring_size(marla_Ring* ring);
size_t marla_Ring_capacity(marla_Ring* ring);
int marla_Ring_readc(marla_Ring* ring, unsigned char* c);
int marla_Ring_read(marla_Ring* ring, unsigned char* sink, size_t size);
void marla_Ring_putbackRead(marla_Ring* ring, size_t count);
void marla_Ring_putbackWrite(marla_Ring* ring, size_t count);
void marla_Ring_slot(marla_Ring* ring, void** slot, size_t* slotLen);
size_t marla_Ring_write(marla_Ring* ring, const void* source, size_t size);
int marla_Ring_writec(marla_Ring* ring, unsigned char source);
int marla_Ring_writeStr(marla_Ring* ring, const char* source);
void marla_Ring_writeSlot(marla_Ring* ring, void** slot, size_t* slotLen);
void marla_Ring_readSlot(marla_Ring* ring, void** slot, size_t* slotLen);
void marla_Ring_simplify(marla_Ring* ring);
int marla_Ring_isFull(marla_Ring* ring);
int marla_Ring_isEmpty(marla_Ring* ring);
void marla_Ring_clear(marla_Ring* ring);
void marla_Ring_dump(marla_Ring* ring, const char* name);

// client.c
enum marla_RequestReadStage {
marla_CLIENT_REQUEST_READ_FRESH,
marla_BACKEND_REQUEST_FRESH,
marla_CLIENT_REQUEST_READING_METHOD,
marla_CLIENT_REQUEST_PAST_METHOD,
marla_CLIENT_REQUEST_READING_REQUEST_TARGET,
marla_CLIENT_REQUEST_PAST_REQUEST_TARGET,
marla_CLIENT_REQUEST_READING_VERSION,
marla_BACKEND_REQUEST_READING_RESPONSE_LINE,
marla_BACKEND_REQUEST_READING_HEADERS,
marla_CLIENT_REQUEST_READING_FIELD,
marla_CLIENT_REQUEST_AWAITING_CONTINUE_WRITE,
marla_CLIENT_REQUEST_AWAITING_UPGRADE_WRITE,
marla_CLIENT_REQUEST_READING_REQUEST_BODY,
marla_CLIENT_REQUEST_WEBSOCKET,
marla_BACKEND_REQUEST_READING_RESPONSE_BODY,
marla_CLIENT_REQUEST_READING_CHUNK_SIZE,
marla_CLIENT_REQUEST_READING_CHUNK_BODY,
marla_BACKEND_REQUEST_READING_CHUNK_SIZE,
marla_BACKEND_REQUEST_READING_CHUNK_BODY,
marla_BACKEND_REQUEST_AFTER_RESPONSE,
marla_BACKEND_REQUEST_READING_RESPONSE_TRAILERS,
marla_CLIENT_REQUEST_DONE_READING,
marla_BACKEND_REQUEST_DONE_READING
};

enum marla_RequestWriteStage {
marla_CLIENT_REQUEST_WRITE_AWAITING_ACCEPT,
marla_BACKEND_REQUEST_WRITING_REQUEST_LINE,
marla_BACKEND_REQUEST_WRITING_HEADERS,
marla_CLIENT_REQUEST_WRITING_UPGRADE,
marla_CLIENT_REQUEST_WRITING_CONTINUE,
marla_BACKEND_REQUEST_WRITING_REQUEST_BODY,
marla_CLIENT_REQUEST_WRITING_RESPONSE,
marla_CLIENT_REQUEST_WRITING_WEBSOCKET_RESPONSE,
marla_CLIENT_REQUEST_AFTER_RESPONSE,
marla_CLIENT_REQUEST_WRITING_TRAILERS,
marla_BACKEND_REQUEST_DONE_WRITING,
marla_CLIENT_REQUEST_DONE_WRITING
};

const char* marla_nameRequestReadStage(enum marla_RequestReadStage stage);
const char* marla_nameRequestWriteStage(enum marla_RequestWriteStage stage);

#define MIN_METHOD_LENGTH 3
#define MAX_METHOD_LENGTH 7
#define MAX_FIELD_NAME_LENGTH 64
#define MAX_FIELD_VALUE_LENGTH 510
#define MAX_RESPONSE_LINE_LENGTH 510
#define MAX_WEBSOCKET_NONCE_LENGTH 255
#define MAX_URI_LENGTH 255
#define marla_MAX_CHUNK_SIZE 0xFFFFFFFF
#define marla_MAX_CHUNK_SIZE_LINE 10
#define MAX_WEBSOCKET_CONTROL_PAYLOAD 125
#define marla_MESSAGE_IS_CHUNKED -1
#define marla_MESSAGE_LENGTH_UNKNOWN -2
#define marla_MESSAGE_USES_CLOSE -3

// chunks

enum marla_ChunkResponseStage {
marla_CHUNK_RESPONSE_GENERATE = 0,
marla_CHUNK_RESPONSE_HEADER = 1,
marla_CHUNK_RESPONSE_RESPOND = 2,
marla_CHUNK_RESPONSE_TRAILER = 3,
marla_CHUNK_RESPONSE_DONE = 4
};
const char* marla_nameChunkResponseStage(enum marla_ChunkResponseStage stage);

enum marla_WriteResult {
marla_WriteResult_LOCKED = -4,
marla_WriteResult_TIMEOUT = -3,
marla_WriteResult_UPSTREAM_CHOKED = -2,
marla_WriteResult_DOWNSTREAM_CHOKED = -1,
marla_WriteResult_CONTINUE = 0,
marla_WriteResult_KILLED = 1,
marla_WriteResult_CLOSED = 2
};
typedef enum marla_WriteResult marla_WriteResult;
const char* marla_nameWriteResult(marla_WriteResult wr);

struct marla_Request;
struct marla_ChunkedPageRequest {
struct marla_Request* req;
marla_WriteResult(*handler)(struct marla_ChunkedPageRequest*);
int handleStage;
int index;
marla_Ring* input;
enum marla_ChunkResponseStage stage;
void* handlerData;
};
typedef struct marla_ChunkedPageRequest marla_ChunkedPageRequest;

struct marla_ChunkedPageRequest* marla_ChunkedPageRequest_new(size_t, struct marla_Request*);
struct marla_Server;
marla_WriteResult marla_writeChunk(struct marla_Server* server, marla_Ring* input, marla_Ring* output);
marla_WriteResult marla_writeChunkTrailer(marla_Ring* output);
void marla_measureChunk(size_t slotLen, int avail, size_t* prefix_len, size_t* availUsed);
void marla_ChunkedPageRequest_free(struct marla_ChunkedPageRequest* cpr);
int marla_ChunkedPageRequest_process(struct marla_ChunkedPageRequest* cpr);
int marla_ChunkedPageRequest_write(marla_ChunkedPageRequest* cpr, unsigned char* in, size_t len);

enum marla_BackendResponderStage {
marla_BackendResponderStage_STARTED,
marla_BackendResponderStage_RESPONSE_LINE,
marla_BackendResponderStage_LOCATION_HEADER,
marla_BackendResponderStage_SET_COOKIE_HEADER,
marla_BackendResponderStage_TERMINAL_HEADER,
marla_BackendResponderStage_RESPONSE,
marla_BackendResponderStage_FLUSHING,
marla_BackendResponderStage_DONE,
};

struct marla_BackendResponder {
int id;
struct marla_Request* req;
void(*handler)(struct marla_BackendResponder*);
enum marla_BackendResponderStage handleStage;
int index;
marla_Ring* backendRequestBody;
marla_Ring* backendResponse;
void* handlerData;
};

typedef struct marla_BackendResponder marla_BackendResponder;
int marla_Backend_connect(struct marla_Server* server);
struct marla_BackendResponder* marla_BackendResponder_new(size_t bufSize, struct marla_Request* req);
void marla_BackendResponder_free(marla_BackendResponder* resp);
int marla_BackendResponder_writeRequestBody(marla_BackendResponder* resp, unsigned char* in, size_t len);

enum marla_ClientEvent {
marla_BACKEND_EVENT_NEED_HEADERS,
marla_BACKEND_EVENT_HEADER,
marla_BACKEND_EVENT_MUST_READ,
marla_BACKEND_EVENT_MUST_WRITE,
marla_BACKEND_EVENT_TRAILER,
marla_BACKEND_EVENT_CLIENT_PEER_CLOSED,
marla_BACKEND_EVENT_CLOSING,
marla_BACKEND_EVENT_ACCEPTING_RESPONSE,
marla_EVENT_BACKEND_PEER_CLOSED,
marla_EVENT_HEADER,
marla_EVENT_ACCEPTING_REQUEST,
marla_EVENT_REQUEST_BODY,
marla_BACKEND_EVENT_RESPONSE_BODY,
marla_EVENT_FORM_FIELD,
marla_EVENT_MUST_WRITE,
marla_EVENT_WEBSOCKET_MUST_READ,
marla_EVENT_WEBSOCKET_MUST_WRITE,
marla_EVENT_WEBSOCKET_CLOSING,
marla_EVENT_WEBSOCKET_CLOSE_REASON,
marla_EVENT_DESTROYING,
marla_BACKEND_EVENT_DESTROYING
};
typedef enum marla_ClientEvent marla_ClientEvent;

void marla_chunkedRequestHandler(struct marla_Request* req, enum marla_ClientEvent ev, void* data, int datalen);
void marla_backendHandler(struct marla_Request* req, enum marla_ClientEvent ev, void* in, int len);
void marla_backendClientHandler(struct marla_Request* req, enum marla_ClientEvent ev, void* in, int len);

struct marla_WriteEvent {
enum marla_WriteResult status;
int index;
size_t length;
void* buf;
};
typedef struct marla_WriteEvent marla_WriteEvent;

void marla_WriteEvent_init(marla_WriteEvent* writeEvent, enum marla_WriteResult st);

struct marla_Connection;

struct marla_BackendSource {
int fd;
};
typedef struct marla_BackendSource marla_BackendSource;

struct marla_Request {
struct marla_Request* next_request;
int id;
int statusCode;
int refs;
char statusLine[MAX_FIELD_VALUE_LENGTH + 1];
struct marla_Connection* cxn;
char method[MAX_METHOD_LENGTH + 1];
char host[MAX_FIELD_VALUE_LENGTH + 1];
char uri[MAX_URI_LENGTH + 1];
char error[marla_BUFSIZE];
char contentType[MAX_FIELD_VALUE_LENGTH + 1];
enum marla_RequestReadStage readStage;
enum marla_RequestWriteStage writeStage;
int is_backend;
int expect_upgrade;
int expect_continue;
int connection_indicates_trailer;
int expect_trailer;
int expect_websocket;
int expect_chunked;
int close_after_done;
void(*handler)(struct marla_Request*, enum marla_ClientEvent, void*, int);
void* handlerData;
struct marla_Request* backendPeer;
long int requestLen;
long int responseLen;
long int remainingResponseLen;
long int remainingContentLen;
long int totalContentLen;
long int chunkSize;
int lastReadIndex;
char acceptHeader[MAX_FIELD_VALUE_LENGTH + 1];
char redirectLocation[MAX_FIELD_VALUE_LENGTH + 1];
char cookieHeader[MAX_FIELD_VALUE_LENGTH + 1];
char setCookieHeader[MAX_FIELD_VALUE_LENGTH + 1];
char websocket_nonce[MAX_WEBSOCKET_NONCE_LENGTH + 1];
char websocket_accept[2 * SHA_DIGEST_LENGTH + 1];
unsigned char websocket_frame[7];
int websocket_pingLen;
unsigned char websocket_ping[MAX_WEBSOCKET_CONTROL_PAYLOAD];
char websocket_pongLen;
unsigned char websocket_pong[MAX_WEBSOCKET_CONTROL_PAYLOAD];
unsigned char websocket_closeReason[MAX_WEBSOCKET_CONTROL_PAYLOAD];
char websocket_closeReasonLen;
uint16_t websocket_closeCode;
int websocket_type;
int websocket_fin;
int needWebSocketClose;
int doingPong;
int doingWebSocketClose;
uint64_t websocketFrameWritten;
uint64_t websocketFrameOutLen;
uint64_t websocketFrameRead;
uint64_t websocketFrameLen;
char websocketOutMask[4];
char websocketMask[4];
int websocket_version;
};
typedef struct marla_Request marla_Request;

struct marla_Connection;
struct marla_Server;

marla_Request* marla_Request_new(struct marla_Connection* cxn);
void marla_Request_ref(marla_Request*);
void marla_Request_unref(marla_Request*);
void marla_killRequest(struct marla_Request* req, const char* reason, ...);
void marla_dumpRequest(marla_Request* req);

// connection.c

enum marla_ConnectionStage {
marla_CLIENT_ACCEPTED, /* struct has been created and socket FD has been set */
marla_CLIENT_SECURED, /* SSL has been accepted */
marla_BACKEND_READY, /* Backend ready for requests */
marla_CLIENT_COMPLETE /* Done with connection */
};

const char* marla_nameConnectionStage(enum marla_ConnectionStage);
struct marla_Connection {

// Flags
int shouldDestroy;
int wantsWrite;
int wantsRead;
enum marla_ConnectionStage stage;
int in_read;
int in_write;
int is_backend;

struct timespec lastProcessTime;

int id;
struct marla_Server* server;
struct marla_Connection* prev_connection;
struct marla_Connection* next_connection;

// Requests
marla_Request* current_request;
marla_Request* latest_request;
size_t requests_in_process;

// Buffers
marla_Ring* input;
marla_Ring* output;

// Source
void* source;
int(*readSource)(struct marla_Connection*, void*, size_t);
int(*writeSource)(struct marla_Connection*, void*, size_t);
void(*acceptSource)(struct marla_Connection*);
int(*shutdownSource)(struct marla_Connection*);
void(*destroySource)(struct marla_Connection*);
int(*describeSource)(struct marla_Connection*, char*, size_t);
struct epoll_event poll;
size_t flushed;
};

typedef struct marla_Connection marla_Connection;
marla_Connection* marla_Connection_new(struct marla_Server* server);
void marla_Connection_putbackRead(marla_Connection* cxn, size_t amount);
void marla_Connection_putbackWrite(marla_Connection* cxn, size_t amount);
int marla_Connection_read(marla_Connection* cxn, unsigned char* sink, size_t requested);
int marla_Connection_refill(marla_Connection* cxn, size_t* total);
void marla_Connection_destroy(marla_Connection* cxn);
int marla_Connection_flush(marla_Connection* cxn, int* outnflushed);
int marla_Connection_write(marla_Connection* cxn, const void* source, size_t requested);

typedef struct {
int fd;
SSL_CTX* ctx;
SSL* ssl;
} marla_SSLSource;

typedef struct {
int fd;
} marla_ClearTextSource;
int marla_cleartext_init(marla_Connection* cxn, int fd);
int marla_readDuplex(marla_Connection* cxn, void* sink, size_t len);
int marla_writeDuplex(marla_Connection* cxn, void* source, size_t len);
void marla_putbackDuplexRead(marla_Connection* cxn, int count);

struct marla_DuplexSource {
marla_Ring* input;
int drain_input;
marla_Ring* output;
int drain_output;
int sigpipe;
};
typedef struct marla_DuplexSource marla_DuplexSource;
void marla_Duplex_init(marla_Connection* cxn, size_t input_size, size_t output_size);
void marla_Duplex_drainInput(marla_Connection* cxn);
void marla_Duplex_drainOutput(marla_Connection* cxn);
void marla_Duplex_plugInput(marla_Connection* cxn);
void marla_Duplex_plugOutput(marla_Connection* cxn);
void marla_Duplex_sigpipe(marla_Connection* cxn);

// ssl.c
int marla_SSL_init(marla_Connection* cxn, SSL_CTX* ctx, int fd);

// default_request_handler.c
extern void(*default_request_handler)(struct marla_Request*, enum marla_ClientEvent, void*, int);

// WebSocket
void marla_closeWebSocketRequest(marla_Request* req, uint16_t closeCode, const char* reason, size_t reasonLen);
int marla_writeWebSocket(struct marla_Request* req, unsigned char* data, int dataLen);
int marla_readWebSocket(struct marla_Request* req, unsigned char* data, int dataLen);
void marla_putbackWebSocketRead(struct marla_Request* req, int dataLen);
void marla_putbackWebSocketWrite(struct marla_Request* req, int dataLen);
int marla_writeWebSocketHeader(struct marla_Request* req, unsigned char opcode, uint64_t frameLen);
void marla_default_websocket_handler(struct marla_Request* req, enum marla_ClientEvent ev, void* data, int datalen);

int marla_clientAccept(marla_Connection* cxn);

marla_WriteResult marla_clientRead(marla_Connection* cxn);
marla_WriteResult marla_clientWrite(marla_Connection* cxn);
marla_WriteResult marla_backendRead(marla_Connection* cxn);
marla_WriteResult marla_backendWrite(marla_Connection* cxn);

void marla_Backend_init(marla_Connection* cxn, int fd);
void marla_Backend_enqueue(struct marla_Server* server, marla_Request* req);

// Server

enum marla_ServerModuleEvent {
marla_EVENT_SERVER_MODULE_START,
marla_EVENT_SERVER_MODULE_STOP
};

struct marla_HookEntry {
void(*hookFunc)(struct marla_Request* req, void*);
void* hookData;
struct marla_HookEntry* prevHook;
struct marla_HookEntry* nextHook;
};

struct marla_HookList {
struct marla_HookEntry* firstHook;
struct marla_HookEntry* lastHook;
};

enum marla_ServerHook {
marla_ServerHook_ROUTE = 0,
marla_ServerHook_WEBSOCKET = 1,
marla_ServerHook_MAX = 2
};
typedef enum marla_ServerHook marla_ServerHook;

struct marla_ServerModule;

enum marla_ServerStatus {
marla_SERVER_STOPPED = 0,
marla_SERVER_STARTED = 1,
marla_SERVER_WAITING_FOR_INPUT = 2,
marla_SERVER_WAITING_FOR_LOCK = 3,
marla_SERVER_PROCESSING = 4,
marla_SERVER_DESTROYING = 5,
};

const char* marla_nameServerStatus(enum marla_ServerStatus);

struct marla_Server {

int idleTimeouts;

struct marla_Connection* first_connection;
struct marla_Connection* last_connection;

int using_ssl;
int wantsLogWrite;
int logfd;
marla_Ring* log;
char logbuf[4096];
char logaddress[1024];
char serverport[64];
char backendport[64];
char db_path[1024];
const char* backendPort;
pthread_mutex_t server_mutex;
volatile enum marla_ServerStatus server_status;
volatile int efd;
volatile int sfd;
int backendfd;
marla_Connection* backend;
pthread_t terminal_thread;
pthread_t idle_thread;
volatile int has_terminal;
struct marla_ServerModule* first_module;
struct marla_ServerModule* last_module;
struct marla_HookList hooks[marla_ServerHook_MAX];
void(*undertaker)(marla_Request*);
void* undertakerData;
};

typedef struct marla_Server marla_Server;
void marla_Backend_recover(marla_Server* server);

struct marla_ServerModule {
const char* moduleDef;
void* moduleHandle;
void(*moduleFunc)(struct marla_Server*, enum marla_ServerModuleEvent);
struct marla_ServerModule* nextModule;
struct marla_ServerModule* prevModule;
};
void marla_Server_init(struct marla_Server* server);
void marla_Server_flushLog(struct marla_Server* server);
void marla_Server_free(struct marla_Server* server);
void marla_Server_invokeHook(struct marla_Server* server, enum marla_ServerHook serverHook, struct marla_Request* req);
int marla_Server_removeHook(struct marla_Server* server, enum marla_ServerHook serverHook, void(*hookFunc)(struct marla_Request* req, void*), void* hookData);
void marla_Server_addHook(struct marla_Server* server, enum marla_ServerHook serverHook, void(*hookFunc)(struct marla_Request* req, void*), void* hookData);
const char* marla_nameClientEvent(enum marla_ClientEvent ev);

void marla_Server_log(struct marla_Server* server, const char* output, size_t len);
void marla_logEntercf(marla_Server* server, const char* category, const char* fmt, ...);
void marla_logEnterc(marla_Server* server, const char* category, const char* message);
void marla_logEnter(marla_Server* server, const char* message);
void marla_logLeave(marla_Server* server, const char* message);
void marla_logLeavec(marla_Server* server, const char* category, const char* message);
void marla_logLeavef(marla_Server* server, const char* fmt, ...);
void marla_logReset(marla_Server* server, const char* message);
void marla_logResetc(marla_Server* server, const char* category, const char* message);
void marla_logMessagec(marla_Server* server, const char* category, const char* message);
void marla_logMessage(marla_Server* server, const char* message);
void marla_logMessagecf(marla_Server* server, const char* category, const char* fmt, ...);
void marla_logMessagef(marla_Server* server, const char* fmt, ...);
void marla_logEnterf(marla_Server* server, const char* fmt, ...);
void marla_die(marla_Server* server, const char* fmt, ...);

// http.o
const char* marla_getDefaultStatusLine(int statusCode);

#endif // marla_INCLUDED
