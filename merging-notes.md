# Cherry-pick 合并记录

## 操作概要

- **日期**: 2026-03-06
- **目标**: 将 `origin/llamacpp_joker` 的 7 个自定义 commit cherry-pick 到最新 master，重建 `llamacpp_joker_2026.01`
- **结果**: 成功，10 个文件修改，+170/-38 行

## 步骤 1: 同步本地 llamacpp_joker

```bash
git checkout llamacpp_joker
git reset --hard origin/llamacpp_joker
# HEAD -> 61e280392 mod: export llama-embedding as dll for batch decode.
```

## 步骤 2: 重建 llamacpp_joker_2026.01

```bash
git checkout master && git pull
git branch -D llamacpp_joker_2026.01
git checkout -b llamacpp_joker_2026.01
git cherry-pick 634feaf92 19cb0c567 80bb5f2e3 7ab065ee5 8c76cd490 3680be2bd 61e280392
```

## Cherry-pick 冲突解决记录

### Commit 1: `634feaf92` — mod: output full embeddings...
**冲突文件**: `examples/embedding/embedding.cpp`
- **冲突位置**: NONE pooling 的日志输出（line ~294）
- **解决方式**: 取 incoming 版本（用 `n_embd` 输出全部 embedding + `[` 格式）
- **原因**: master 用 `n_embd_out` + 只显示前3后3，incoming 用 `n_embd` + 输出全部

### Commit 2: `19cb0c567` — fix: deepseek token debug logs
**冲突文件**: `src/llama-vocab.cpp`（2处冲突）
- **冲突1** (EOT token ~line 2277): master 新增了 `_<EOT>`, `[EOT]`(Kimi-K2), incoming 新增了 `<|EOT|>`
  - **解决**: 保留两边所有 token
- **冲突2** (EOG set ~line 2494): master 新增了 `_<EOT>`, `[EOT]`, `[EOS]`(Kimi-K2), incoming 新增了 `<|EOT|>`, `<｜end▁of▁sentence｜>`
  - **解决**: 保留两边所有 token

### Commit 3: `80bb5f2e3` — mod: chat template add support for deepseek-r1 series
**冲突文件**: `tests/test-chat-template.cpp`
- **冲突位置**: 测试用例数组末尾（line ~529）
- **解决方式**: 保留 master 新增的测试用例（YandexGPT, Ling-lite, ByteDance-Seed），在其后追加 DeepSeek R1 测试用例
- `src/llama-chat.cpp` 和 `src/llama-chat.h` 自动合并成功

### Commit 4: `7ab065ee5` — mod: dont add special and output embedding summary
**冲突文件**: `examples/embedding/embedding.cpp`
- **冲突位置**: tokenize 调用处（line ~184）
- **解决方式**: 保留 master 的 rerank 分类逻辑（含 rerank_prompt 和 SEP/EOS 拼接），将所有 `common_tokenize` 的 `add_special` 参数改为 `false`
- **原因**: master 新增了 rerank 支持代码，incoming 只是简单地改了 tokenize 参数

### Commit 5: `8c76cd490` — mod: add gguf.PoolingType.LAST and default pooling type
**冲突文件**:
1. `src/llama-model.cpp` — master 新增了 `n_embd_out_impl`, `n_expert_groups`, `n_group_used` 字段
   - **解决**: 保留 master 所有字段，末尾追加 `pooling_type`
2. `gguf-py/gguf/constants.py` — master 新增了 `RANK = 4`
   - **解决**: 保留 master 的 `RANK = 4`（incoming 只添加 `LAST = 3`，已存在）
3. `convert_hf_to_gguf.py`（2处）— master 新增了 `modify_tensors` 和 `cls_out_labels`
   - **解决**: 保留 master 代码（incoming 的旧 Qwen2VL 类定义已过时）

### Commit 6: `3680be2bd` — mod: dump embedding summary for valid pooling type
**冲突文件**: `examples/embedding/embedding.cpp`
- **冲突位置**: else 分支（非 NONE 非 RANK）的日志输出
- **解决方式**: 取 incoming 版本（前3后3 + `[` 格式替代原有全量/16个输出）

### Commit 7: `61e280392` — mod: export llama-embedding as dll for batch decode
**冲突文件**: `examples/embedding/embedding.cpp`（5处冲突）
- **冲突1** (includes): master 有 `<clocale>`，incoming 有 `embedding.h` 条件编译
  - **解决**: 两边都保留
- **冲突2** (batch_decode 函数签名): master 是 `static void batch_decode(..., n_embd_out, ...)`，incoming 是 `bool llama_batch_decode(..., n_embd, ...)`
  - **解决**: 取 incoming 版本（DLL 导出需要新签名）
- **冲突3** (encode/decode 逻辑): master 用简单的 `llama_decode`，incoming 区分 encoder/decoder 模型
  - **解决**: 取 incoming 版本（更完善的处理）
- **冲突4/5** (main 中 batch 调用): 函数名和参数变更
  - **解决**: 取 incoming 版本，但保留 master 的 `s >= n_seq_max` 条件检查

## 额外修复 Round 1（amend 到最后一个 commit）

cherry-pick 完成后发现 `n_embd` / `n_embd_out` 变量混用问题：

1. **添加 `const int n_embd = llama_model_n_embd(model);`** — main 函数中未定义 `n_embd`
2. **统一所有 `n_embd_out` 引用为 `n_embd`** — 包括：
   - `llama_batch_decode` 函数体内的 `n_embd_out` → `n_embd`
   - 添加 `return true;` 到函数末尾
   - NONE pooling 日志中的末尾3个元素
   - else 分支日志中的首尾元素
   - rerank score 的数组索引
   - cosine similarity 的数组指针和长度
   - JSON 输出的数组索引和循环终止
   - `print_raw_embeddings` 的参数
3. **移除未使用的 `n_embd_out` 定义**

## 额外修复 Round 2 — Plan2 验证后修复（amend 到最后一个 commit）

Plan2 验证发现 2 个 CRITICAL 问题：

1. **移除 `print_raw_embeddings` 中的 `return true;`** — `examples/embedding/embedding.cpp` ~line 110
   - 函数声明为 `static void` 但包含 `return true;`
   - Round 1 修复 n_embd_out 时误添加（从 `llama_batch_decode` 的修改模式复制过来）
   - 会导致编译错误

2. **移除 `convert_hf_to_gguf.py` 中不可达的 pooling 代码** — ~line 518-543（27行）
   - pooling type 自动检测代码被放在基类 `set_gguf_parameters()` 的 `raise NotImplementedError()` 之后
   - cherry-pick Commit 5 时，incoming 的旧代码结构与 master 不同，冲突解决保留了 master 的 `raise` 但也保留了 incoming 的 pooling 代码
   - master 已有独立的 `_try_set_pooling_type()` 方法覆盖此功能，各子类按需调用

## 最终分支状态

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

9 files changed, 142 insertions(+), 10 deletions(-)
