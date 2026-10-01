/*
 * libaudiopolicymanagercustom.so for the phh Android 12L GSI on the Xperia XZs.
 *
 * AudioPolicyService loads this library when it exists and creates the audio
 * policy manager through createAudioPolicyManager(). AOSP's manager mixes every
 * linear PCM track that does not ask for AUDIO_OUTPUT_FLAG_DIRECT, so hi-res
 * players end up resampled to the 48 kHz deep-buffer mixer. The CAF policy
 * (AudioPolicyManagerCustom) routes such tracks to the direct_pcm output
 * instead, but this GSI does not ship it.
 *
 * This library does not reimplement the policy. It creates the GSI's own
 * AudioPolicyManager (libaudiopolicymanagerdefault.so) and hands out a proxy
 * whose vtable forwards every slot unchanged, so it does not depend on the
 * manager's class layout beyond its size and vtable (both checked against the
 * GSI binary). Only getOutputForAttr() is intercepted: a media track in a
 * hi-res format (24/32-bit or float, or above 48 kHz) gets
 * AUDIO_OUTPUT_FLAG_DIRECT. If no direct output can take it, the AOSP manager
 * falls back to the mixer by itself (openDirectOutput() returns NAME_NOT_FOUND
 * for linear PCM).
 *
 * Built for android-12.1.0_r11 (SQ3A.220705.003.A1):
 *   sizeof(AudioPolicyManager) = 0x530, primary vtable = 103 slots,
 *   getOutputForAttr = slot 12.
 * Set persist.sys.xzs.hires_direct=0 to disable the interception.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/system_properties.h>
#include <system/audio.h>

extern "C" int __android_log_print(int prio, const char *tag, const char *fmt, ...);
#define TAG "xzs_apm"
#define LOGI(...) __android_log_print(4 /* ANDROID_LOG_INFO */, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(6 /* ANDROID_LOG_ERROR */, TAG, __VA_ARGS__)

namespace {

constexpr size_t kApmAlloc = 0x4000;  /* sizeof(AudioPolicyManager) is 0x530 */
constexpr int kSlots = 103;
constexpr int kGetOutputForAttrSlot = 12;

struct Proxy {
    void **vptr;
    void *impl;
};

typedef void (*apm_ctor_t)(void *self, void *client);
typedef int (*apm_init_t)(void *self);
typedef void (*dtor_t)(void *self);
typedef int (*get_output_for_attr_t)(void *self, const audio_attributes_t *attr,
        int *output, audio_session_t session, audio_stream_type_t *stream,
        const void *attributionSource, const audio_config_t *config,
        audio_output_flags_t *flags, int *selectedDeviceId, int *portId,
        void *secondaryOutputs, void *outputType);

void **impl_vtable(void *impl) { return *reinterpret_cast<void ***>(impl); }

bool hires_enabled() {
    char v[PROP_VALUE_MAX] = "";
    __system_property_get("persist.sys.xzs.hires_direct", v);
    return strcmp(v, "0") != 0;
}

bool is_hires_media(const audio_attributes_t *attr, const audio_config_t *config,
                    audio_output_flags_t flags) {
    if (attr == nullptr || config == nullptr)
        return false;
    if (attr->usage != AUDIO_USAGE_MEDIA)
        return false;
    /*
     * Leave fast, raw, mmap, offload, ... requests alone. DIRECT|DEEP_BUFFER is
     * what players with their own "hi-res output" ask for: the AOSP manager
     * opens the direct output with those flags, and the CAF HAL then treats the
     * stream as deep-buffer (48 kHz/16-bit app type, so the ADM resamples).
     * Reduce both cases to plain DIRECT, which the HAL runs as direct PCM.
     */
    if ((flags & ~(AUDIO_OUTPUT_FLAG_DEEP_BUFFER | AUDIO_OUTPUT_FLAG_DIRECT)) !=
            AUDIO_OUTPUT_FLAG_NONE)
        return false;
    if (!audio_is_linear_pcm(config->format))
        return false;
    if (audio_channel_count_from_out_mask(config->channel_mask) > 2)
        return false;
    if (config->sample_rate < 44100)
        return false;
    switch (config->format) {
    case AUDIO_FORMAT_PCM_24_BIT_PACKED:
    case AUDIO_FORMAT_PCM_8_24_BIT:
    case AUDIO_FORMAT_PCM_32_BIT:
    case AUDIO_FORMAT_PCM_FLOAT:
        return true;
    default:
        return config->sample_rate > 48000;
    }
}

}  // namespace

