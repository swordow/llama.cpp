# Plan1: 验证 llamacpp_joker_2026.01 分支修改

## Context

该分支包含6个commit（远程7个），涉及9个文件的修改（+95, -6行），主要目标：
1. DeepSeek R1 聊天模板支持
2. DeepSeek 特殊 token 处理修复
3. Embedding 功能修改（DLL导出、tokenize参数、日志格式）
4. 通用 pooling_type 加载

---

## 逐项分析结果

### 1. DeepSeek R1 聊天模板 — ✅ 有意义，基本正确
**文件**: [llama-chat.h](src/llama-chat.h), [llama-chat.cpp](src/llama-chat.cpp), [test-chat-template.cpp](tests/test-chat-template.cpp)

- Master 没有 R1 支持，只有 deepseek/deepseek2/deepseek3
- 检测逻辑正确：通过 `<｜Assistant｜><think>\n` 区分 R1 和 deepseek3，且 R1 检测放在 deepseek3 之前（更具体的匹配优先）
- 模板实现正确：BOS + system + User/Assistant 标签 + `<think>\n` 生成提示
- 测试用例的 expected output 与 Jinja 模板逻辑一致
- **小问题**: `} else if (...)` 后的 `else if` 换行风格不一致（`}\n    else if` vs `} else if`），但不影响功能

### 2. DeepSeek Token 修复 — ✅ 有意义，解决真实问题
**文件**: [llama-vocab.cpp](src/llama-vocab.cpp)

- 添加 `<|EOT|>` 为 EOT token 并加入 EOG 集合 — 解决 DeepSeek R1 的 `'<|EOT|>' is not marked as EOG` 警告
- 添加 `<｜end▁of▁sentence｜>` 为 EOS token — 解决 `special_eos_id is not in special_eog_ids` 警告
- 添加 `<｜end▁of▁sentence｜>` 到 EOG 集合 — 确保生成终止
- **逻辑正确**，master 确实缺少这些 token 的完整处理

### 3. Embedding `add_special=false` — ⚠️ 保留，待进一步确认
**文件**: [embedding.cpp](examples/embedding/embedding.cpp)

- 将 `common_tokenize(ctx, prompt, true, true)` 改为 `common_tokenize(ctx, prompt, false, true)`
- 注释写着 `/*dont add special leongyi*/`
- 可能是为配合 llamacpp_python 使用时避免重复添加特殊 token
- **保留此修改**，用户后续确认具体使用场景

**最初分析（供参考）**：
- `common_tokenize` 的第三个参数 `add_special` 控制是否自动添加特殊 token（如 BOS/EOS）
- 对于大多数 embedding 模型，BOS token 是必要的；禁用可能降低 embedding 质量
- **但**：如果模型的 tokenizer 已经在 prompt template 中包含了特殊 token，重复添加会导致问题（例如出现两个 BOS）
- 如果是配合 llamacpp_python 使用，Python 端可能已经处理了特殊 token 的添加，此时 C++ 端再添加会重复

### 4. Embedding 日志修改 — ❌ 本地编译错误（远程已修复）
**文件**: [embedding.cpp](examples/embedding/embedding.cpp)

- **本地分支问题**: `n_embd` 在 `main()` 中未定义，编译错误
- **远程已修复**: `origin/llamacpp_joker` line 193 定义了 `const int n_embd = llama_model_n_embd(model);`
- 远程还新增了 commit `3680be2bd`，完善了日志输出（显示前3个+后3个值）

### 5. Embedding DLL 导出 — ❌ 本地不完整（远程已完整实现）
**文件**: [CMakeLists.txt](examples/embedding/CMakeLists.txt), [embedding.h](examples/embedding/embedding.h), [embedding.cpp](examples/embedding/embedding.cpp)

