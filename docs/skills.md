# Skills

Skill 是项目级指令单元，存放于工作区的以下目录：

- `.agents/skills/<skill-id>/SKILL.md`
- `.skills/<skill-id>/SKILL.md`

## 文件格式

`SKILL.md` 可使用 YAML Frontmatter 描述元数据：

```md
---
id: cpp-review
name: C++ Review
description: Review Qt and C++ changes
tags: [cpp, qt, review]
---

审查时关注 QObject 生命周期、线程亲和性和错误处理。
```

支持字段：`id`、`name`、`description`、`tags`。未指定 `id` 时使用 Skill 所在目录名称。

## 加载与筛选

`SkillLoader` 先读取元数据；仅在构建 Agent 上下文时按需读取正文指令。`SkillRegistry` 负责按 ID 注册、去重、启用/禁用和查询。

项目 Agent 会根据 Agent 配置中的已启用 Skill 列表筛选指令，同时跳过 Registry 中禁用的 Skill。

## 使用建议

- 指令保持针对单一能力，避免把通用项目规则放入多个 Skill。
- 将长期项目规则放在 `AGENTS.md`；将可选、任务特定行为放在 Skill 中。
- 不要在 Skill 中写入密钥、令牌或机器私有路径。
