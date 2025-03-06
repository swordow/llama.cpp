#include "arg.h"
#include "common.h"
#include "log.h"
#include "llama.h"

#include <ctime>

#if defined(_MSC_VER)
#pragma warning(disable: 4244 4267) // possible loss of data
#endif

#if LLAMA_SHARED && LLAMA_EMBEDDING_SHARED
#ifdef __cplusplus
extern "C" {
#endif
// batch decode
 LLAMA_API bool llama_batch_decode(struct llama_context * ctx, struct llama_batch batch, int n_seq, int n_embd, int embd_norm, float* output);
#ifdef __cplusplus
}
#endif
#endif
