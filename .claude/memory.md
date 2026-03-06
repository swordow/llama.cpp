# llama.cpp 项目 Memory

## 用户偏好

- 语言：中文交流
- 个人调试注释（如 `leongyi`、`/*dont add special leongyi*/`）**不能移除**，方便回溯问题排查
- `add_special=false` 修改需要保留（可能配合 llamacpp_python 使用，避免双重 BOS）
- Plan 文档**必须包含详细的有效性/有意义评估**（问题描述、不修改的后果、修改价值、master 对比），不能仅做正确性检查
- 用户会要求输出 plan/report/notes 等 md 文档到项目根目录
- 需要 force push 时用 `--force-with-lease`
- 用户 GitHub: swordow, 邮箱: swordow89@gmail.com

## 分支结构

- `master` — 上游 ggml-org/llama.cpp 主分支（只同步，不修改）
- `llamacpp_joker` — 自定义修改的开发分支（**以远程 origin/llamacpp_joker 为准**，本地曾因 rebase 落后远程）
- `llamacpp_joker_2026.01` — 基于最新 master cherry-pick 重建的发布分支（2026-03-06 创建）

### 分支重建流程

1. `git fetch --all --prune` 确保远程最新
2. `git checkout llamacpp_joker && git reset --hard origin/llamacpp_joker` 同步本地
3. `git checkout master && git pull` 更新 master
4. `git checkout -b llamacpp_joker_YYYY.MM` 基于 master 创建新分支
5. `git cherry-pick <commit1> <commit2> ...` 依次 pick 自定义 commits
6. 解决冲突、验证、amend 修复

## 自定义修改详情（7 commits，按 cherry-pick 顺序）

### Commit 1: `6836e4ac5` — 输出完整 embeddings
- **文件**: `examples/embedding/embedding.cpp`
- **内容**: NONE pooling 时输出全量 embedding 值（逐 token）
- **意义**: master 默认只显示前3后3，NONE pooling 需要完整值用于 token-level 任务

### Commit 2: `998502176` — DeepSeek token 修复
- **文件**: `src/llama-vocab.cpp`
- **内容**: 注册 `<|EOT|>` 为 EOT+EOG，`<｜end▁of▁sentence｜>` 为 EOS+EOG
- **意义**: 修复 DeepSeek R1/V3 的 `'<|EOT|>' is not marked as EOG` 警告，不修复会导致生成无法正确终止
- **注意**: master 的 Kimi-K2 token（`[EOT]`, `[EOS]`, `_<EOT>`）必须保留

### Commit 3: `838f0fcd6` — DeepSeek R1 聊天模板
- **文件**: `src/llama-chat.h`, `src/llama-chat.cpp`, `tests/test-chat-template.cpp`
- **内容**: 新增 `LLM_CHAT_TEMPLATE_DEEPSEEK_R1`，模板含 `<think>\n` 推理前缀
- **关键**: R1 检测必须在 deepseek3 **之前**（更具体的模式优先），否则 R1 会被错误匹配为 V3
- **意义**: master 只有 deepseek/2/3，不支持 R1 推理格式

### Commit 4: `6d70b2811` — add_special=false + embedding 摘要
- **文件**: `examples/embedding/embedding.cpp`
- **内容**: 所有 `common_tokenize` 调用的 `add_special` 参数改为 `false`
- **意义**: 配合 DLL 导出使用时，外部调用者自行控制特殊 token，避免重复添加
- **状态**: 保留待确认

### Commit 5: `c6f15a5f5` — 通用 pooling_type 加载
- **文件**: `src/llama-model.cpp`
- **内容**: `ml.get_key(LLM_KV_POOLING_TYPE, hparams.pooling_type, false)` 提升到通用 `load_hparams()` 区域
- **意义**: master 仅在约 10 个特定架构（BERT, Qwen2 等）中加载，新架构的 embedding 模型无法读取 pooling 配置
- **注意**: master 架构专用块中仍有重复加载（冗余但无害）

### Commit 6: `fe75fd195` — embedding 摘要日志
- **文件**: `examples/embedding/embedding.cpp`
- **内容**: 非 NONE pooling 使用 `[first3 ... last3]` 格式
- **意义**: 全量输出刷屏（768-4096维），前3后3兼顾信息量和可读性

