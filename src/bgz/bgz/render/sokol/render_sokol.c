DISABLE_CLANG_WARNINGS()
#define COBJMACROS // Note: in order to use directX C function calls
#define SOKOL_GFX_IMPL
#define SOKOL_LOG_IMPL
#define SOKOL_TRACE_HOOKS
#define SOKOL_D3D11
#define SOKOL_DEBUG
#define SOKOL_ASSERT(condition) Assert(condition)
#include "third_party/sokol/sokol_gfx_2.h"
#include "third_party/sokol/sokol_log_2.h"
RESTORE_CLANG_WARNINGS()

#define D3D11_SAFE_RELEASE(p)       \
   do {                             \
      if ((p) != 0) {               \
         (p)->lpVtbl->Release((p)); \
         (p) = 0;                   \
      }                             \
   } while (0)

#include <windows.h> // OutputDebugStringA, for _R_LogCallback
#include <stdio.h>   // snprintf / fputs, same
#include <dxgi.h>
#include <dxgidebug.h>
#include <d3d11_1.h> // ID3D11Device1 / ID3D11DeviceContext1
#include <dxgi1_2.h> //IDXGIFactory2 / IDXGISwapChain1
#include <dxgi1_3.h> // IDXGISwapChain2: the frame latency waitable object
#include <dxgi1_5.h> // So we can allow tearing DXGI_FEATURE_PRESENT_ALLOW_TEARING if user wants uncapped frame rate (apparently need to set this feature if I want to control vsync on/off from app)

#include "generated/render_sokol.meta.h"

#define AssertHR(hr) Assert(SUCCEEDED(hr))

typedef struct R_Sokol_D3D11State R_Sokol_D3D11State;
struct R_Sokol_D3D11State {
   Arena* arena;

   // TODO: eventually make user configurable
   B32 vsync;
   B32 allow_tearing; // Only allows this if vsync gets turned off for uncapped fps

   // base d3d11 objects
   ID3D11Device*         base_device;
   ID3D11DeviceContext*  base_device_ctx;
   ID3D11Device1*        device;
   ID3D11DeviceContext1* device_ctx;
   IDXGIDevice1*         dxgi_device;
   IDXGIAdapter*         dxgi_adapter;
   IDXGIFactory2*        dxgi_factory;

   // The swap chain, its backbuffer and its waitable are each window's own:
   // see R_Sokol_Window.
};

// Forward declare
typedef struct R_Sokol_Tex2D  R_Sokol_Tex2D;
typedef struct R_Sokol_Buffer R_Sokol_Buffer;

typedef struct R_Sokol_State R_Sokol_State;
struct R_Sokol_State {
   Arena* arena;
   U32    platform_window_width;
   U32    platform_window_height;

   sg_pass_action clear_dark_grey;

   // Present pass
   sg_pipeline present_pipeline; // for final blit to screen at end of frame

   // Test pass
   sg_pipeline test_pipeline;
   R_Handle    test_rect_buffer;

   // UI pass
   sg_pipeline ui_pipeline;
   R_Handle    ui_rect_buffer;

   // Samplers
   sg_sampler linear_sampler;
   sg_sampler nearest_sampler;

   // Will default to this if user does not create a text atlas right away
   R_Handle dummy_texture;

   // free lists
   R_Sokol_Tex2D*  first_available_tex2d;
   R_Sokol_Buffer* first_available_buffer;
   R_Sokol_Tex2D*  first_pending_destroy_tex2d;
   R_Sokol_Buffer* first_pending_destory_buffer;

   // Every tex2d node ever taken from the arena, threaded through all_next and
   // never unlinked. The lists above only hold nodes in a particular state -
   // a texture that is alive and in use is on none of them - so this is the
   // only way to reach them all at shutdown. See Core_R_Shutdown.
   //
   // Only textures need this. A buffer is entirely sokol's, but a texture is
   // an ID3D11Texture2D we create ourselves and inject, and sokol does not own
   // an injected resource any more than it owns an injected device.
   R_Sokol_Tex2D* first_tex2d;
};

// One per equipped window (Core_R_WindowEquip), so a program can draw to more
// than one. The R_Handle an equip returns points at one of these.
typedef struct R_Sokol_Window R_Sokol_Window;
struct R_Sokol_Window {
   B32 in_use;

   // swapchain, and its frame buffer (what gets displayed)
   IDXGISwapChain1*        d3d11_swapchain;
   ID3D11Texture2D*        framebuffer;
   ID3D11RenderTargetView* framebuffer_rtv;

   // Signalled when the swap chain can take another frame without queueing
   // it behind one not yet shown. Core_R_WaitForFrame waits on it before the
   // app reads input, so the wait for vsync comes before the keys are read
   // rather than after the frame is built. 0 if the system has no
   // IDXGISwapChain2; Present then throttles as it always did.
   HANDLE frame_latency_waitable;

   sg_swapchain sokol_swapchain;

   // rjf: staging buffer
   sg_image stage_image;            // image memory
   sg_view  stage_image_write_view; // description of write
   sg_view  stage_image_read_view;  // description of read

   // rjf: last state
   Vec2S32 last_resolution;
};

typedef struct R_Sokol_Tex2D R_Sokol_Tex2D;
struct R_Sokol_Tex2D {
   R_Sokol_Tex2D*   next;     // free list, or pending destroy list
   R_Sokol_Tex2D*   all_next; // see R_Sokol_State.first_tex2d
   U64              generation;
   R_ResourceKind   kind;
   R_Tex2DFormat    format;
   Vec2S32          size;
   sg_image         sokol_image_handle;
   sg_view          sokol_image_view;
   ID3D11Texture2D* d3d_texture_handle;
};

typedef struct R_Sokol_Buffer R_Sokol_Buffer;
struct R_Sokol_Buffer {
   R_Sokol_Buffer* next;
   sg_buffer       sokol_buffer_handle;
   U64             generation;
   R_ResourceKind  kind;
   U64             size;
};

typedef struct R_Sokol_Uniforms {
   int     screen_resolution[2];
   Vec2F32 texture_t2d_size_px;
} R_Sokol_Uniforms;

// Globals
local_persist R_Sokol_State*      _sokol_state;
local_persist R_Sokol_D3D11State* _d3d11_state;
#define R_SOKOL_WINDOW_CAP 8
local_persist R_Sokol_Window      _windows[R_SOKOL_WINDOW_CAP];
local_persist Arena*              _frame_arena;
local_persist S64                 _frame;

// Frame stats: the frame being drawn, and the last one finished. See R_FrameStats.
local_persist R_FrameStats _frame_stats;
local_persist R_FrameStats _frame_stats_last;
local_persist U64          _frame_begin_us;
local_persist U64          _frame_end_us;

// Messages waiting for Core_R_TakeMessages. See R_MessageList.
#define R_MESSAGE_CAP 256
local_persist Arena*        _message_arena;
local_persist R_MessageList _messages;
#ifdef SOKOL_DEBUG
local_persist ID3D11InfoQueue* _d3d11_info_queue; // the debug layer's messages
static void                    _R_PullD3D11Messages(void);
#endif

local_persist read_only R_Sokol_Tex2D  r_sokol_tex2d_nil = {.next = &r_sokol_tex2d_nil};
local_persist read_only R_Sokol_Buffer r_sokol_buffer_nil = {.next = &r_sokol_buffer_nil};

void _SokolTraceHookCallBack_BeginPass(const sg_pass* pass, void* user_data);
void _SokolTraceHookCallBack_EndPass();
void _SokolTraceHookCallBack_ApplyPipeline(sg_pipeline pip, void* user_data);

// Sokol's own diagnostics. Each is kept for Core_R_TakeMessages, which is how it
// reaches the Log, and also written straight out to the debugger and stderr:
// a panic aborts the process a few lines below, before anything can take it.
//
// OutputDebugStringA is what makes the file(line) prefix worth building: the
// debugger's output window turns it into a line you can click to jump to.
// This is the D3D11 backend, so Windows-only formatting is not a limitation.
// Gives a D3D11 object a name so it identifies itself in a live object report
// instead of showing up as a bare address. sokol does this for its own
// resources from the .label in a desc; these are the ones we create directly.
//
// Two call sites: initial creation and the resize path, which throws both the
// backbuffer and its view away and makes new ones.
#if BUILD_DEBUG
static void _R_SetDebugName(ID3D11DeviceChild* child, String8 name) {
   local_persist const GUID debug_name_guid = {0x429b8c22, 0x9188, 0x4b0c, {0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00}};

   if (child != 0) {
      child->lpVtbl->SetPrivateData(child, &debug_name_guid, (UINT)name.size, name.str);
   }
}
#define Core_R_SetDebugName(child, name_literal) _R_SetDebugName((ID3D11DeviceChild*)(child), str8_lit(name_literal))
#else
#define Core_R_SetDebugName(child, name_literal)
#endif

// Adds a message to _messages, or counts it again if the same one is already waiting.
static void _R_PushMessage(R_MessageLevel level, String8 tag, U32 id, String8 text, String8 file, U32 line) {
   if (_message_arena == 0) {
      _message_arena = arena_alloc();
   }

   for (R_Message* m = _messages.first; m != 0; m = m->next) {
      if (m->id == id && m->line == line && str8_match(m->tag, tag, 0) && str8_match(m->text, text, 0)) {
         m->count += 1;
         return;
      }
   }

   if (_messages.count >= R_MESSAGE_CAP) {
      _messages.dropped_count += 1;
      return;
   }

   R_Message* m   = push_array(_message_arena, R_Message, 1);
   m->level       = level;
   m->tag         = push_str8_copy(_message_arena, tag);
   m->id          = id;
   m->text        = push_str8_copy(_message_arena, text);
   m->file        = push_str8_copy(_message_arena, file);
   m->line        = line;
   m->frame_index = (U64)_frame;
   m->count       = 1;
   SLLQueuePush(_messages.first, _messages.last, m);
   _messages.count += 1;
}

