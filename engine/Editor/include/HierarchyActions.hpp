#pragma once

#include "ChikaEngine/base/UIDGenerator.h"
#include <string_view>

namespace ChikaEngine::Framework
{
    class Scene;
}

namespace ChikaEngine::Editor
{
    enum class CreateChildStatus
    {
        Success,
        SceneNotEditable,
        InvalidParent,
        CreateFailed,
        ParentRejected,
    };

    struct CreateChildResult
    {
        CreateChildStatus status = CreateChildStatus::CreateFailed;
        Core::GameObjectID childId = Core::InvalidGameObjectID;

        bool Succeeded() const noexcept
        {
            return status == CreateChildStatus::Success;
        }
    };

    /**
     * @brief 在 Hierarchy 绘制完成后提交 Create Child 请求。
     *
     * 该函数只接收稳定 ID，并在提交时重新解析 parent。创建后若挂载失败，
     * 会销毁新对象，避免留下静默的 root orphan。
     */
    CreateChildResult CommitCreateChild(Framework::Scene& scene, Core::GameObjectID parentId, std::string_view childName = "GameObject");

    const char* CreateChildStatusName(CreateChildStatus status) noexcept;
} // namespace ChikaEngine::Editor