### Commit 7: `36a46c605` — embedding DLL 导出
- **文件**: `examples/embedding/CMakeLists.txt`, `examples/embedding/embedding.h`, `examples/embedding/embedding.cpp`
- **内容**:
  - `static void batch_decode(...)` → `bool llama_batch_decode(...)` 导出为 LLAMA_API
  - `embedding.h` 声明 C API（`extern "C"` + `LLAMA_API`）
  - `CMakeLists.txt` 条件创建共享库（`BUILD_SHARED_LIBS` 时）
  - `#if !LLAMA_EMBEDDING_SHARED` 包裹 `main()`
  - encoder/decoder 区分逻辑（`llama_encode` vs `llama_decode`）
- **意义**: 允许 Python/C# 等直接调用 embedding 计算，无需 CLI 或 HTTP server

## Cherry-pick 冲突解决经验

### 高频冲突文件
- `examples/embedding/embedding.cpp` — 最多冲突（5处），涉及函数签名、API 变更、日志格式
- `src/llama-vocab.cpp` — token 列表扩展（保留两边所有 token）
- `tests/test-chat-template.cpp` — 测试用例排序（master 新测试在前，自定义在后）
- `convert_hf_to_gguf.py` — 代码结构变化（基类重构）

### 常见陷阱
1. **n_embd vs n_embd_out**: master 用 `n_embd_out = llama_model_n_embd_out(model)`，自定义分支用 `n_embd = llama_model_n_embd(model)`。cherry-pick 后必须统一，否则 buffer 分配和索引不匹配
2. **void 函数误加 return**: 批量替换 n_embd_out→n_embd 时，可能从 bool 函数复制 `return true;` 到 void 函数
3. **不可达代码**: 冲突解决保留 master 的 `raise NotImplementedError()` 但也保留了 incoming 的后续代码，形成死代码
4. **API 名称变更**: `llama_kv_self_clear` → `llama_memory_clear(llama_get_memory(ctx), true)`（master 新 API）

## Plan2 验证清单

每次执行 Plan2 验证时，必须检查以下项目：

### 必要性评估（Section 0）
- [ ] 每个 commit 的问题描述、不修改的后果、修改价值
- [ ] 与最新 master 对比，确认无等效实现
- [ ] 输出详细报告到 plan2-report.md

### 正确性验证（Section 1-7）
- [ ] llama_batch_decode: 签名、返回值、n_embd 一致性、encoder/decoder 区分
- [ ] print_raw_embeddings: void 函数无 return value
- [ ] common_tokenize: 所有调用 add_special=false
- [ ] DLL 条件编译: include guard + main guard 正确闭合
- [ ] R1 检测顺序: R1 在 R3 之前
- [ ] Token 注册: EOT/EOS/EOG 完整，master token 未丢失
- [ ] pooling_type: 通用区域加载，false 表示可选
- [ ] convert_hf_to_gguf.py: raise 后无死代码

### 设计意图对比（Section 8）
- [ ] vs origin/llamacpp_joker: 所有自定义逻辑无丢失
- [ ] 差异分类: EXPECTED / ACCEPTABLE / CONCERN / BUG

## 已知遗留问题

- `src/llama-batch.cpp`: 多余 `#include "common.h"`（src/ 不应依赖 examples/）和重复 `#include "llama-impl.h"`（低优先级，无功能影响）

## 项目文档索引

| 文件 | 说明 |
|------|------|
| `CLAUDE.md` | 项目指令 + 文档索引 |
| `plan1.md` | Plan1: 分支验证和 cherry-pick 执行计划 |
| `plan2.md` | Plan2: 合并后正确性/必要性验证（含验证清单） |
| `plan2-report.md` | Plan2 详细评估报告 |
| `merging-notes.md` | Cherry-pick 冲突解决和修复记录 |
| `.claude/memory.md` | 本文件，跨会话持久化 memory |
| `.claude/settings.local.json` | Claude Code 本地设置 |
| `.claude/session-2026-03-06-plan1-plan2.jsonl` | 完整会话记录（Plan1+Plan2 执行，6MB JSONL） |