void _R_LogCallback(const char* tag, uint32_t log_level, uint32_t log_item, const char* message, uint32_t line_nr, const char* filename, void* user_data) {
   (void)user_data;

   // sokol levels: 0=panic, 1=error, 2=warning, 3=info
   const char* lvl_str = (log_level == 0)   ? "panic"
                         : (log_level == 1) ? "error"
                         : (log_level == 2) ? "warning"
                                            : "info";

   R_MessageLevel level = log_level <= R_MessageLevel_Info ? (R_MessageLevel)log_level : R_MessageLevel_Info;
   _R_PushMessage(level, str8_lit("sokol"), log_item, str8_cstring(message ? (char*)message : ""),
                  str8_cstring(filename ? (char*)filename : ""), line_nr);

   char line_buf[1024];

   if (filename && filename[0]) {
      // MSVC clickable format: path\file.c(123):
      snprintf(line_buf, sizeof(line_buf),
               "%s(%u): [%s][%s][id:%u]\n\t%s\n",
               filename, (unsigned)line_nr,
               tag ? tag : "sokol", lvl_str, (unsigned)log_item,
               message ? message : "");
   }
   else {
      // No filename, so still carry line and item to have something to go on.
      snprintf(line_buf, sizeof(line_buf),
               "[%s][%s][id:%u][line:%u]\n\t%s\n",
               tag ? tag : "sokol", lvl_str, (unsigned)log_item, (unsigned)line_nr,
               message ? message : "");
   }

   OutputDebugStringA(line_buf);

   // stderr rather than stdout, so renderer complaints do not interleave with
   // the frame log. Flushed because the next two lines may end the process.
   fputs(line_buf, stderr);
   fflush(stderr);

   if (log_level == 0) {
      Trap();
      abort_self(1);
   }
}

R_Sokol_Tex2D* Core_R_Sokol_Tex2DFromHandle(R_Handle handle) {
   R_Sokol_Tex2D* texture = (R_Sokol_Tex2D*)handle.u64[0];
   if (texture == 0) {
      texture = &r_sokol_tex2d_nil;
   }
   return texture;
}

R_Handle Core_R_Sokol_HandleFromTex2D(R_Sokol_Tex2D* texture) {
   R_Handle handle = {0};
   handle.u64[0] = (U64)texture;
   return handle;
}

R_Sokol_Buffer* Core_R_Sokol_BufferFromHandle(R_Handle handle) {
   R_Sokol_Buffer* buffer = (R_Sokol_Buffer*)handle.u64[0];
   if (buffer == 0) {
      buffer = &r_sokol_buffer_nil;
   }
   return buffer;
}

R_Handle Core_R_Sokol_HandleFromBuffer(R_Sokol_Buffer* buffer) {
   R_Handle handle = {0};
   handle.u64[0] = (U64)buffer;
   return handle;
}

// A zero handle is the first window, for callers from before there could be
// more than one.
static R_Sokol_Window* _R_SokolWindowFromHandle(R_Handle handle) {
   R_Sokol_Window* r_window = (R_Sokol_Window*)handle.u64[0];
   if (r_window == 0) {
      r_window = &_windows[0];
   }
   return r_window;
}

void Core_R_Init(CmdLine* cmdline) {
   UnusedVariable(cmdline);
   Arena* arena = arena_alloc();
   _d3d11_state = push_array(arena, R_Sokol_D3D11State, 1);
   _d3d11_state->arena = arena;

   Arena* s_arena = arena_alloc();
   _sokol_state = push_array(s_arena, R_Sokol_State, 1);
   _sokol_state->arena = s_arena;

   _frame_arena = arena_alloc();

   // Deliberately no LogAlloc/LogSelect here. Selecting a log is global (and
   // per thread) with nothing to restore it, so a library doing it silently
   // redirects every log in the program. It only ever looked harmless because
   // this runs before the application selects its own log; swap that order and
   // the app's logging goes quiet for no visible reason.

   _frame = 0;

   HRESULT error = 0;

   // TODO: Turning this off doesn't seem to work in all scenarios (seems to work on desktop but not laptop). Though it might not matter. Might just always leave vsync on
   // and split app updating (logic updates vs rendering updates)
   _d3d11_state->vsync = 1;

   // Create base device
   UINT creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef SOKOL_DEBUG
   creation_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
   D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
   D3D_DRIVER_TYPE   driver_type = D3D_DRIVER_TYPE_HARDWARE;

   error = D3D11CreateDevice(0,
                             driver_type,
                             0,
                             creation_flags,
                             feature_levels, ArrayCount(feature_levels),
                             D3D11_SDK_VERSION,
                             &_d3d11_state->base_device, 0, &_d3d11_state->base_device_ctx);

   if (FAILED(error) && driver_type == D3D_DRIVER_TYPE_HARDWARE) {
      // try with WARP driver as backup solution in case HW device is not available
      error = D3D11CreateDevice(0,
                                D3D_DRIVER_TYPE_WARP,
                                0,
                                creation_flags,
                                feature_levels, ArrayCount(feature_levels),
                                D3D11_SDK_VERSION,
                                &_d3d11_state->base_device, 0, &_d3d11_state->base_device_ctx);
   }

   // The debug flag needs the D3D11 SDK layers ("Graphics Tools" optional feature), which a stock
   // Windows install doesn't have: device creation then fails on both drivers. Retry without it.
   B32 debug_layer = (creation_flags & D3D11_CREATE_DEVICE_DEBUG) != 0;
   if (FAILED(error) && debug_layer) {
      debug_layer = 0;
      creation_flags &= ~D3D11_CREATE_DEVICE_DEBUG;
      error = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, creation_flags,
                                feature_levels, ArrayCount(feature_levels), D3D11_SDK_VERSION,
                                &_d3d11_state->base_device, 0, &_d3d11_state->base_device_ctx);
      if (FAILED(error)) {
         error = D3D11CreateDevice(0, D3D_DRIVER_TYPE_WARP, 0, creation_flags,
                                   feature_levels, ArrayCount(feature_levels), D3D11_SDK_VERSION,
                                   &_d3d11_state->base_device, 0, &_d3d11_state->base_device_ctx);
      }
   }

   if (FAILED(error)) {
      char buffer[256] = {0};
      raddbg_snprintf(buffer, sizeof(buffer), "D3D11 device creation failure (%lx). The process is terminating.", error);
      sh_message(1, str8_lit("Fatal Error"), str8_cstring(buffer));
      abort_self(1);
   }

#ifdef SOKOL_DEBUG
   ID3D11InfoQueue* info = 0;
   // No Assert here: without the SDK layers there is no info queue, and that's fine.
   if (debug_layer) {
      error = _d3d11_state->base_device->lpVtbl->QueryInterface(_d3d11_state->base_device, &IID_ID3D11InfoQueue, (void**)(&info));
   }
   if (debug_layer && SUCCEEDED(error) && info) {
      error = info->lpVtbl->SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
      error = info->lpVtbl->SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
      // error = info->lpVtbl->SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_WARNING, TRUE);

      // Hides certain messages so messages won't clutter debugger output
      D3D11_MESSAGE_ID hide[] = {
          D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,
      };
      D3D11_INFO_QUEUE_FILTER filter = {0};
      filter.DenyList.NumIDs = ArrayCount(hide);
      filter.DenyList.pIDList = hide;
      info->lpVtbl->AddStorageFilterEntries(info, &filter);

      // Kept: Core_R_TakeMessages reads it. Released in Core_R_Shutdown.
      _d3d11_info_queue = info;
   }
#endif

   //- rjf: get main device for direct3d 11.1 features
   error = _d3d11_state->base_device->lpVtbl->QueryInterface(_d3d11_state->base_device, &IID_ID3D11Device1, (void**)(&_d3d11_state->device));
   Assert(SUCCEEDED(error));
   error = _d3d11_state->base_device_ctx->lpVtbl->QueryInterface(_d3d11_state->base_device_ctx, &IID_ID3D11DeviceContext1, (void**)(&_d3d11_state->device_ctx));
   Assert(SUCCEEDED(error));

   //- rjf: get dxgi device/adapter/factory for dxgi 1.2 features
   error = _d3d11_state->device->lpVtbl->QueryInterface(_d3d11_state->device, &IID_IDXGIDevice1, (void**)(&_d3d11_state->dxgi_device));
   Assert(SUCCEEDED(error));
   error = _d3d11_state->dxgi_device->lpVtbl->GetAdapter(_d3d11_state->dxgi_device, &_d3d11_state->dxgi_adapter);
   Assert(SUCCEEDED(error));
   error = _d3d11_state->dxgi_adapter->lpVtbl->GetParent(_d3d11_state->dxgi_adapter, &IID_IDXGIFactory2, (void**)(&_d3d11_state->dxgi_factory));
   Assert(SUCCEEDED(error));

#if 0
	//Surpress dxgi debug messages
	IDXGIInfoQueue* iq = 0;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, &IID_IDXGIInfoQueue, (void**)&iq)) && iq) {
        // Example: suppress all INFO/WARNING from DXGI
        DXGI_INFO_QUEUE_FILTER filter = {0};

        DXGI_INFO_QUEUE_MESSAGE_SEVERITY deny_sev[] = {
            DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO,
            DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING,
        };

        filter.DenyList.NumSeverities = (UINT)_countof(deny_sev);
        filter.DenyList.pSeverityList = deny_sev;

        iq->lpVtbl->AddStorageFilterEntries(iq, DXGI_DEBUG_ALL, &filter);
        iq->lpVtbl->Release(iq);
    }
