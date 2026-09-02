// rp_chiaki_shim.c - minimal platform shim so chiaki's pure crypto/protocol
// files link on bare-metal RP2350 (no pthreads).
// SPDX-License-Identifier: Apache-2.0
//
// chiaki's gkcrypt.c (and other reused pure files) reference the ChiakiThread /
// ChiakiMutex / ChiakiCond / aligned-alloc / log APIs. We never take the threaded
// paths (gkcrypt is initialised with key_buf_chunks=0 → no worker thread), but the
// symbols must still resolve. These are no-op stubs, plus a real chiaki_log() that
// routes to printf so chiaki's diagnostics land in the CDC LOG.DUMP ring.

#include <chiaki/common.h>
#include <chiaki/thread.h>
#include <chiaki/log.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

// --- logging: route chiaki logs to stdout (captured by the LOG ring) ----------
void chiaki_log(ChiakiLog *log, ChiakiLogLevel level, const char *fmt, ...)
{
    (void)log;
    static const char lch[] = { [CHIAKI_LOG_DEBUG]='D', [CHIAKI_LOG_VERBOSE]='V',
                                [CHIAKI_LOG_INFO]='I', [CHIAKI_LOG_WARNING]='W',
                                [CHIAKI_LOG_ERROR]='E' };
    char c = (level < (ChiakiLogLevel)sizeof(lch) && lch[level]) ? lch[level] : '?';
    printf("[chiaki:%c] ", c);
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    printf("\n");
}

// --- threads (never spawned on our config) ------------------------------------
ChiakiErrorCode chiaki_thread_create(ChiakiThread *t, ChiakiThreadFunc f, void *a)
{ (void)t; (void)f; (void)a; return CHIAKI_ERR_UNKNOWN; }   // must not be called
ChiakiErrorCode chiaki_thread_join(ChiakiThread *t, void **r) { (void)t; (void)r; return CHIAKI_ERR_SUCCESS; }
ChiakiErrorCode chiaki_thread_set_name(ChiakiThread *t, const char *n) { (void)t; (void)n; return CHIAKI_ERR_SUCCESS; }
void chiaki_thread_set_affinity(ChiakiThreadName n) { (void)n; }

// --- mutex / cond (no-op; single-threaded poll loop) --------------------------
ChiakiErrorCode chiaki_mutex_init(ChiakiMutex *m, bool r) { (void)m; (void)r; return CHIAKI_ERR_SUCCESS; }
ChiakiErrorCode chiaki_mutex_fini(ChiakiMutex *m) { (void)m; return CHIAKI_ERR_SUCCESS; }
ChiakiErrorCode chiaki_mutex_lock(ChiakiMutex *m) { (void)m; return CHIAKI_ERR_SUCCESS; }
ChiakiErrorCode chiaki_mutex_unlock(ChiakiMutex *m) { (void)m; return CHIAKI_ERR_SUCCESS; }
ChiakiErrorCode chiaki_cond_init(ChiakiCond *c) { (void)c; return CHIAKI_ERR_SUCCESS; }
ChiakiErrorCode chiaki_cond_fini(ChiakiCond *c) { (void)c; return CHIAKI_ERR_SUCCESS; }
ChiakiErrorCode chiaki_cond_signal(ChiakiCond *c) { (void)c; return CHIAKI_ERR_SUCCESS; }
ChiakiErrorCode chiaki_cond_wait_pred(ChiakiCond *c, ChiakiMutex *m, ChiakiCheckPred p, void *u)
{ (void)c; (void)m; (void)p; (void)u; return CHIAKI_ERR_SUCCESS; }
ChiakiErrorCode chiaki_cond_timedwait_pred(ChiakiCond *c, ChiakiMutex *m, uint64_t t, ChiakiCheckPred p, void *u)
{ (void)c; (void)m; (void)t; (void)p; (void)u; return CHIAKI_ERR_TIMEOUT; }

// --- aligned alloc ------------------------------------------------------------
void *chiaki_aligned_alloc(size_t alignment, size_t size)
{
    (void)alignment;
    return malloc(size);   // only used by gkcrypt's key_buf path, which we don't take
}
void chiaki_aligned_free(void *ptr) { free(ptr); }
