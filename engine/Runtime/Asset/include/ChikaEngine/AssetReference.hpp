#pragma once

#include "ChikaEngine/AssetDatabase.hpp"
#include "ChikaEngine/reflection/ReflectionMacros.h"
#include <string>
#include <utility>

namespace ChikaEngine::Asset
{
    /**
     * @brief 保存可跨进程、移动和 Cook 阶段保持稳定的资产引用。
     *
     * GUID 是身份；subAsset 区分同一源资产导出的逻辑对象；diagnosticPath 只用于
     * 开发态诊断和旧路径迁移，不参与发布正确性。Runtime Handle 不允许持久化到这里。
     */
    MCLASS(AssetReference)
    {
        REFLECTION_BODY(AssetReference)

      public:
        AssetReference() = default;
        AssetReference(AssetGuid guid, AssetType expectedType, std::string subAsset = {}, std::string diagnosticPath = {}) : guid(std::move(guid.value)), subAsset(std::move(subAsset)), expectedType(static_cast<int>(expectedType)), diagnosticPath(std::move(diagnosticPath)) {}

        /**
         * @brief 创建仅用于迁移旧内容的路径引用。
         *
         * AssetManager 会尝试从 AssetDatabase 将路径解析为 GUID；新持久化内容应直接提供 GUID。
         */
        static AssetReference LegacyPath(std::string path, AssetType expectedType)
        {
            AssetReference reference;
            reference.expectedType = static_cast<int>(expectedType);
            reference.diagnosticPath = std::move(path);
            return reference;
        }

        /** @brief 返回稳定 GUID；无 GUID 的旧路径引用会返回无效值。 */
        AssetGuid GetGuid() const
        {
            return { guid };
        }

        /** @brief 返回引用声明的源资产类型。 */
        AssetType GetExpectedType() const
        {
            return static_cast<AssetType>(expectedType);
        }

        /** @brief 判断引用是否具备正式持久身份。 */
        bool IsValid() const
        {
            return !guid.empty();
        }

        MFIELD()
        std::string guid;
        MFIELD()
        std::string subAsset;
        MFIELD()
        int expectedType = static_cast<int>(AssetType::Unknown);
        MFIELD()
        std::string diagnosticPath;
    };
} // namespace ChikaEngine::Asset