#endif

   error = _d3d11_state->dxgi_device->lpVtbl->SetMaximumFrameLatency(_d3d11_state->dxgi_device, 1);
   Assert(SUCCEEDED(error));

   IDXGIFactory5* factory5 = 0;
   HRESULT        hr = _d3d11_state->dxgi_factory->lpVtbl->QueryInterface(
       _d3d11_state->dxgi_factory,
       &IID_IDXGIFactory5,
       (void**)&factory5);

   if (SUCCEEDED(hr) && factory5) {
      BOOL allow_tearing = FALSE;

      hr = factory5->lpVtbl->CheckFeatureSupport(factory5,
                                                 DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                 &allow_tearing,
                                                 sizeof(allow_tearing));

      if (SUCCEEDED(hr) && allow_tearing) {
         _d3d11_state->allow_tearing = 1;
      }

      factory5->lpVtbl->Release(factory5);
   }

   { // Init sokol stuff
      sg_setup(&(sg_desc){
          .environment = (sg_environment){
              .defaults = {
                  .color_format = SG_PIXELFORMAT_BGRA8,
                  .sample_count = 1,
                  .depth_format = SG_PIXELFORMAT_NONE},
              .d3d11 = {
                  .device = _d3d11_state->device,
                  .device_context = _d3d11_state->device_ctx,
              },
          },

          .logger.func = _R_LogCallback});

      _sokol_state->clear_dark_grey = (sg_pass_action){
          .colors[0] = {
              .load_action = SG_LOADACTION_CLEAR,
              .clear_value = {0.2f, 0.2f, 0.2f, 1.0f}},
      };

      //
      // UAV slot guard - only matters once we start writing compute shaders.
      //
      // A "UAV" is a slot a shader can WRITE to (compute shaders, read/write
      // storage buffers). In HLSL they are the "register(u0)", "register(u1)"
      // numbers. Slots a shader only READS from are a different thing
      // (register(t0)) and are not affected by any of this.
      //
      // How many write slots exist depends on the graphics card:
      //
      //    feature level 11.1 and up -> 64 slots
      //    feature level 11.0        ->  8 slots
      //
      // We ask for 11.1 first and fall back to 11.0, so on an older card we
      // silently end up with only 8. That is the trap: sokol does NOT check
      // your shaders against the real number. Its own check uses a fixed 32,
      // so a shader using register(u10) passes validation here and then fails
      // or renders garbage down inside D3D, only on the machines with 8 slots.
      //
      // So we check it ourselves. Set the value below to the highest u-register
      // any shader uses, and this fails immediately and clearly on a machine
      // that cannot provide it - instead of shipping a renderer that is broken
      // only on some people's hardware.
      //
      // Currently zero: nothing writes to UAV slots yet.
      //
#define BGZ_MAX_UAV_REGISTER_USED 0

      sg_limits limits = sg_query_limits();

      if (BGZ_MAX_UAV_REGISTER_USED >= limits.d3d11_max_unordered_access_views) {
         char buffer[256] = {0};
         raddbg_snprintf(buffer,
                      sizeof(buffer),
                      "This GPU provides %d shader write slots (UAVs), but the renderer needs %d. "
                      "The process is terminating.",
                      limits.d3d11_max_unordered_access_views,
                      BGZ_MAX_UAV_REGISTER_USED + 1);
         sh_message(1, str8_lit("Fatal Error"), str8_cstring(buffer));
         abort_self(1);
      }
   }

   { // Create hooks for debugging
      sg_trace_hooks hooks;
      MemoryZero(&hooks, sizeof(hooks));
      hooks.user_data = (R_Sokol_State*)_sokol_state;
      hooks.begin_pass = _SokolTraceHookCallBack_BeginPass;
      hooks.apply_pipeline = _SokolTraceHookCallBack_ApplyPipeline;
      hooks.end_pass = _SokolTraceHookCallBack_EndPass;

      sg_install_trace_hooks(&hooks);
   }

   { // Create pipelines
      Temp scratch = scratch_begin(0, 0);

      { // Create ui pipeline
         sg_shader shd = sg_make_shader(&(sg_shader_desc){
             // Vertex stage descriptions
             .attrs = {
                 [0] = {.hlsl_sem_name = "POS", .hlsl_sem_index = 0},   // float4 dst_rect_px
                 [1] = {.hlsl_sem_name = "TEX", .hlsl_sem_index = 0},   // float4 src_rect_px
                 [2] = {.hlsl_sem_name = "COLOR", .hlsl_sem_index = 0}, // float4 color
                 [3] = {.hlsl_sem_name = "CRAD", .hlsl_sem_index = 0},  // float4 corner_radii_px
                 [4] = {.hlsl_sem_name = "STY", .hlsl_sem_index = 0},   // float4 style_params
             },
             .vertex_func.source = (const char*)_sokol_ui_shader_src.str,
             .vertex_func.entry = "vs_main",
             .vertex_func.d3d11_target = "vs_5_0",
             .uniform_blocks = {[0] = (sg_shader_uniform_block){
                                    .stage = SG_SHADERSTAGE_VERTEX,
                                    .size = sizeof(R_Sokol_Uniforms),
                                    .hlsl_register_b_n = 0,
                                }},

             // Fragment stage descriptions
             .fragment_func.source = (const char*)_sokol_ui_shader_src.str,
             .fragment_func.entry = "ps_main",
             .fragment_func.d3d11_target = "ps_5_0",
             .views = {
                 [0] = (sg_shader_view){.texture = {.stage = SG_SHADERSTAGE_FRAGMENT, .hlsl_register_t_n = 0}},
             },

             .samplers = {
                 [0] = (sg_shader_sampler){
                     .stage = SG_SHADERSTAGE_FRAGMENT,
                     .sampler_type = SG_SAMPLERTYPE_FILTERING, // or NONFILTERING
                     .hlsl_register_s_n = 0,                   // s0
                 },
             },
             .texture_sampler_pairs[0] = (sg_shader_texture_sampler_pair){
                 .stage = SG_SHADERSTAGE_FRAGMENT,
                 .view_slot = 0,
                 .sampler_slot = 0,
             },

             // TODO: Sokol doens't seem to handle label names well and they seem to get overriden or cleared. Mabye implement a simple label/sokol_id table if I want to use them in trace hooks functions
             //.label = "ui stage shader"
         });

         _sokol_state->ui_pipeline = sg_make_pipeline(&(sg_pipeline_desc){
             // if the vertex layout doesn't have gaps, don't need to provide strides and offsets
             .layout = {
                 .attrs = {
                     [0].format = SG_VERTEXFORMAT_FLOAT4, // POS  (dst_rect_px)
                     [1].format = SG_VERTEXFORMAT_FLOAT4, // TEX  (src_rect_px)
                     [2].format = SG_VERTEXFORMAT_FLOAT4, // COLOR
                     [3].format = SG_VERTEXFORMAT_FLOAT4, // CRAD (corner_radii_px)
                     [4].format = SG_VERTEXFORMAT_FLOAT4, // STY  (style_params)
                 },
                 .buffers[0] = {
                     .step_func = SG_VERTEXSTEP_PER_INSTANCE,
                     .step_rate = 1,
                     .stride = sizeof(R_Rect2DInst),
                 }},

             .index_type = SG_INDEXTYPE_NONE,
             .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,

             .colors[0] = {.blend = {.enabled = 1, .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA, .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, .op_rgb = SG_BLENDOP_ADD, .src_factor_alpha = SG_BLENDFACTOR_ONE, .dst_factor_alpha = SG_BLENDFACTOR_ZERO, .op_alpha = SG_BLENDOP_ADD}},

             .shader = shd,

             //.label = "ui pipeline",
         });

         // TODO: right now just allocating a fixed size for buffer. Need to be more dynamic if more space is needed. Will assert if too much has been passed in
         _sokol_state->ui_rect_buffer = Core_R_BufferAlloc(R_ResourceKind_Dynamic, MB(30) /*cap*/, 0);
      }

      { // Create test pass pipeline
         sg_shader shd = sg_make_shader(&(sg_shader_desc){
             // Vertex stage descriptions
             .attrs = {
                 [0] = {.hlsl_sem_name = "POS_TL", .hlsl_sem_index = 0},
                 [1] = {.hlsl_sem_name = "POS_BR", .hlsl_sem_index = 0},
                 [2] = {.hlsl_sem_name = "TEX_MIN", .hlsl_sem_index = 0},
                 [3] = {.hlsl_sem_name = "TEX_MAX", .hlsl_sem_index = 0},
                 [4] = {.hlsl_sem_name = "COLOR", .hlsl_sem_index = 0},
             },
             .vertex_func.source = (const char*)_sokol_test_shader_src.str,
             .vertex_func.entry = "vs_main",
             .vertex_func.d3d11_target = "vs_5_0",
             .uniform_blocks = {[0] = (sg_shader_uniform_block){
                                    .stage = SG_SHADERSTAGE_VERTEX,
                                    .size = sizeof(R_Sokol_Uniforms),
                                    .hlsl_register_b_n = 0,
                                }},

             // Fragment stage descriptions
             .fragment_func.source = (const char*)_sokol_test_shader_src.str,
             .fragment_func.entry = "ps_main",
             .fragment_func.d3d11_target = "ps_5_0",
             .views = {
                 [0] = (sg_shader_view){.texture = {.stage = SG_SHADERSTAGE_FRAGMENT, .hlsl_register_t_n = 0}},
             },
             .samplers = {
                 [0] = (sg_shader_sampler){
                     .stage = SG_SHADERSTAGE_FRAGMENT,
                     .sampler_type = SG_SAMPLERTYPE_FILTERING, // or NONFILTERING
                     .hlsl_register_s_n = 0,                   // s0
                 },
             },
             .texture_sampler_pairs[0] = (sg_shader_texture_sampler_pair){
                 .stage = SG_SHADERSTAGE_FRAGMENT,
                 .view_slot = 0,
                 .sampler_slot = 0,
             },

             //.label = "test stage shader"
         });

         //  pipeline object
         _sokol_state->test_pipeline = sg_make_pipeline(&(sg_pipeline_desc){
             // if the vertex layout doesn't have gaps, don't need to provide strides and offsets
             .layout = {
                 .attrs = {
                     [0].format = SG_VERTEXFORMAT_FLOAT2, // POS_TL
                     [1].format = SG_VERTEXFORMAT_FLOAT2, // POS_BR
                     [2].format = SG_VERTEXFORMAT_FLOAT2, // TEX_MIN
                     [3].format = SG_VERTEXFORMAT_FLOAT2, // TEX_MAX
                     [4].format = SG_VERTEXFORMAT_FLOAT4  // COLOR
                 },
                 .buffers[0] = {.step_func = SG_VERTEXSTEP_PER_INSTANCE, .step_rate = 1}},

             .index_type = SG_INDEXTYPE_NONE,
             .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,

             .colors[0] = {.blend = {.enabled = 1, .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA, .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, .op_rgb = SG_BLENDOP_ADD, .src_factor_alpha = SG_BLENDFACTOR_ONE, .dst_factor_alpha = SG_BLENDFACTOR_ZERO, .op_alpha = SG_BLENDOP_ADD}},

             .shader = shd,

             //.label = "test pipeline",
         });

         uint32_t white_pixel = 0xFFFFFFFF;
         _sokol_state->test_rect_buffer = Core_R_BufferAlloc(R_ResourceKind_Dynamic, MB(2) /*cap*/, 0);
         _sokol_state->dummy_texture = Core_R_Tex2DAlloc(R_ResourceKind_Static, v2s32(1, 1), R_Tex2DFormat_RGBA8, &white_pixel);
      }

      { // Create finalize pipeline (writing to framebuffer for display)
         sg_shader shd = sg_make_shader(&(sg_shader_desc){
             .vertex_func.source = (const char*)_sokol_finalized_shader_src.str,
             .vertex_func.entry = "vs_main",
             .vertex_func.d3d11_target = "vs_5_0",

             .fragment_func.source = (const char*)_sokol_finalized_shader_src.str,
             .fragment_func.entry = "ps_main",
             .fragment_func.d3d11_target = "ps_5_0",

             .views = {
                 [0] = (sg_shader_view){
                     .texture = (sg_shader_texture_view){
                         .stage = SG_SHADERSTAGE_FRAGMENT,
                         // IMPORTANT: bind slot for HLSL t0
                         .hlsl_register_t_n = 0,
                     },
                 },
             },

             .samplers = {
                 [0] = (sg_shader_sampler){
                     .stage = SG_SHADERSTAGE_FRAGMENT,
                     .sampler_type = SG_SAMPLERTYPE_FILTERING, // or NONFILTERING
                     .hlsl_register_s_n = 0,                   // s0
                 },
             },

             .texture_sampler_pairs[0] = (sg_shader_texture_sampler_pair){.view_slot = 0, .sampler_slot = 0, .stage = SG_SHADERSTAGE_FRAGMENT},

             //.label = "present stage shader"
         });

         _sokol_state->present_pipeline = sg_make_pipeline(&(sg_pipeline_desc){
             .shader = shd,
             .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
             .index_type = SG_INDEXTYPE_NONE,
             .layout = {0}, // no vertex attributes (SV_VertexID)
             .colors[0].blend.enabled = false,
             //.label = "final present pipeline",
         });
      }

      { // Create sampler states
         _sokol_state->linear_sampler = sg_make_sampler(&(sg_sampler_desc){
             .min_filter = SG_FILTER_LINEAR,
             .mag_filter = SG_FILTER_LINEAR,
             .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
             .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
             //.label = "linear-sampler",
         });

         _sokol_state->nearest_sampler = sg_make_sampler(&(sg_sampler_desc){
             .min_filter = SG_FILTER_NEAREST,
             .mag_filter = SG_FILTER_NEAREST,
             .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
             .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
             //.label = "nearest-sampler",
         });
      }

      scratch_end(scratch);
   }
}

