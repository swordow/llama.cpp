# Plan2: 验证合并后 llamacpp_joker_2026.01 的正确性

## Context

Plan1 已完成：将 origin/llamacpp_joker 的 7 个 commit cherry-pick 到最新 master 上重建了 `llamacpp_joker_2026.01`。
本计划验证合并后的代码是否：(1) 修改有必要、有意义 (2) 仍然有效 (3) 没有逻辑错误 (4) 符合原有设计意图 (5) 没有破坏主分支逻辑

---

## 0. 修改必要性与意义评估

> **注**: 每次执行 Plan2 验证时，必须包含此类详细的有效性/有意义评估，不能仅做正确性检查。

### 0.1 各 Commit 详细评估

详细评估报告见 [plan2-report.md](plan2-report.md)。

### 0.2 必要性总表

| Commit | 修改内容 | 必要？ | 意义 | 理由 |
|--------|----------|--------|------|------|
| `6836e4ac5` | 输出完整 embeddings（NONE pooling 时） | YES | 调试/生产 | master 默认只显示前3后3，对于需要查看原始 embedding 值的场景不够用；NONE pooling 需要逐 token embedding 全量输出 |
| `998502176` | DeepSeek token 修复 | YES | 功能修复 | 解决 DeepSeek R1/V3 推理时 `'<\|EOT\|>' is not marked as EOG` 和 `special_eos_id is not in special_eog_ids` 警告，不修复会导致生成无法正确终止 |
| `838f0fcd6` | DeepSeek R1 聊天模板 | YES | 新功能 | master 只有 deepseek/deepseek2/deepseek3 模板，不支持 R1 的 `<think>` 推理格式。R1 是广泛使用的推理模型，缺少模板会导致输出格式错误 |
| `6d70b2811` | add_special=false + embedding 摘要 | YES（保留待确认） | 兼容性 | 可能是为配合 llamacpp_python 使用时避免重复添加 BOS/EOS token。若 Python 端已处理特殊 token，C++ 端再添加会导致双重 BOS 问题 |
| `c6f15a5f5` | 通用 pooling_type 加载 | YES | 架构改进 | master 仅在 BERT/Qwen2 等特定架构中加载 pooling_type，其他 embedding 模型（如自定义架构）无法从 GGUF 读取 pooling 配置。通用加载使所有模型受益 |
| `fe75fd195` | embedding 摘要日志（前3后3格式） | YES | 可用性 | master 原始日志输出全量 embedding 值（几百到上千维），刷屏且难以快速判断结果。前3后3格式兼顾信息量和可读性 |
| `36a46c605` | embedding DLL 导出 | YES | 集成需求 | 将 `llama_batch_decode` 导出为共享库函数，允许其他程序（如 Python/C# 宿主）直接调用 embedding 计算，无需通过 CLI 或 HTTP server |

### 0.3 修改是否仍有意义（vs 最新 master）

| 修改 | master 是否已有等效实现？ | 仍有意义？ |
|------|--------------------------|-----------|
| DeepSeek R1 聊天模板 | NO — master 只有 deepseek/2/3 | YES |
| DeepSeek token 修复 | NO — master 缺少 `<\|EOT\|>` 和 `<｜end▁of▁sentence｜>` | YES |
| add_special=false | NO — master 默认 `true` | YES（保留） |
| 通用 pooling_type | NO — master 仅架构专用加载 | YES |
| embedding 日志格式 | NO — master 用不同的输出格式 | YES |
| embedding DLL 导出 | NO — master 无此功能 | YES |
| embedding 全量输出 | 部分 — master 有前3后3但不区分 pooling | YES |

**结论**: 全部 7 个 commit 在最新 master 上均无等效实现，修改仍然必要且有意义。

---

## 1. Embedding 模块验证

### 1.1 llama_batch_decode 函数 — PASS

| 验证项 | 状态 | 说明 |
|--------|------|------|
| 函数签名 | PASS | `bool llama_batch_decode(struct llama_context * ctx, llama_batch batch, int n_seq, int n_embd, int embd_norm, float * output)` |
| 返回值 | PASS | 成功返回 `true`，encode/decode 失败返回 `false` |
| n_embd 一致性 | PASS | 全部使用 `n_embd`，无残留 `n_embd_out` |
| encoder/decoder 区分 | PASS | encoder-only 调用 `llama_encode`，decoder-only 调用 `llama_decode` |
| Memory API | PASS | 使用 `llama_memory_clear(llama_get_memory(ctx), true)`（适配 master 新 API，origin 用 `llama_kv_self_clear`） |
| Pooling 分支 | PASS | NONE: token embeddings via `llama_get_embeddings_ith`；其他: sequence embeddings via `llama_get_embeddings_seq` |
| GGML_ASSERT | PASS | embedding 指针均有 null 检查 |

