# llama.cpp 项目 Memory

## 用户偏好
- 语言：中文交流
- 个人调试注释（如 `leongyi`）不能移除，方便回溯问题排查
- `add_special=false` 修改需要保留（可能配合 llamacpp_python）
- Plan 文档需要包含详细的有效性/有意义评估，不能仅做正确性检查
- 用户会要求输出 plan/report/notes 等 md 文档
- 需要 force push 时用 `--force-with-lease`

## 分支结构
- `master` — 上游 llama.cpp 主分支
- `llamacpp_joker` — 自定义修改的开发分支（远程为准）
- `llamacpp_joker_2026.01` — 基于最新 master cherry-pick 重建的发布分支
- 详细文档见项目根目录: plan1.md, plan2.md, plan2-report.md, merging-notes.md

## 自定义修改概要（7 commits）
1. DeepSeek R1 聊天模板（`<think>\n` 推理格式）
2. DeepSeek token 修复（`<|EOT|>`, `<｜end▁of▁sentence｜>` EOG 注册）
3. `add_special=false`（避免 DLL 调用时双重 BOS）
4. 通用 pooling_type 加载（提升到 load_hparams 通用区域）
5. embedding 日志前3后3格式
6. NONE pooling 全量输出
7. embedding DLL 导出（llama_batch_decode 共享库）

## Cherry-pick 流程
- 详见 merging-notes.md
- 关键：cherry-pick 后需检查 n_embd/n_embd_out 一致性、void 函数无 return value、不可达代码

## 已知遗留问题
- src/llama-batch.cpp: 多余 `#include "common.h"` 和重复 `#include "llama-impl.h"`（低优先级）