R_Handle Core_R_WindowEquip(WM_Window window) {
   R_Handle        res = {0};
   R_Sokol_Window* r_window = 0;
   for (U64 i = 0; i < R_SOKOL_WINDOW_CAP; i += 1) {
      if (!_windows[i].in_use) {
         r_window = &_windows[i];
         break;
      }
   }
   if (r_window == 0) {
      sh_message(1, str8_lit("Fatal Error"), str8_lit("Too many windows equipped for rendering. The process is terminating."));
      abort_self(1);
   }
   MemoryZeroStruct(r_window);
   r_window->in_use = 1;
   res.u64[0] = (U64)r_window;

   //- rjf: map os window handle -> hwnd
   HWND hwnd = {0};
   {
      W32_WM_Window* w32_layer_window = w32_wm_window_from_handle(window);
      hwnd = w32_layer_window->hwnd;
   }

   //- rjf: create d3d11 swapchain
   DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {0};
   {
      swapchain_desc.Width = 0;  // NOTE(rjf): use window width
      swapchain_desc.Height = 0; // NOTE(rjf): use window height
      swapchain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      swapchain_desc.Stereo = FALSE;
      swapchain_desc.SampleDesc.Count = 1;
      swapchain_desc.SampleDesc.Quality = 0;
      swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
      swapchain_desc.BufferCount = 2;
      swapchain_desc.Scaling = DXGI_SCALING_NONE;
      swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
      swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
      swapchain_desc.Flags = (_d3d11_state->allow_tearing) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
      swapchain_desc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
   }

   HRESULT error = _d3d11_state->dxgi_factory->lpVtbl->CreateSwapChainForHwnd(_d3d11_state->dxgi_factory, (IUnknown*)_d3d11_state->device, hwnd, &swapchain_desc, 0, 0, &r_window->d3d11_swapchain);
   if (FAILED(error)) {
      char buffer[256] = {0};
      raddbg_snprintf(buffer, sizeof(buffer), "DXGI swap chain creation failure (%lx). The process is terminating.", error);
      sh_message(1, str8_lit("Fatal Error"), str8_cstring(buffer));
      abort_self(1);
   }

   // One frame in flight, and the handle Core_R_WaitForFrame waits on. With the
   // waitable flag the swap chain's own latency is what counts, not the
   // device's SetMaximumFrameLatency above.
   {
      IDXGISwapChain2* swapchain2 = 0;
      HRESULT          qi = r_window->d3d11_swapchain->lpVtbl->QueryInterface(r_window->d3d11_swapchain, &IID_IDXGISwapChain2, (void**)&swapchain2);

      if (SUCCEEDED(qi) && swapchain2 != 0) {
         swapchain2->lpVtbl->SetMaximumFrameLatency(swapchain2, 1);
         r_window->frame_latency_waitable = swapchain2->lpVtbl->GetFrameLatencyWaitableObject(swapchain2);
         swapchain2->lpVtbl->Release(swapchain2);
      }
   }

   // disables dxgi's default alt + enter fullscreen toggle.
   /*
           Why engines do this (almost always):

           Modern engines want explicit control over:
           Borderless fullscreen vs exclusive fullscreen
           Multi-window setups
           DPI scaling / resizing logic
           Custom hotkeys
           Swap-chain recreation timing

           Letting DXGI auto-toggle fullscreen can:

           Bypass your resize logic
           Break UI scaling
           Conflict with multi-monitor setups
           Cause device-loss edge cases

           So disabling it is considered best practice unless you’re making a very simple sample.
   */
   _d3d11_state->dxgi_factory->lpVtbl->MakeWindowAssociation(_d3d11_state->dxgi_factory, hwnd, DXGI_MWA_NO_ALT_ENTER);

   //- rjf: create framebuffer & view
   error = r_window->d3d11_swapchain->lpVtbl->GetBuffer(r_window->d3d11_swapchain, 0, &IID_ID3D11Texture2D, (void**)&r_window->framebuffer);
   Assert(SUCCEEDED(error) && r_window->framebuffer);
   error = _d3d11_state->device->lpVtbl->CreateRenderTargetView(_d3d11_state->device, (ID3D11Resource*)r_window->framebuffer, NULL, &r_window->framebuffer_rtv);
   Assert(SUCCEEDED(error) && r_window->framebuffer_rtv);

   Core_R_SetDebugName(r_window->framebuffer, "bgz swapchain backbuffer");
   Core_R_SetDebugName(r_window->framebuffer_rtv, "bgz swapchain backbuffer rtv");

   // Get width/height of framebuffer
   D3D11_TEXTURE2D_DESC bb_desc = {0};
   r_window->framebuffer->lpVtbl->GetDesc(r_window->framebuffer, &bb_desc);
   UINT frame_buffer_width = bb_desc.Width;
   UINT frame_buffer_height = bb_desc.Height;

   // Create sokol swapchain
   r_window->sokol_swapchain = (sg_swapchain){
       .width = frame_buffer_width,
       .height = frame_buffer_height,
       .sample_count = swapchain_desc.SampleDesc.Count,
       .color_format = SG_PIXELFORMAT_BGRA8,
       .d3d11 = {
           .resolve_view = 0,
           .render_view = r_window->framebuffer_rtv,
       }};

   return res;
}