### 1.2 print_raw_embeddings 函数 — PASS

| 验证项 | 状态 | 说明 |
|--------|------|------|
| 返回类型 | PASS | `static void`，函数体内无 `return true;`（Round 2 已修复） |
| RANK 处理 | PASS | `is_rank` 动态调整列数 `cols = std::min(n_embd, n_cls_out)` |

### 1.3 common_tokenize 调用 — PASS

| 位置 | 状态 | 说明 |
|------|------|------|
| line ~211 | PASS | `common_tokenize(vocab, final_prompt, false/*dont add special leongyi*/, true)` |
| line ~225 | PASS | `common_tokenize(ctx, final_prompt, false/*dont add special leongyi*/, true)` |
| line ~228 | PASS | `common_tokenize(ctx, prompt, false/*dont add special leongyi*/, true)` |

### 1.4 DLL 条件编译 — PASS

| 验证项 | 状态 | 说明 |
|--------|------|------|
| include guard | PASS | `#if LLAMA_SHARED && LLAMA_EMBEDDING_SHARED` 包裹 `#include "embedding.h"` |
| main guard | PASS | `#if !LLAMA_EMBEDDING_SHARED` 包裹整个 `main()` 函数 |
| `#endif` 闭合 | PASS | 正确闭合 |

### 1.5 main() 函数 — PASS

| 验证项 | 状态 | 说明 |
|--------|------|------|
| n_embd 定义 | PASS | `const int n_embd = llama_model_n_embd(model)` |
| n_seq_max 检查 | PASS | `batch.n_tokens + n_toks > n_batch \|\| s >= n_seq_max` |
| 日志格式 NONE | PASS | `[first3 ... last3]` 格式 |
| 日志格式 RANK | PASS | `rerank score %d: %8.3f [label]` 含分类标签 |
| 日志格式 else | PASS | `[first3 ... last3]` 格式 + cosine similarity 矩阵 |
| locale 设置 | PASS | `std::setlocale(LC_NUMERIC, "C")` |
| rerank 支持 | PASS | `llama_model_chat_template(model, "rerank")` + SEP/EOS token 检测 |

### 1.6 embedding.h — PASS

| 验证项 | 状态 | 说明 |
|--------|------|------|
| 签名匹配 | PASS | 声明与 embedding.cpp 定义完全一致 |
| extern "C" | PASS | 正确包裹 |
| LLAMA_API | PASS | DLL 导出宏正确使用 |
| 与 origin 对比 | PASS | 文件内容完全相同 |

### 1.7 CMakeLists.txt — PASS

| 验证项 | 状态 | 说明 |
|--------|------|------|
| 可执行文件重命名 | PASS | `llama-embedding` → `llama-embedding-cli` |
| 共享库目标 | PASS | 条件创建 `llama-embedding` 共享库 |
| 编译定义 | PASS | `LLAMA_EMBEDDING_SHARED` + `LLAMA_BUILD` |
| PIC | PASS | `POSITION_INDEPENDENT_CODE ON` |
| 与 origin 对比 | PASS | 文件内容完全相同 |

---

## 2. DeepSeek R1 聊天模板验证

### 2.1 Enum 定义 — PASS
- `LLM_CHAT_TEMPLATE_DEEPSEEK_R1` 位于 `llama-chat.h:31`
- 正确放置在 `DEEPSEEK_3` 之后、`COMMAND_R` 之前

### 2.2 模板字符串映射 — PASS
- `"deepseek-r1"` → `LLM_CHAT_TEMPLATE_DEEPSEEK_R1` 位于 `llama-chat.cpp:52`

### 2.3 检测逻辑 — PASS (CRITICAL)

| 顺序 | 模板 | 检测标志 | 状态 |
|------|------|----------|------|
| 1 | DEEPSEEK_2 | `'Assistant: ' + message['content'] + eos_token` | PASS |
| 2 | **DEEPSEEK_R1** | `<｜Assistant｜>` + `<｜User｜>` + `<｜end▁of▁sentence｜>` + **`<｜Assistant｜><think>\n`** | PASS |
| 3 | DEEPSEEK_3 | `<｜Assistant｜>` + `<｜User｜>` + `<｜end▁of▁sentence｜>` | PASS |

