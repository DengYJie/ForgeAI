#include "AgentContextBuilder.h"
#include "agent/skill/SkillLoader.h"

namespace agent::runtime {

    QString AgentContextBuilder::buildSystemPrompt(
        const AgentRunContext& runContext,
        const domain::project::ProjectContext& projectContext,
        const QList<domain::agent::Skill>& activeSkills
    ) const {
        QString prompt = QStringLiteral(
            "你是 ForgeAI 的专业项目 Agent。仅在当前项目工作区内工作，并在需要事实时优先使用提供的工具。"
            "不要臆造文件内容；执行工具后，必须基于真实的工具返回结果继续推理和回答。\n\n"
            "项目工作区根目录：%1"
        ).arg(projectContext.rootPath.isEmpty() ? runContext.workspaceRoot : projectContext.rootPath);

        if (!projectContext.agentsInstructions.trimmed().isEmpty()) {
            prompt += QStringLiteral("\n\n以下是项目 AGENTS.md 规范与指引，必须严格遵守：\n%1")
                .arg(projectContext.agentsInstructions.trimmed());
        }

        skill::SkillLoader loader;
        const auto& skillsToInclude = !activeSkills.isEmpty() ? activeSkills : projectContext.skills;
        for (auto skill : skillsToInclude) {
            if (!skill.isEnabled) continue;

            if (skill.instructions.isEmpty() && !skill.path.isEmpty()) {
                loader.loadInstructions(skill);
            }

            if (!skill.instructions.trimmed().isEmpty()) {
                prompt += QStringLiteral("\n\n# Skill: %1\n%2")
                    .arg(skill.name, skill.instructions.trimmed());
            }
        }

        return prompt;
    }

} // namespace agent::runtime