// Lets go of what Core_R_WindowEquip made for the window: its stage image, its
// backbuffer and its swap chain. Before the OS window itself is closed, and
// between frames.
static void _R_SokolWindowRelease(R_Sokol_Window* r_window) {
   if (!r_window->in_use) {
      return;
   }

   if (r_window->stage_image_write_view.id) {
      sg_destroy_view(r_window->stage_image_write_view);
   }
   if (r_window->stage_image_read_view.id) {
      sg_destroy_view(r_window->stage_image_read_view);
   }
   if (r_window->stage_image.id) {
      sg_destroy_image(r_window->stage_image);
   }

   // Nothing may still have the backbuffer bound when it goes.
   ID3D11DeviceContext1* ctx = _d3d11_state->device_ctx;
   ctx->lpVtbl->OMSetRenderTargets(ctx, 0, 0, 0);
   ctx->lpVtbl->ClearState(ctx);
   ctx->lpVtbl->Flush(ctx);
   sg_reset_state_cache();

   // The view goes before the texture it views.
   D3D11_SAFE_RELEASE(r_window->framebuffer_rtv);
   D3D11_SAFE_RELEASE(r_window->framebuffer);
   if (r_window->frame_latency_waitable != 0) {
      CloseHandle(r_window->frame_latency_waitable);
      r_window->frame_latency_waitable = 0;
   }
   D3D11_SAFE_RELEASE(r_window->d3d11_swapchain);

   MemoryZeroStruct(r_window);
}

void Core_R_WindowUnequip(WM_Window window, R_Handle window_equip) {
   UnusedVariable(window);
   if (window_equip.u64[0] == 0) {
      return;
   }
   _R_SokolWindowRelease(_R_SokolWindowFromHandle(window_equip));
}

sg_pixel_format _R_Tex2DFormatToSokolPixelFormat(R_Tex2DFormat fmt) {
   switch (fmt) {
   case R_Tex2DFormat_R8:
      return SG_PIXELFORMAT_R8;
   case R_Tex2DFormat_RG8:
      return SG_PIXELFORMAT_RG8;
   case R_Tex2DFormat_RGBA8:
      return SG_PIXELFORMAT_RGBA8;
   case R_Tex2DFormat_BGRA8:
      return SG_PIXELFORMAT_BGRA8;

   case R_Tex2DFormat_R16:
      return SG_PIXELFORMAT_R16; // UNORM
   case R_Tex2DFormat_RGBA16:
      return SG_PIXELFORMAT_RGBA16; // UNORM

   case R_Tex2DFormat_R32:
      return SG_PIXELFORMAT_R32F; // float
   case R_Tex2DFormat_RG32:
      return SG_PIXELFORMAT_RG32F; // float
   case R_Tex2DFormat_RGBA32:
      return SG_PIXELFORMAT_RGBA32F; // float

   default:
      return SG_PIXELFORMAT_RGBA8;
   }
}

R_Handle Core_R_Tex2DAlloc(R_ResourceKind kind, Vec2S32 size, R_Tex2DFormat format, void* data) {
   // Allocate texture
   R_Sokol_Tex2D* texture = 0;
   texture = _sokol_state->first_available_tex2d;
   if (texture == 0) {
      texture = push_array(_sokol_state->arena, R_Sokol_Tex2D, 1);

      // New node, so put it on the registry. Reused nodes are already there.
      texture->all_next = _sokol_state->first_tex2d;
      _sokol_state->first_tex2d = texture;
   }
   else {
      U64            gen = texture->generation;
      R_Sokol_Tex2D* all_next = texture->all_next;
      SLLStackPop(_sokol_state->first_available_tex2d);
      MemoryZeroStruct(texture);
      texture->generation = gen;
      texture->all_next = all_next;
   }
   texture->generation += 1;

   // format mapping: format -> sokol format for sokol image, format -> d3d format for actual d3d texture
   sg_pixel_format sokol_format = _R_Tex2DFormatToSokolPixelFormat(format);
   DXGI_FORMAT     dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
   switch (format) {
   default: {
   } break;
   case R_Tex2DFormat_R8: {
      dxgi_format = DXGI_FORMAT_R8_UNORM;
   } break;
   case R_Tex2DFormat_RG8: {
      dxgi_format = DXGI_FORMAT_R8G8_UNORM;
   } break;
   case R_Tex2DFormat_RGBA8: {
      dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
   } break;
   case R_Tex2DFormat_BGRA8: {
      dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
   } break;
   case R_Tex2DFormat_R16: {
      dxgi_format = DXGI_FORMAT_R16_UNORM;
   } break;
   case R_Tex2DFormat_RGBA16: {
      dxgi_format = DXGI_FORMAT_R16G16B16A16_UNORM;
   } break;
   case R_Tex2DFormat_R32: {
      dxgi_format = DXGI_FORMAT_R32_FLOAT;
   } break;
   case R_Tex2DFormat_RG32: {
      dxgi_format = DXGI_FORMAT_R32G32_FLOAT;
   } break;
   case R_Tex2DFormat_RGBA32: {
      dxgi_format = DXGI_FORMAT_R32G32B32A32_FLOAT;
   } break;
   }

   // d3d usage policy
   D3D11_USAGE d3d11_usage = D3D11_USAGE_DEFAULT;
   UINT        cpu_access_flags = 0;
   switch (kind) {
   case R_ResourceKind_Static: {
      d3d11_usage = D3D11_USAGE_IMMUTABLE;
      cpu_access_flags = 0;
   } break;

   case R_ResourceKind_Dynamic: {
      d3d11_usage = D3D11_USAGE_DEFAULT;
      cpu_access_flags = 0;
   } break;

   case R_ResourceKind_Stream: {
      d3d11_usage = D3D11_USAGE_DYNAMIC;
      cpu_access_flags = D3D11_CPU_ACCESS_WRITE;
   } break;

   default: {
      InvalidPath;
   }
   }
   if (kind == R_ResourceKind_Static) {
      Assert(data != 0); // Static texture must have inital data provided
   }

   // prep initial data, if passed
   D3D11_SUBRESOURCE_DATA  initial_data_ = {0};
   D3D11_SUBRESOURCE_DATA* initial_data = 0;
   if (data != 0) {
      sg_pixelformat_info info = sg_query_pixelformat(sokol_format);
      Assert(!info.compressed && info.bytes_per_pixel > 0);
      initial_data = &initial_data_;
      initial_data->pSysMem = data;
      initial_data->SysMemPitch = info.bytes_per_pixel * size.x;
   }

   // Create native d3d11 texture - need to do this since I want to call updatesubresource in d3d and sokol currently doesn't have native way to update sub rects of a texture
   D3D11_TEXTURE2D_DESC d3d11_texture_desc = {0};
   d3d11_texture_desc.Width = size.x;
   d3d11_texture_desc.Height = size.y;
   d3d11_texture_desc.MipLevels = 1;
   d3d11_texture_desc.ArraySize = 1;
   d3d11_texture_desc.Format = dxgi_format;
   d3d11_texture_desc.SampleDesc.Count = 1;
   d3d11_texture_desc.Usage = d3d11_usage;
   d3d11_texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
   d3d11_texture_desc.CPUAccessFlags = cpu_access_flags;
   HRESULT hr = _d3d11_state->device->lpVtbl->CreateTexture2D(_d3d11_state->device, &d3d11_texture_desc, initial_data, &texture->d3d_texture_handle);
   AssertHR(hr);

   // Create sokol texture wrapper
   sg_image_desc sokol_tex_desc;
   MemoryZero(&sokol_tex_desc, sizeof(sokol_tex_desc));
   sokol_tex_desc = sg_query_image_defaults(&sokol_tex_desc);
   sokol_tex_desc.type = SG_IMAGETYPE_2D;
   sokol_tex_desc.width = size.x;
   sokol_tex_desc.height = size.y;
   sokol_tex_desc.num_mipmaps = 1;
   sokol_tex_desc.pixel_format = sokol_format;
   sokol_tex_desc.usage = (sg_image_usage){.immutable = true}; // treated as externally updated
   sokol_tex_desc.d3d11_texture = texture->d3d_texture_handle; // add created d3d texture
   texture->sokol_image_handle = sg_make_image(&sokol_tex_desc);

   // create view to new texture
   texture->sokol_image_view = sg_make_view(&(sg_view_desc){
       .texture = {
           .image = texture->sokol_image_handle,
           .mip_levels = {.base = 0, .count = 1},
           .slices = {.base = 0, .count = 1},
       },
       //.label = "tex2d view", // optional
   });

   // fill basics
   texture->kind = kind;
   texture->size = size;
   texture->format = format;

   R_Handle res = Core_R_Sokol_HandleFromTex2D(texture);
   return res;
}

void Core_R_Tex2DRelease(R_Handle handle) {
   R_Sokol_Tex2D* texture = Core_R_Sokol_Tex2DFromHandle(handle);
   if (texture != &r_sokol_tex2d_nil) {
      SLLStackPush(_sokol_state->first_pending_destroy_tex2d, texture);
   }
}

void Core_R_FillTex2DRegion(R_Handle r_tex_handle, Rng2S32 subrect, void* data) {
   Assert((subrect.x1 - subrect.x0) > 0 && (subrect.y1 - subrect.y0) > 0);

   R_Sokol_Tex2D* texture = Core_R_Sokol_Tex2DFromHandle(r_tex_handle);
   Assert(texture->kind == R_ResourceKind_Dynamic); // only dynamic textures can update regions

   // Query image and set destination region
   sg_image_desc sokol_img_desc = sg_query_image_desc(texture->sokol_image_handle);
   U64           bytes_per_pixel = sg_query_pixelformat(sokol_img_desc.pixel_format).bytes_per_pixel;
   Assert(bytes_per_pixel > 0);
   Vec2S32   dim = v2s32(subrect.x1 - subrect.x0, subrect.y1 - subrect.y0);
   D3D11_BOX dst_box = {
       (UINT)subrect.x0,
       (UINT)subrect.y0,
       0,
       (UINT)subrect.x1,
       (UINT)subrect.y1,
       1,
   };

   // Update d3d11 texture region
   _d3d11_state->device_ctx->lpVtbl->UpdateSubresource(_d3d11_state->device_ctx, (ID3D11Resource*)texture->d3d_texture_handle, 0, &dst_box, data, dim.x * bytes_per_pixel, 0);
}