extern "C" {

/* forwarding thunks, defined in asm below */
#define DECL(n) void xzs_fwd_##n();
#define DECL10(t) DECL(t##0) DECL(t##1) DECL(t##2) DECL(t##3) DECL(t##4) \
                  DECL(t##5) DECL(t##6) DECL(t##7) DECL(t##8) DECL(t##9)
DECL10() DECL10(1) DECL10(2) DECL10(3) DECL10(4) DECL10(5) DECL10(6) DECL10(7) DECL10(8)
DECL10(9) DECL(100) DECL(101) DECL(102)
#undef DECL
#undef DECL10

static void proxy_complete_dtor(Proxy *p) {
    if (p->impl) {
        reinterpret_cast<dtor_t>(impl_vtable(p->impl)[1])(p->impl); /* deleting dtor */
        p->impl = nullptr;
    }
}

static void proxy_deleting_dtor(Proxy *p) {
    proxy_complete_dtor(p);
    free(p);
}

static int proxy_get_output_for_attr(Proxy *p, const audio_attributes_t *attr,
        int *output, audio_session_t session, audio_stream_type_t *stream,
        const void *attributionSource, const audio_config_t *config,
        audio_output_flags_t *flags, int *selectedDeviceId, int *portId,
        void *secondaryOutputs, void *outputType) {
    auto fn = reinterpret_cast<get_output_for_attr_t>(
            impl_vtable(p->impl)[kGetOutputForAttrSlot]);
    audio_output_flags_t req = flags ? *flags : AUDIO_OUTPUT_FLAG_NONE;
    bool hires = flags && hires_enabled() && is_hires_media(attr, config, req);
    if (hires)
        *flags = AUDIO_OUTPUT_FLAG_DIRECT;
    int ret = fn(p->impl, attr, output, session, stream, attributionSource, config,
                 flags, selectedDeviceId, portId, secondaryOutputs, outputType);
    if (hires)
        LOGI("hi-res media %u Hz fmt %#x ch %#x: req flags %#x -> output %d flags %#x ret %d",
             config->sample_rate, config->format, config->channel_mask, req,
             output ? *output : -1, *flags, ret);
    return ret;
}

#define F(n) reinterpret_cast<void *>(xzs_fwd_##n)
#define F10(t) F(t##0), F(t##1), F(t##2), F(t##3), F(t##4), \
               F(t##5), F(t##6), F(t##7), F(t##8), F(t##9)
/* [0] offset-to-top, [1] typeinfo (copied from the real vtable), [2..] slots */
static void *g_vtable[2 + kSlots] = {
    nullptr, nullptr,
    F10(), F10(1), F10(2), F10(3), F10(4), F10(5), F10(6), F10(7), F10(8), F10(9),
    F(100), F(101), F(102),
};
#undef F
#undef F10

/* the proxy handed out by createAudioPolicyManager(), if any */
static Proxy *g_proxy;

/* does a vtable slot of the GSI's AudioPolicyManager point at @prefix...? */
static bool slot_is(void **ap, int slot, const char *prefix) {
    Dl_info info;
    if (!dladdr(ap[slot], &info) || !info.dli_sname)
        return false;
    return strncmp(info.dli_sname, prefix, strlen(prefix)) == 0;
}

/*
 * Check, before anything is constructed, that this GSI's AudioPolicyManager
 * has the vtable this proxy was built for. On any other GSI the library just
 * creates the stock manager (no interception).
 */
static bool layout_ok(void *lib) {
    void **vt = static_cast<void **>(dlsym(lib, "_ZTVN7android18AudioPolicyManagerE"));
    if (!vt)
        return false;
    void **ap = vt + 2;  /* address point */
    if (ap[kSlots] != reinterpret_cast<void *>(static_cast<intptr_t>(-8)))
        return false;    /* secondary vtable (offset-to-top -8) right after slot 102 */
    return slot_is(ap, 0, "_ZN7android18AudioPolicyManagerD") &&
           slot_is(ap, 1, "_ZN7android18AudioPolicyManagerD") &&
           slot_is(ap, kGetOutputForAttrSlot,
                   "_ZN7android18AudioPolicyManager16getOutputForAttrEPK18audio_attributes_tPi"
                   "15audio_session_tP19audio_stream_type_tRKNS_7content22AttributionSourceState"
                   "EPK12audio_configP20audio_output_flags_tS4_S4_PNSt3__16vectorIiNSH_9allocatorIiEEEE"
                   "PNS_20AudioPolicyInterface13output_type_tE") &&
           slot_is(ap, kSlots - 1, "_ZNK7android18AudioPolicyManager19canBeSpatializedIntE");
}

__attribute__((visibility("default")))
void *createAudioPolicyManager(void *clientInterface) {
    void *lib = dlopen("libaudiopolicymanagerdefault.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib)
        lib = dlopen("libaudiopolicymanagerdefault.so", RTLD_NOW);
    auto ctor = reinterpret_cast<apm_ctor_t>(lib ? dlsym(lib,
            "_ZN7android18AudioPolicyManagerC1EPNS_26AudioPolicyClientInterfaceE") : nullptr);
    auto init = reinterpret_cast<apm_init_t>(lib ? dlsym(lib,
            "_ZN7android18AudioPolicyManager10initializeEv") : nullptr);
    if (!ctor || !init) {
        LOGE("AudioPolicyManager symbols not found");
        return nullptr;
    }
    bool use_proxy = layout_ok(lib);
    if (!use_proxy)
        LOGE("AudioPolicyManager layout differs from android-12.1.0_r11: stock policy, no hi-res direct");

    /* operator new in libc++ is malloc; allocate well above the real size (0x530) */
    void *impl = malloc(kApmAlloc);
    if (!impl)
        return nullptr;
    memset(impl, 0, kApmAlloc);
    ctor(impl, clientInterface);
    if (init(impl) != 0) {
        LOGE("AudioPolicyManager::initialize() failed");
        reinterpret_cast<dtor_t>(impl_vtable(impl)[1])(impl);
        return nullptr;
    }
    if (!use_proxy)
        return impl;

    g_vtable[1] = impl_vtable(impl)[-1];  /* typeinfo */
    g_vtable[2 + 0] = reinterpret_cast<void *>(proxy_complete_dtor);
    g_vtable[2 + 1] = reinterpret_cast<void *>(proxy_deleting_dtor);
    g_vtable[2 + kGetOutputForAttrSlot] = reinterpret_cast<void *>(proxy_get_output_for_attr);

    Proxy *p = static_cast<Proxy *>(malloc(sizeof(Proxy)));
    if (!p) {
        reinterpret_cast<dtor_t>(impl_vtable(impl)[1])(impl);
        return nullptr;
    }
    p->vptr = &g_vtable[2];
    p->impl = impl;
    g_proxy = p;
    LOGI("xzs hi-res audio policy proxy active (direct for hi-res media: %s)",
         hires_enabled() ? "on" : "off");
    return p;
}

__attribute__((visibility("default")))
void destroyAudioPolicyManager(void *interface) {
    if (!interface)
        return;
    if (interface == g_proxy) {
        g_proxy = nullptr;
        proxy_deleting_dtor(static_cast<Proxy *>(interface));
    } else {
        /* stock manager: delete through its virtual (deleting) destructor */
        reinterpret_cast<dtor_t>(impl_vtable(interface)[1])(interface);
    }
}

}  // extern "C"

/*
 * Slot n: this = x0; forward to impl->vtable[n] with every other argument
 * register (x1-x7, x8, stack) untouched. x16/x17 are the intra-call scratch
 * registers.
 */
#define THUNK(n)                                   \
    ".globl xzs_fwd_" #n "\n"                      \
    ".type xzs_fwd_" #n ", %function\n"            \
    "xzs_fwd_" #n ":\n"                            \
    "  ldr x16, [x0, #8]\n"                        \
    "  mov x0, x16\n"                              \
    "  ldr x17, [x16]\n"                           \
    "  ldr x17, [x17, #(" #n " * 8)]\n"            \
    "  br x17\n"
#define THUNK10(t) THUNK(t##0) THUNK(t##1) THUNK(t##2) THUNK(t##3) THUNK(t##4) \
                   THUNK(t##5) THUNK(t##6) THUNK(t##7) THUNK(t##8) THUNK(t##9)
asm(".text\n.balign 16\n"
    THUNK10() THUNK10(1) THUNK10(2) THUNK10(3) THUNK10(4) THUNK10(5) THUNK10(6)
    THUNK10(7) THUNK10(8) THUNK10(9) THUNK(100) THUNK(101) THUNK(102));