- **本地分支问题**: commit `ad4ab8fc5` 只修改了 header/CMake，遗漏了 embedding.cpp 中函数签名修改
- **远程已修复**: commit `61e280392`（同名 message 但不同内容）包含完整实现：
  - `static void batch_decode(...)` → `bool llama_batch_decode(...)`
  - 添加 `#include "embedding.h"` 条件编译
  - `#if !LLAMA_EMBEDDING_SHARED` 包裹 `main()` 函数
  - 添加 `return true/false` 错误处理
- **根因**: 本地 rebase 时产生了与远程不同的 commit，丢失了 embedding.cpp 的修改

### 6. llama-batch.cpp 重复 include — ⚠️ 无害但不必要
**文件**: [llama-batch.cpp](src/llama-batch.cpp)

- 添加 `#include "common.h"` — 来自 examples/common，不应出现在 src/ 核心库中（违反层次依赖）
- 重复 `#include "llama-impl.h"` — 有 include guard 所以无害，但代码不整洁
- 这个 include 可能是为了配合 DLL 导出功能而添加的，但由于功能未完成，目前无实际作用

### 7. 通用 pooling_type 加载 — ✅ 有意义
**文件**: [llama-model.cpp](src/llama-model.cpp)

- Master 只在特定架构（BERT, EUROBERT 等）中加载 pooling_type
- 分支将其提升为通用加载（`false` 表示可选），使所有模型都能从 GGUF 读取 pooling_type
- 这对 embedding 模型更友好，逻辑正确

---

## 总结

| 修改 | 有意义？ | 正确？ | 严重程度 |
|------|---------|--------|---------|
| DeepSeek R1 聊天模板 | ✅ 是 | ✅ 正确 | — |
| DeepSeek Token 修复 | ✅ 是 | ✅ 正确 | — |
| `add_special=false` | ⚠️ 保留 | ⚠️ 待确认 | 中 |
| Embedding 日志修改 | ✅ 本地有误 | ❌ 本地编译错误，远程已修复 | **高** |
| Embedding DLL 导出 | ✅ 有意义 | ❌ 本地不完整，远程已完整 | **高** |
| 重复 include | ❌ 不必要 | ⚠️ 违反层次 | 低 |
| 通用 pooling_type | ✅ 是 | ✅ 正确 | — |

### 关于 rebase/pick 的结论

**本地 `llamacpp_joker` 落后于远程 `origin/llamacpp_joker`**：
- 远程多了 commit `3680be2bd`（"dump embedding summary for valid pooling type"）
- 远程的 DLL 导出 commit `61e280392` 与本地的 `ad4ab8fc5` message 相同但**内容不同**

**远程版本的 `61e280392` 包含了完整实现**：
- ✅ 将 `static void batch_decode(...)` 改为 `bool llama_batch_decode(...)`
- ✅ 添加 `#include "embedding.h"` 条件编译
- ✅ 用 `#if !LLAMA_EMBEDDING_SHARED` 包裹 `main()` 函数
- ✅ 添加 `return true/false` 错误处理

**远程版本的 `3680be2bd` 修复了日志问题**：
- ✅ 在 pooling_type != NONE 的分支中，使用 `n_embd`（此时函数参数已改名，`n_embd` 是有效的）

**根因**: 本地分支在某次 rebase 时没有完整拉取远程的最新代码，导致 DLL 导出 commit 丢失了 embedding.cpp 的修改。

### 执行计划（已完成）

#### 步骤 1: 同步本地 llamacpp_joker 到远程 ✅
```bash
git checkout llamacpp_joker
git reset --hard origin/llamacpp_joker
```

#### 步骤 2: 基于最新 master 重建 llamacpp_joker_2026.01 ✅
从 origin/llamacpp_joker 提取自定义 commit，cherry-pick 到最新 master 上。

#### 步骤 3: 验证
- ✅ cherry-pick 完成，所有冲突已解决
- ⏳ 编译验证和测试待执行（plan2）

### 注意事项
- **保留个人调试注释**（如 `leongyi`）：方便回溯问题排查
- **保留 `add_special=false`**: 待用户进一步确认是否为配合 llamacpp_python 的 workaround