R_Handle Core_R_BufferAlloc(R_ResourceKind kind, U64 size, void* data) {
   Assert(size);

   // Allocate buffer for tracking
   R_Sokol_Buffer* buffer = 0;
   buffer = _sokol_state->first_available_buffer;
   if (buffer == 0) {
      buffer = push_array(_sokol_state->arena, R_Sokol_Buffer, 1);
   }
   else {
      U64 gen = buffer->generation;
      SLLStackPop(_sokol_state->first_available_buffer);
      MemoryZeroStruct(buffer);
      buffer->generation = gen;
   }
   buffer->generation += 1;

   // Allocate and init sokol buffer
   sg_buffer buf = sg_alloc_buffer();
   {
      sg_buffer_desc desc = {0};
      desc.usage.vertex_buffer = true;

      switch (kind) {
      case R_ResourceKind_Static: {
         Assert(data);
         desc.usage.immutable = true;
         desc.data = (sg_range){.ptr = data, .size = (size_t)size};
      } break;

      case R_ResourceKind_Dynamic: {
         // If you update this buffer frequently (every frame), consider STREAM.
         desc.usage.dynamic_update = true;
         desc.size = (size_t)size; // capacity
         if (data) {
            desc.data = (sg_range){.ptr = data, .size = (size_t)size};
         }
      } break;

      case R_ResourceKind_Stream: {
         desc.usage.stream_update = true;
         desc.size = (size_t)size; // capacity
         if (data) {
            desc.data = (sg_range){.ptr = data, .size = (size_t)size};
         }
      } break;

      default:
         InvalidPath;
      }

      sg_init_buffer(buf, &desc);
   }

   buffer->kind = kind;
   buffer->size = size;
   buffer->sokol_buffer_handle = buf;

   R_Handle res = Core_R_Sokol_HandleFromBuffer(buffer);
   return res;
}

void Core_R_BufferRelease(R_Handle handle) {
   R_Sokol_Buffer* buffer = Core_R_Sokol_BufferFromHandle(handle);

   if (buffer != &r_sokol_buffer_nil) {
      SLLStackPush(_sokol_state->first_pending_destory_buffer, buffer);
   }
}

void Core_R_BeginFrame(void) {
   // no-op
}

// Blocks until the swap chain can take a frame that will be shown at the next
// vblank: called at the top of the app's loop, before it reads input, so what
// was typed is read as late as possible and drawn in the very next frame.
// Without it Present does the waiting, after the frame is built, and every
// key waits one more frame. The timeout keeps a minimized or occluded window,
// whose frames are never shown, from stopping the loop.
//
// With more than one window, every one of their swap chains: each is presented
// once a frame, and one not waited on would make its Present do the waiting.
void Core_R_WaitForFrame(void) {
   if (_d3d11_state == 0) {
      return;
   }
   HANDLE waitables[R_SOKOL_WINDOW_CAP];
   DWORD  waitable_count = 0;
   for (U64 i = 0; i < R_SOKOL_WINDOW_CAP; i += 1) {
      if (_windows[i].in_use && _windows[i].frame_latency_waitable != 0) {
         waitables[waitable_count] = _windows[i].frame_latency_waitable;
         waitable_count += 1;
      }
   }
   if (waitable_count != 0) {
      WaitForMultipleObjectsEx(waitable_count, waitables, TRUE, 100, TRUE);
   }
}

void Core_R_EndFrame(void) {
   for (R_Sokol_Tex2D *tex = _sokol_state->first_pending_destroy_tex2d, *next = 0; tex != 0; tex = next) {
      next = tex->next;

      if (tex->sokol_image_handle.id) {
         sg_destroy_view(tex->sokol_image_view);
         sg_destroy_image(tex->sokol_image_handle);
      }
      if (tex->d3d_texture_handle) {
         tex->d3d_texture_handle->lpVtbl->Release(tex->d3d_texture_handle);
      }
      tex->d3d_texture_handle = 0;
      tex->sokol_image_handle.id = 0;
      tex->sokol_image_view.id = 0;
      tex->generation += 1;
      SLLStackPush(_sokol_state->first_available_tex2d, tex);
   }

   for (R_Sokol_Buffer *buf = _sokol_state->first_pending_destory_buffer, *next = 0; buf != 0; buf = next) {
      next = buf->next;
      if (buf->sokol_buffer_handle.id != 0) {
         sg_destroy_buffer(buf->sokol_buffer_handle);
      }
      buf->generation += 1;
      SLLStackPush(_sokol_state->first_available_buffer, buf);
   }

   _sokol_state->first_pending_destroy_tex2d = 0;
   _sokol_state->first_pending_destory_buffer = 0;
}

// This is for setting up staging buffers. Will only allocate new stageing buffer
// upon window resizing or intial loop. Creating stage buffers also helps with effects
// and compositing.
void Core_R_WindowBeginFrame(WM_Window window, R_Handle window_equip) {
   R_Sokol_Window* r_window = _R_SokolWindowFromHandle(window_equip);

   // No LogScopeBegin here, and no matching End in Core_R_WindowEndFrame.
   //
   // That pair used to bracket the entire frame body, on whatever log the
   // application happened to have selected - and Core_R_WindowEndFrame discarded
   // the result. So everything the app logged between the two calls, which is
   // the whole frame, landed in the renderer's scope and was thrown away.
   //
   // Whatever the renderer logs now goes into the app's own scope and gets
   // drained wherever the app drains it, which is what you want.

   //- rjf: get resolution
   Rng2F32 client_rect = wm_client_rect_from_window(window);
   Vec2S32 screen_res = {{(S32)(client_rect.x1 - client_rect.x0), (S32)(client_rect.y1 - client_rect.y0)}};

   MemoryZeroStruct(&_frame_stats);
   _frame_stats.frame_index = (U64)_frame;
   _frame_stats.window_size = screen_res;
   _frame_begin_us = now_time_us();

   //- rjf: check for resolution change and if so resize
   B32 resize_done = 0;
   if (r_window->last_resolution.x != screen_res.x || r_window->last_resolution.y != screen_res.y) {
      resize_done = 1;
      r_window->last_resolution = screen_res;

      // rjf: release screen-sized render target resources, if there
      if (r_window->stage_image_write_view.id)
         sg_destroy_view(r_window->stage_image_write_view);
      if (r_window->stage_image_read_view.id)
         sg_destroy_view(r_window->stage_image_read_view);
      if (r_window->stage_image.id)
         sg_destroy_image(r_window->stage_image);

      // unbind everything (important before ResizeBuffers)
      ID3D11DeviceContext1* ctx = _d3d11_state->device_ctx;
      ctx->lpVtbl->OMSetRenderTargets(ctx, 0, 0, 0);
      ctx->lpVtbl->ClearState(ctx);
      ctx->lpVtbl->Flush(ctx);

      // rjf: resize d3d11 swapchain & main framebuffer
      r_window->framebuffer_rtv->lpVtbl->Release(r_window->framebuffer_rtv);
      r_window->framebuffer->lpVtbl->Release(r_window->framebuffer);
      // The same flags the swap chain was made with: ResizeBuffers fails if the
      // waitable flag is added or dropped.
      UINT resize_flags = (_d3d11_state->allow_tearing) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
      resize_flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
      HRESULT hr = r_window->d3d11_swapchain->lpVtbl->ResizeBuffers(r_window->d3d11_swapchain, 0, 0, 0, DXGI_FORMAT_UNKNOWN, resize_flags);
      AssertHR(hr);
      hr = r_window->d3d11_swapchain->lpVtbl->GetBuffer(r_window->d3d11_swapchain, 0, &IID_ID3D11Texture2D, (void**)(&r_window->framebuffer));
      AssertHR(hr);
      D3D11_RENDER_TARGET_VIEW_DESC framebuffer_rtv_desc = {0};
      framebuffer_rtv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      framebuffer_rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      hr = _d3d11_state->device->lpVtbl->CreateRenderTargetView(_d3d11_state->device, (ID3D11Resource*)r_window->framebuffer, &framebuffer_rtv_desc, &r_window->framebuffer_rtv);
      AssertHR(hr);

      // These are new objects, so the names from creation do not carry over.
      // This branch also runs on the very first frame, when the stored
      // resolution is still zero - which is why the originals never survive.
      Core_R_SetDebugName(r_window->framebuffer, "bgz swapchain backbuffer");
      Core_R_SetDebugName(r_window->framebuffer_rtv, "bgz swapchain backbuffer rtv");

      // Reallocate and resize staging image
      UINT frame_buffer_width = 0;
      UINT frame_buffer_height = 0;
      {
         // Get width/height of framebuffer
         D3D11_TEXTURE2D_DESC bb_desc = {0};
         r_window->framebuffer->lpVtbl->GetDesc(r_window->framebuffer, &bb_desc);
         frame_buffer_width = bb_desc.Width;
         frame_buffer_height = bb_desc.Height;
         Assert(frame_buffer_width > 0 && frame_buffer_height > 0);

         // Allocate staging image memory
         r_window->stage_image = sg_make_image(&(sg_image_desc){
             .type = SG_IMAGETYPE_2D,
             .usage = {
                 .color_attachment = true,
             },
             .width = frame_buffer_width,
             .height = frame_buffer_height,
             .pixel_format = SG_PIXELFORMAT_BGRA8, // match your swapchain format
             .sample_count = 1,
             //.label = "stage image memory",
         });

         // color render target
         r_window->stage_image_write_view = sg_make_view(&(sg_view_desc){
             .color_attachment = {
                 .image = r_window->stage_image,
                 .mip_level = 0,
                 .slice = 0,
             },
             //.label = "stage image write"
         });

         // color target read
         r_window->stage_image_read_view = sg_make_view(&(sg_view_desc){
             .texture = {
                 .image = r_window->stage_image,
             },

             //.label = "stage image read",
         });
      }

      sg_reset_state_cache();

      // Update sokol swapchain object
      r_window->sokol_swapchain.width = frame_buffer_width;
      r_window->sokol_swapchain.height = frame_buffer_height;
      r_window->sokol_swapchain.d3d11.render_view = r_window->framebuffer_rtv;
   }

   _frame_stats.resized = resize_done;

   sg_begin_pass(&(sg_pass){.action = _sokol_state->clear_dark_grey,
                            .attachments = {.colors[0] = r_window->stage_image_write_view},
                            /*.label = "offscreen stage pass"*/});
};