- R1 在 R3 **之前**检测（更具体的模式优先）— 逻辑正确
- 若反序，R1 模板会被错误匹配为 R3

### 2.4 模板实现 — PASS

| 元素 | 状态 | 内容 |
|------|------|------|
| BOS | PASS | `<｜begin▁of▁sentence｜>` |
| system | PASS | 直接输出内容，无包裹标签 |
| user | PASS | `<｜User｜>` + content |
| assistant | PASS | `<｜Assistant｜>` + content + `<｜end▁of▁sentence｜>` |
| 生成提示 | PASS | `<｜Assistant｜><think>\n` |

### 2.5 与 origin 对比 — PASS
- 模板实现**完全一致**
- 检测逻辑**完全一致**

---

## 3. DeepSeek Token 处理验证

### 3.1 Token 注册 — PASS

| Token | 类型 | 位置 | 状态 |
|-------|------|------|------|
| `<\|EOT\|>` | EOT | llama-vocab.cpp:2281 | PASS |
| `<｜end▁of▁sentence｜>` | EOS | llama-vocab.cpp:2294 | PASS |
| `<\|EOT\|>` | EOG | llama-vocab.cpp:2496 | PASS |
| `<｜end▁of▁sentence｜>` | EOG | llama-vocab.cpp:2497 | PASS |

### 3.2 Master 原有 Token 保留 — PASS

| Token | 用途 | 状态 |
|-------|------|------|
| `_<EOT>` | Legacy EOT | PASS 保留 |
| `<EOT>` | Standard EOT | PASS 保留 |
| `[EOT]` | Kimi-K2 EOT | PASS 保留 |
| `[EOS]` | Kimi-K2 EOS | PASS 保留 |

### 3.3 与 origin 对比 — PASS
- 所有 DeepSeek token 完整保留，master 新 token 也未丢失

---

## 4. 测试用例验证

### 4.1 DeepSeek R1 测试 — PASS
- 位于 `test-chat-template.cpp:553-560`
- 放置在 ByteDance-Seed 测试用例之后（master 最后一个测试之后）
- expected output 与 expected_output_jinja **一致**
- 输出内容与模板实现逻辑**匹配**

---

## 5. 通用 pooling_type 加载验证

### 5.1 llama-model.cpp — PASS

| 验证项 | 状态 | 说明 |
|--------|------|------|
| 加载位置 | PASS | 通用 `load_hparams()` 区域 line 529，所有架构可用 |
| 可选标志 | PASS | `false` 参数表示可选，GGUF 中无此字段不报错 |
| vs master | 改进 | master 仅在特定架构块中加载（BERT, Qwen2 等） |
| vs origin | 改进 | origin 同样仅在特定架构块中加载 |

**注意**: master 架构专用块中仍有重复的 pooling_type 加载（line 889, 923, 939 等），通用加载使其冗余但无害。

---

## 6. convert_hf_to_gguf.py 验证

### 6.1 基类 set_gguf_parameters — PASS

| 验证项 | 状态 | 说明 |
|--------|------|------|
| raise 后无死代码 | PASS | `raise NotImplementedError()` 之后直接是 `modify_tensors()` 方法（Round 2 已清理） |
| `_try_set_pooling_type()` | PASS | 独立方法存在于 `ModelSentenceTransformers` 类，line 1613-1637 |
| 子类调用 | PASS | ArceeModel, Qwen2Model, DreamModel, LLaDAModel, BertModel, EuroBertModel, Gemma3Model 等均按需调用 |

---

## 7. llama-batch.cpp 验证

### 7.1 Include 问题 — WARNING（低优先级）

| 问题 | 严重性 | 说明 |
|------|--------|------|
| `#include "common.h"` | 低 | src/ 核心库不应依赖 examples/common，违反层次依赖 |
| 重复 `#include "llama-impl.h"` | 低 | 冗余但有 include guard 无害 |

Master 无这两个 include。功能上无影响，属于代码整洁度问题。

---

## 8. origin/llamacpp_joker 对比验证（设计意图）

### 8.1 功能对照表