void Core_R_WindowEndFrame(WM_Window window, R_Handle window_equip) {
   UnusedVariable(window);
   R_Sokol_Window* r_window = _R_SokolWindowFromHandle(window_equip);
   sg_end_pass();

   { // Begin final pass to framebuffer for display
      sg_begin_pass(&(sg_pass){
          .action = (sg_pass_action){
              .colors[0] = {
                  .load_action = SG_LOADACTION_LOAD}},

          .swapchain = r_window->sokol_swapchain,

          //.label = "Final frame buffer pass"
      });

      // testing again

      // set pipeline
      sg_apply_pipeline(_sokol_state->present_pipeline);

      // set bindings
      sg_bindings bind = {0};
      bind.views[0] = r_window->stage_image_read_view;
      bind.samplers[0] = _sokol_state->linear_sampler;
      sg_apply_bindings(&bind);

      sg_draw(0, 4, 1);
      _frame_stats.draw_count += 1;

      sg_end_pass();
   }

   sg_commit();

   UINT present_flags = 0;
   if (!_d3d11_state->vsync && _d3d11_state->allow_tearing) {
      present_flags |= DXGI_PRESENT_ALLOW_TEARING;
   }
   U64 present_begin_us = now_time_us();
   HRESULT result = IDXGISwapChain_Present(r_window->d3d11_swapchain, _d3d11_state->vsync, present_flags);
   AssertHR(result);

   U64 end_us = now_time_us();
   _frame_stats.build_ms = (F32)(present_begin_us - _frame_begin_us) / 1000.f;
   _frame_stats.present_ms = (F32)(end_us - present_begin_us) / 1000.f;
   _frame_stats.frame_ms = _frame_end_us != 0 ? (F32)(end_us - _frame_end_us) / 1000.f : 0;
   _frame_end_us = end_us;
   _frame_stats_last = _frame_stats;

#ifdef SOKOL_DEBUG
   // Here rather than only in Core_R_TakeMessages, so they get this frame's index.
   _R_PullD3D11Messages();
#endif

   // No LogScopeEnd - see the note in Core_R_WindowBeginFrame.

   arena_clear(_frame_arena);
   ++_frame;
}

void Core_R_WindowSubmit(WM_Window window, R_Handle window_equip, R_PassList* passes) {
   UnusedVariable(window_equip);
   for (R_PassNode* pass_n = passes->first; pass_n != 0; pass_n = pass_n->next) {
      R_Pass* pass = &pass_n->v;
      _frame_stats.pass_count += 1;

      switch (pass->kind) {
      default: {
         InvalidPath;
      }

      case R_PassKind_Test: {
         R_PassParams_Test* params = pass->params_test;

         sg_apply_pipeline(_sokol_state->test_pipeline);
         R_Sokol_Tex2D* texture = Core_R_Sokol_Tex2DFromHandle(params->texture);

         // If user didn't pass texture then fallback to dummy texture
         if (texture == &r_sokol_tex2d_nil) {
            texture = Core_R_Sokol_Tex2DFromHandle(_sokol_state->dummy_texture);
         }

         R_Sokol_Buffer* test_buffer = Core_R_Sokol_BufferFromHandle(_sokol_state->test_rect_buffer);
         sg_update_buffer((sg_buffer){.id = test_buffer->sokol_buffer_handle.id}, &(sg_range){params->rects.v, (U64)params->rects.byte_count});

         sg_bindings bind = {
             .vertex_buffers[0] = test_buffer->sokol_buffer_handle,
             .views[0] = texture->sokol_image_view,
             .samplers[0] = _sokol_state->linear_sampler};
         sg_apply_bindings(&bind);

         Rng2F32          client_rect = wm_client_rect_from_window(window);
         R_Sokol_Uniforms uniforms = (R_Sokol_Uniforms){.screen_resolution = {(int)(client_rect.max.x - client_rect.min.x), (int)(client_rect.max.y - client_rect.min.y)}};
         sg_apply_uniforms(0, &SG_RANGE(uniforms));

         sg_draw(0, 4, params->num_rects /*instances*/); // Since indexed rendering middle param asks for number of indices used. See sokol and search 'elements' for detail on draw call
         _frame_stats.draw_count += 1;
         _frame_stats.rect_count += (U64)params->num_rects;
         _frame_stats.upload_bytes += params->rects.byte_count;
      } break;

      case R_PassKind_UI: {
         R_PassParams_UI*    params = pass->params_ui;
         R_BatchGroup2DList* rect_batch_groups = &params->rects;

         Temp scratch = scratch_begin(0, 0);

         sg_apply_pipeline(_sokol_state->ui_pipeline);

         // Grab the streaming instance buffer handle once
         R_Sokol_Buffer* stream_buf = Core_R_Sokol_BufferFromHandle(_sokol_state->ui_rect_buffer);
         sg_buffer       inst_sg_buf = (sg_buffer){.id = stream_buf->sokol_buffer_handle.id};

         //- rjf: draw each batch group
         for (R_BatchGroup2DNode* group_n = rect_batch_groups->first; group_n != 0; group_n = group_n->next) {
            R_BatchList*          batches = &group_n->batches;
            R_BatchGroup2DParams* group_params = &group_n->params;

            // Nothing to draw?
            if (batches->byte_count == 0 || batches->bytes_per_inst == 0) {
               continue;
            }

            R_Sokol_Tex2D* texture = &r_sokol_tex2d_nil;
            { // Apply uniforms for batch group
               R_Handle tex_handle = group_params->tex;
               if (Core_R_HandleMatch(tex_handle, Core_R_HandleZero())) {
                  tex_handle = _sokol_state->dummy_texture;
               };
               texture = Core_R_Sokol_Tex2DFromHandle(tex_handle);

               Rng2F32          client_rect = wm_client_rect_from_window(window);
               R_Sokol_Uniforms uniforms = (R_Sokol_Uniforms){
                   .screen_resolution = {(int)(client_rect.max.x - client_rect.min.x), (int)(client_rect.max.y - client_rect.min.y)},
                   .texture_t2d_size_px = (Vec2F32){{texture->size.x, texture->size.y}}};

               sg_apply_uniforms(0, &SG_RANGE(uniforms));
            }

            { // Scissor to the group's clip, which bgz_draw's
              // clip stack sets. A zero clip means none - the whole window. An
              // empty one (clipped against something it doesn't overlap) draws
              // nothing.
               Rng2F32 clip = group_params->clip;

               if (clip.x0 == 0 && clip.y0 == 0 && clip.x1 == 0 && clip.y1 == 0) {
                  Vec2F32 window_dim = dim_2f32(wm_client_rect_from_window(window));
                  sg_apply_scissor_rectf(0, 0, window_dim.x, window_dim.y, true);
               }
               else if (clip.x0 >= clip.x1 || clip.y0 >= clip.y1) {
                  continue;
               }
               else {
                  sg_apply_scissor_rectf(clip.x0, clip.y0, clip.x1 - clip.x0, clip.y1 - clip.y0, true);
               }
            }

            // Instance count = total bytes / bytes per instance
            U64 inst_count_u64 = batches->byte_count / batches->bytes_per_inst;
            if (inst_count_u64 == 0) {
               continue;
            }
            int inst_count = (int)inst_count_u64;

            // 1) Pack batch nodes into a contiguous blob
            U64 total = batches->byte_count;
            U8* packed = push_array(scratch.arena, U8, total);
            U64 off = 0;
            for (R_BatchNode* bn = batches->first; bn != 0; bn = bn->next) {
               // skip empty nodes
               if (bn->v.byte_count == 0 || bn->v.v == 0)
                  continue;

               // avoid overflow if something is inconsistent
               Assert(off + bn->v.byte_count <= total);

               MemoryCopy(packed + off, bn->v.v, bn->v.byte_count);
               off += bn->v.byte_count;
            }
            Assert(off == total); // Should match exactly

            // 2) Upload contiguous bytes to the streaming instance buffer
            int         buffer_offset = sg_append_buffer(inst_sg_buf, &(sg_range){.ptr = packed, .size = total});
            sg_bindings bind = {
                .vertex_buffers[0] = inst_sg_buf,
                .vertex_buffer_offsets[0] = buffer_offset,
                .views[0] = texture->sokol_image_view,
                .samplers[0] = _sokol_state->linear_sampler};
            sg_apply_bindings(&bind);

            sg_draw(0, 4, inst_count);
            _frame_stats.draw_count += 1;
            _frame_stats.rect_count += inst_count_u64;
            _frame_stats.upload_bytes += total;
         }

         { // @Added(clip stack) Leave the scissor open for whatever draws next.
            Vec2F32 window_dim = dim_2f32(wm_client_rect_from_window(window));
            sg_apply_scissor_rectf(0, 0, window_dim.x, window_dim.y, true);
         }

         scratch_end(scratch);
#if 0
				sg_apply_pipeline(_sokol_state->ui_pipeline);
				R_Sokol_Tex2D* texture = Core_R_Sokol_Tex2DFromHandle(params->texture);

				//If user didn't pass texture then fallback to dummy texture
				if(texture == &r_sokol_tex2d_nil) {
					texture = Core_R_Sokol_Tex2DFromHandle(_sokol_state->dummy_texture);
				}

				R_Sokol_Buffer* ui_rect_buffer = Core_R_Sokol_BufferFromHandle(_sokol_state->ui_rect_buffer);
				sg_update_buffer((sg_buffer) { .id = ui_rect_buffer->sokol_buffer_handle.id }, &(sg_range) { params->rects.v, (U64)params->rects.byte_count });

				sg_bindings bind = {
					.vertex_buffers[0] = ui_rect_buffer->sokol_buffer_handle,
					.views[0] = texture->sokol_image_view,
					.samplers[0] = _sokol_state->linear_sampler
				};
				sg_apply_bindings(&bind);

				Rng2F32 client_rect = wm_client_rect_from_window(window);
				R_Sokol_Uniforms uniforms = (R_Sokol_Uniforms) { .screen_resolution = { (int)(client_rect.max.x - client_rect.min.x), (int)(client_rect.max.y - client_rect.min.y) } };
				sg_apply_uniforms(0, &SG_RANGE(uniforms));

				sg_draw(0, 4, params->num_rects/*instances*/);// Since indexed rendering middle param asks for number of indices used. See sokol and search 'elements' for detail on draw call
#endif
      }
      }
   }
}

R_FrameStats Core_R_LastFrameStats(void) {
   return _frame_stats_last;
}

#ifdef SOKOL_DEBUG
// Moves the debug layer's stored messages into _messages. Its errors break into
// the debugger first (SetBreakOnSeverity in Core_R_Init), so mostly warnings
// get this far. Info and plain messages are left out.
static void _R_PullD3D11Messages(void) {
   ID3D11InfoQueue* queue = _d3d11_info_queue;
   if (queue == 0) {
      return;
   }

   Temp   scratch = scratch_begin(0, 0);
   UINT64 count   = queue->lpVtbl->GetNumStoredMessages(queue);
   for (UINT64 i = 0; i < count; i += 1) {
      SIZE_T size = 0;
      if (FAILED(queue->lpVtbl->GetMessage(queue, i, 0, &size)) || size == 0) {
         continue;
      }

      D3D11_MESSAGE* message = (D3D11_MESSAGE*)push_array(scratch.arena, U8, size);
      if (FAILED(queue->lpVtbl->GetMessage(queue, i, message, &size))) {
         continue;
      }

      R_MessageLevel level;
      switch (message->Severity) {
         case D3D11_MESSAGE_SEVERITY_CORRUPTION:
         case D3D11_MESSAGE_SEVERITY_ERROR: level = R_MessageLevel_Error; break;
         case D3D11_MESSAGE_SEVERITY_WARNING: level = R_MessageLevel_Warning; break;
         default: continue;
      }

      // DescriptionByteLength counts the terminating zero.
      SIZE_T  text_size = message->DescriptionByteLength > 0 ? message->DescriptionByteLength - 1 : 0;
      String8 text      = str8((U8*)message->pDescription, (U64)text_size);
      _R_PushMessage(level, str8_lit("d3d11"), (U32)message->ID, text, str8_zero(), 0);
   }
   queue->lpVtbl->ClearStoredMessages(queue);
   scratch_end(scratch);
}
#endif

R_MessageList Core_R_TakeMessages(Arena* arena) {
#ifdef SOKOL_DEBUG
   _R_PullD3D11Messages();
#endif

   R_MessageList result = {0};
   result.dropped_count = _messages.dropped_count;
   for (R_Message* m = _messages.first; m != 0; m = m->next) {
      R_Message* copy = push_array(arena, R_Message, 1);
      *copy           = *m;
      copy->next      = 0;
      copy->tag       = push_str8_copy(arena, m->tag);
      copy->text      = push_str8_copy(arena, m->text);
      copy->file      = push_str8_copy(arena, m->file);
      SLLQueuePush(result.first, result.last, copy);
      result.count += 1;
   }

   MemoryZeroStruct(&_messages);
   if (_message_arena != 0) {
      arena_clear(_message_arena);
   }
   return result;
}

void Core_R_Shutdown(void) {
   _d3d11_state->base_device_ctx->lpVtbl->ClearState(_d3d11_state->base_device_ctx);
   _d3d11_state->base_device_ctx->lpVtbl->Flush(_d3d11_state->base_device_ctx);
   if (_d3d11_state->device_ctx) {
      _d3d11_state->device_ctx->lpVtbl->ClearState(_d3d11_state->device_ctx);
      _d3d11_state->device_ctx->lpVtbl->Flush(_d3d11_state->device_ctx);
   }

   // First, because sokol owns most of the D3D11 objects in this process -
   // buffers, textures, samplers, shaders, input layouts, rasterizer, depth
   // stencil and blend states - and every one of them holds a reference to the
   // device. This is the only call that releases them. Dropping our own device
   // reference first, as this used to, means the refcount never reaches zero
   // while we are looking, so nothing tears down cleanly and the runtime dumps
   // its live object list at DLL unload instead ("Process is terminating.
   // Using simple reporting.").
   sg_shutdown();

   // Every ID3D11Texture2D Core_R_Tex2DAlloc created. Core_R_Tex2DRelease only queues a
   // texture for destruction and Core_R_EndFrame does the actual release, so
   // anything released on the last frame - and anything never released at all,
   // like a glyph atlas that lives for the life of the program - still holds
   // its D3D11 texture here. sokol will not have freed them: they were injected
   // with .d3d11_texture, and sokol does not own an injected resource.
   //
   // After sg_shutdown, so sokol's image wrappers are already gone.
   for (R_Sokol_Tex2D* tex = _sokol_state->first_tex2d; tex != 0; tex = tex->all_next) {
      D3D11_SAFE_RELEASE(tex->d3d_texture_handle);
   }

   _sokol_state->first_pending_destroy_tex2d = 0;

   // Release the objects we created ourselves. The view goes before the texture
   // it views, so the count it holds is gone before we drop ours.
   for (U64 i = 0; i < R_SOKOL_WINDOW_CAP; i += 1) {
      R_Sokol_Window* r_window = &_windows[i];
      if (!r_window->in_use) {
         continue;
      }
      D3D11_SAFE_RELEASE(r_window->framebuffer_rtv);
      D3D11_SAFE_RELEASE(r_window->framebuffer);
      if (r_window->frame_latency_waitable != 0) {
         CloseHandle(r_window->frame_latency_waitable);
         r_window->frame_latency_waitable = 0;
      }
      D3D11_SAFE_RELEASE(r_window->d3d11_swapchain);
      MemoryZeroStruct(r_window);
   }
   D3D11_SAFE_RELEASE(_d3d11_state->dxgi_factory);
   D3D11_SAFE_RELEASE(_d3d11_state->dxgi_adapter);
   D3D11_SAFE_RELEASE(_d3d11_state->dxgi_device);
#ifdef SOKOL_DEBUG
   D3D11_SAFE_RELEASE(_d3d11_info_queue);
#endif
   D3D11_SAFE_RELEASE(_d3d11_state->device_ctx);
   D3D11_SAFE_RELEASE(_d3d11_state->device);
   D3D11_SAFE_RELEASE(_d3d11_state->base_device_ctx);
   D3D11_SAFE_RELEASE(_d3d11_state->base_device);

   // Last, and deliberately through DXGI rather than D3D11.
   //
   // ID3D11Debug comes from QueryInterface on the device, so a report through
   // it has to run while the device is still alive - which means it lists
   // every reference we are still legitimately holding and cannot tell you
   // whether you leaked. IDXGIDebug1 is a global interface, so it can report
   // here, after everything above has been released. Anything it still names
   // is a real leak.
   //
   // IGNORE_INTERNAL drops the objects the runtime holds for its own reasons,
   // which are not leaks and which otherwise bury the ones that are. DETAIL
   // gives the type and the debug name - see Core_R_SetDebugName, and sokol sets
   // its own from the .label in a resource desc.
#if BUILD_DEBUG
   {
      IDXGIDebug1* dxgi_debug = 0;

      if (SUCCEEDED(DXGIGetDebugInterface1(0, &IID_IDXGIDebug1, (void**)&dxgi_debug)) && dxgi_debug) {
         dxgi_debug->lpVtbl->ReportLiveObjects(dxgi_debug, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL);
         D3D11_SAFE_RELEASE(dxgi_debug);
      }
   }
#endif
}

// TODO: Just assert stage image is being rendered to and then display image details to user
void _SokolTraceHookCallBack_BeginPass(const sg_pass* pass, void* user_data) {
   UnusedVariable(user_data);

   { // Identify render buffer and width/height
      B32     using_attachment = (pass->attachments.colors[0].id != 0);
      String8 active_buffer = using_attachment ? str8_lit("offscreen") : str8_lit("swapchain");

      int width = 0, height = 0;
      if (using_attachment) {
         sg_view  v = pass->attachments.colors[0];
         sg_image img = sg_query_view_image(v);

         // 3) Identify which offscreen target (compare sg_image ids you own)
         for (U64 i = 0; i < R_SOKOL_WINDOW_CAP; i += 1) {
            if (_windows[i].in_use && img.id == _windows[i].stage_image.id) {
               active_buffer = str8_lit("stage image");
            }
         }

         // 4) Query size (note: this is base size, not mip-adjusted)
         sg_image_desc desc = sg_query_image_desc(img);
         width = desc.width;
         height = desc.height;
      }
      else {
         width = pass->swapchain.width;
         height = pass->swapchain.height;
      }
      UnusedVariable(active_buffer);
      UnusedVariable(width);
      UnusedVariable(height);

   }
}

void _SokolTraceHookCallBack_ApplyPipeline(sg_pipeline pip, void* user_data) {
   UnusedVariable(user_data);
   sg_pipeline_desc pip_desc = sg_query_pipeline_desc(pip);
   sg_shader_desc   shader_desc = sg_query_shader_desc(pip_desc.shader);
   UnusedVariable(shader_desc);
}

void _SokolTraceHookCallBack_EndPass() {
}