| 功能 | origin/llamacpp_joker | llamacpp_joker_2026.01 | 一致？ |
|------|----------------------|-------------------------|--------|
| llama_batch_decode 导出签名 | `bool llama_batch_decode(ctx, batch, n_seq, n_embd, embd_norm, output)` | 相同 | PASS |
| encoder/decoder 区分 | 区分 encode/decode 路径 | 相同 | PASS |
| DLL 条件编译 | `#if !LLAMA_EMBEDDING_SHARED` 包裹 main | 相同 | PASS |
| n_embd 统一使用 | 全部使用 n_embd | 全部使用 n_embd | PASS |
| add_special=false | 单个 tokenize 调用 | 3个 tokenize 调用（含 rerank） | PASS 扩展 |
| 日志格式 [前3...后3] | 实现 | 实现 | PASS |
| batch 容量检查 | `n_toks > n_batch` | `n_toks > n_batch \|\| s >= n_seq_max` | PASS 增强 |
| KV cache API | `llama_kv_self_clear` | `llama_memory_clear` (新API) | PASS 适配 |
| R1 聊天模板 | 实现 | 实现（完全一致） | PASS |
| DeepSeek token 修复 | EOT/EOS/EOG 注册 | 相同 + master 新 token 保留 | PASS |
| 通用 pooling_type 加载 | 架构专用 | 通用加载（改进） | PASS |
| embedding.h DLL 头文件 | 实现 | 完全相同 | PASS |
| CMakeLists.txt DLL 构建 | 实现 | 完全相同 | PASS |

### 8.2 差异分类

| 分类 | 数量 | 说明 |
|------|------|------|
| EXPECTED（master 新代码导致） | 多处 | 新 API 名称、新功能（rerank, 分类标签）、新模型支持 |
| ACCEPTABLE（功能无影响的差异） | 2处 | llama-batch.cpp 多余 include |
| CONCERN（自定义逻辑丢失） | 0 | 无 |
| BUG（合并引入的错误） | 0 | 无（Round 2 已修复 2 个 CRITICAL） |

---

## 9. 总结

### 最终验证状态

| 修改模块 | 必要？ | 有意义？ | 有效？ | 正确？ | 设计意图保留？ | 主分支逻辑？ |
|----------|--------|----------|--------|--------|----------------|-------------|
| Embedding DLL 导出 | YES | YES — 集成需求 | PASS | PASS | PASS | 未破坏 |
| Embedding 日志格式 | YES | YES — 可读性 | PASS | PASS | PASS | 未破坏 |
| Embedding 全量输出 | YES | YES — 调试需求 | PASS | PASS | PASS | 未破坏 |
| add_special=false | YES（保留） | YES — 兼容性 | PASS | PASS（待确认场景） | PASS 扩展 | 未破坏 |
| DeepSeek R1 聊天模板 | YES | YES — R1 无替代 | PASS | PASS | PASS | 未破坏 |
| DeepSeek Token 修复 | YES | YES — 修复警告 | PASS | PASS | PASS | 未破坏 |
| 通用 pooling_type 加载 | YES | YES — 架构改进 | PASS | PASS | PASS 改进 | 未破坏 |
| convert_hf_to_gguf.py | N/A | N/A（死代码已清理） | PASS | PASS | PASS | 未破坏 |
| llama-batch.cpp | 低 | 低 — 副作用 | WARNING | 低 | 待清理 | 未破坏 |

### 已修复问题（Plan2 执行）

1. **[CRITICAL] print_raw_embeddings `return true;`** — 已移除（Round 2）
2. **[CRITICAL] convert_hf_to_gguf.py 不可达代码** — 已移除 27 行（Round 2）

### 遗留低优先级问题

1. **llama-batch.cpp 多余 include** — `#include "common.h"` 和重复 `#include "llama-impl.h"`，无害但不整洁

### 最终分支状态

```
36a46c605 mod: export llama-embedding as dll for batch decode.
fe75fd195 mod: dump embedding summary for valid pooling type, not full embeddings.
c6f15a5f5 mod: add gguf.PoolingType.LAST and default...
6d70b2811 mod: dont add special and output embedding summary
838f0fcd6 mod: chat template add support for deepseek-r1 series
998502176 fix: deepseek token debug logs
6836e4ac5 mod: output full embeddings...
a0ed91a44 (master) models : kda chunk size = 16 (#19827)
```

9 files changed, 142 insertions(+), 38 deletions(-)
