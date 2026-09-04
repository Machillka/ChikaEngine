#include "InspectorPanel.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/component/Animator.hpp"
#include "ChikaEngine/component/Collider.hpp"
#include "ChikaEngine/component/LightComponent.hpp"
#include "ChikaEngine/component/MeshRenderer.h"
#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/component/ScriptComponent.h"
#include "ChikaEngine/scene/scene.hpp"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/ChikaMath.h"
#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ChikaEngine::Editor
{
    bool DrawReflectedObject(void* instance, const Reflection::ClassInfo* classInfo);

    template <std::size_t Size> void CopyToFixedBuffer(char (&destination)[Size], std::string_view source)
    {
        static_assert(Size > 0);

        const std::size_t copiedLength = source.copy(destination, Size - 1);
        destination[copiedLength] = '\0';
    }

    std::string ToLower(std::string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (char character : value)
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        return result;
    }

    std::string InspectorLabel(std::string_view name)
    {
        std::string visible;
        visible.reserve(name.size());

        for (size_t index = 0; index < name.size(); ++index)
        {
            const char character = name[index];
            if (character == '_')
            {
                if (!visible.empty() && visible.back() != ' ')
                    visible.push_back(' ');
                continue;
            }

            const bool startsWord = !visible.empty() && visible.back() != ' ' && std::isupper(static_cast<unsigned char>(character))
                && index > 0 && std::islower(static_cast<unsigned char>(name[index - 1]));
            if (startsWord)
                visible.push_back(' ');
            visible.push_back(character);
        }

        while (!visible.empty() && visible.front() == ' ')
            visible.erase(visible.begin());
        while (!visible.empty() && visible.back() == ' ')
            visible.pop_back();

        if (visible.empty())
            visible = "Property";
        else
            visible.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(visible.front())));

        // The original reflection/material name remains the stable ImGui identity but is hidden after ##.
        visible += "##";
        visible.append(name);
        return visible;
    }

    bool IsColorName(std::string_view name)
    {
        const std::string lower = ToLower(name);
        return lower.find("color") != std::string::npos || lower.find("emissive") != std::string::npos;
    }

    bool IsColorParameter(std::string_view name, Resource::MaterialParameterType type)
    {
        return type == Resource::MaterialParameterType::Vec4 && IsColorName(name);
    }

    ImGuiColorEditFlags ColorEditFlags(bool hdr, bool hasAlpha)
    {
        ImGuiColorEditFlags flags =
            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar;
        if (hdr)
            flags |= ImGuiColorEditFlags_HDR;
        if (hasAlpha)
            flags |= ImGuiColorEditFlags_AlphaBar;
        return flags;
    }

    bool DrawColor3(const char* label, float* values, bool hdr = false)
    {
        // ColorEdit keeps RGB float fields visible and opens the picker when the preview swatch is clicked.
        return ImGui::ColorEdit3(label, values, ColorEditFlags(hdr, false));
    }

    bool DrawColor4(const char* label, float* values, bool hdr = false)
    {
        return ImGui::ColorEdit4(label, values, ColorEditFlags(hdr, true));
    }

    bool IsEmissiveParameter(std::string_view name)
    {
        return ToLower(name).find("emissive") != std::string::npos;
    }

    bool IsUnitParameter(std::string_view name)
    {
        return name == "Metallic" || name == "Roughness" || name == "OcclusionStrength";
    }

    bool IsNormalScaleParameter(std::string_view name)
    {
        return name == "NormalScale";
    }

    Math::Vector3 QuaternionToEulerDegrees(Math::Quaternion q)
    {
        const float lengthSquared = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        if (lengthSquared <= 0.000001f)
            return {};

        const float invLength = 1.0f / std::sqrt(lengthSquared);
        q.x *= invLength;
        q.y *= invLength;
        q.z *= invLength;
        q.w *= invLength;

        const float sinXCosY = 2.0f * (q.w * q.x + q.y * q.z);
        const float cosXCosY = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
        const float x = std::atan2(sinXCosY, cosXCosY);

        const float sinY = 2.0f * (q.w * q.y - q.z * q.x);
        const float y = std::abs(sinY) >= 1.0f ? std::copysign(Math::PI * 0.5f, sinY) : std::asin(sinY);

        const float sinZCosY = 2.0f * (q.w * q.z + q.x * q.y);
        const float cosZCosY = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
        const float z = std::atan2(sinZCosY, cosZCosY);

        return { x * Math::RAD2DEG, y * Math::RAD2DEG, z * Math::RAD2DEG };
    }

    Math::Quaternion EulerDegreesToQuaternion(const Math::Vector3& eulerDegrees)
    {
        return Math::Quaternion::FromEuler({
            eulerDegrees.x * Math::DEG2RAD,
            eulerDegrees.y * Math::DEG2RAD,
            eulerDegrees.z * Math::DEG2RAD,
        }).Normalized();
    }

    void DrawMaterialReference(const Asset::AssetReference& reference)
    {
        if (!reference.diagnosticPath.empty())
        {
            ImGui::TextDisabled("Material Asset: %s", reference.diagnosticPath.c_str());
            return;
        }
        if (reference.IsValid())
        {
            ImGui::TextDisabled("Material GUID: %s", reference.guid.c_str());
            return;
        }
        ImGui::TextDisabled("Material Asset: none");
    }

    bool DrawMaterialParameterControl(const Resource::MaterialParameterInfo& parameter, std::vector<float>& values)
    {
        values.resize(parameter.componentCount, 0.0f);
        const std::string label = InspectorLabel(parameter.name);

        switch (parameter.type)
        {
        case Resource::MaterialParameterType::Float:
        {
            if (IsUnitParameter(parameter.name))
                return ImGui::DragFloat(label.c_str(), values.data(), 0.01f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            if (IsNormalScaleParameter(parameter.name))
                return ImGui::DragFloat(label.c_str(), values.data(), 0.01f, 0.0f, 4.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            return ImGui::DragFloat(label.c_str(), values.data(), 0.01f);
        }
        case Resource::MaterialParameterType::Vec2:
            return ImGui::DragFloat2(label.c_str(), values.data(), 0.01f);
        case Resource::MaterialParameterType::Vec3:
            return ImGui::DragFloat3(label.c_str(), values.data(), 0.01f);
        case Resource::MaterialParameterType::Vec4:
        {
            if (IsColorParameter(parameter.name, parameter.type))
                return DrawColor4(label.c_str(), values.data(), IsEmissiveParameter(parameter.name));
            return ImGui::DragFloat4(label.c_str(), values.data(), 0.01f);
        }
        case Resource::MaterialParameterType::Bool:
        {
            bool enabled = !values.empty() && values[0] != 0.0f;
            if (!ImGui::Checkbox(label.c_str(), &enabled))
                return false;
            values[0] = enabled ? 1.0f : 0.0f;
            return true;
        }
        }

        ImGui::TextDisabled("Unsupported material parameter: %s", parameter.name.c_str());
        return false;
    }

    bool DrawProperty(const Reflection::PropertyInfo& prop, void* instance)
    {
        if (!prop.Get || !prop.Set)
            return false;

        const std::string label = InspectorLabel(prop.Name);

        if (prop.IsPointer)
        {
            void* ptrValue = nullptr;
            prop.Get(instance, &ptrValue); // 取出实际指针
            if (ptrValue)
            {
                std::string typeName = prop.TypeName;
                if (typeName.back() == '*')
                    typeName.pop_back();

                const auto* refClass = Reflection::TypeRegister::Instance().GetClassByName(typeName);
                if (refClass)
                {
                    if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        DrawReflectedObject(ptrValue, refClass);
                        ImGui::TreePop();
                    }
                }
                else
                {
                    ImGui::TextDisabled("Unregistered Pointer: %.*s", static_cast<int>(label.find("##")), label.c_str());
                }
            }
            return false;
        }

        switch (prop.Type)
        {
        case Reflection::ReflectType::Int:
        {
            int val;
            prop.Get(instance, &val);
            if (ImGui::DragInt(label.c_str(), &val))
            {
                prop.Set(instance, &val);
                return true;
            }
            break;
        }
        case Reflection::ReflectType::Float:
        {
            float val;
            prop.Get(instance, &val);
            if (ImGui::DragFloat(label.c_str(), &val, 0.1f))
            {
                prop.Set(instance, &val);
                return true;
            }
            break;
        }
        case Reflection::ReflectType::Bool:
        {
            bool val;
            prop.Get(instance, &val);
            if (ImGui::Checkbox(label.c_str(), &val))
            {
                prop.Set(instance, &val);
                return true;
            }
            break;
        }
        case Reflection::ReflectType::String:
        {
            std::string val;
            prop.Get(instance, &val);
            char buffer[256];
            CopyToFixedBuffer(buffer, val);
            if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer)))
            {
                val = buffer;
                prop.Set(instance, &val);
                return true;
            }
            break;
        }
        case Reflection::ReflectType::Object:
        {
            if (prop.TypeName.find("Vector3") != std::string::npos)
            {
                Math::Vector3 val;
                prop.Get(instance, &val);
                const bool changed = IsColorName(prop.Name)
                    ? DrawColor3(label.c_str(), &val.x, IsEmissiveParameter(prop.Name))
                    : ImGui::DragFloat3(label.c_str(), &val.x, 0.1f);
                if (changed)
                {
                    prop.Set(instance, &val);
                    return true;
                }
            }
            else if (prop.TypeName.find("Vector4") != std::string::npos)
            {
                Math::Vector4 val;
                prop.Get(instance, &val);
                const bool changed = IsColorName(prop.Name)
                    ? DrawColor4(label.c_str(), &val.x, IsEmissiveParameter(prop.Name))
                    : ImGui::DragFloat4(label.c_str(), &val.x, 0.1f);
                if (changed)
                {
                    prop.Set(instance, &val);
                    return true;
                }
            }
            else if (prop.TypeName.find("Quaternion") != std::string::npos)
            {
                Math::Quaternion val;
                prop.Get(instance, &val);
                Math::Vector3 eulerDegrees = QuaternionToEulerDegrees(val);
                if (ImGui::DragFloat3(label.c_str(), &eulerDegrees.x, 0.5f))
                {
                    val = EulerDegreesToQuaternion(eulerDegrees);
                    prop.Set(instance, &val);
                    return true;
                }
            }
            else
            {
                ImGui::TextDisabled("Unsupported ByValue Object Edit: %s", prop.Name.c_str());
            }
            break;
        }
        default:
            ImGui::TextDisabled("Unsupported Type: %s", prop.Name.c_str());
            break;
        }
        return false;
    }

    bool DrawReflectedObject(void* instance, const Reflection::ClassInfo* classInfo)
    {
        if (!classInfo || !instance)
            return false;
        bool changed = false;
        for (const auto& prop : classInfo->Properties)
        {
            changed = DrawProperty(prop, instance) || changed;
        }
        return changed;
    }

    bool DrawColliderAuthoring(Framework::Collider& collider)
    {
        bool changed = false;
        int shape = static_cast<int>(collider.GetShapeType());
        constexpr const char* ShapeNames[] = { "Box", "Sphere", "Capsule" };
        if (ImGui::Combo("Shape", &shape, ShapeNames, static_cast<int>(std::size(ShapeNames))))
        {
            collider.SetShapeType(static_cast<Physics::ColliderShapeType>(shape));
            changed = true;
        }

        Math::Vector3 center = collider.GetCenter();
        if (ImGui::DragFloat3("Center", &center.x, 0.05f))
        {
            collider.SetCenter(center);
            changed = true;
        }

        if (shape == static_cast<int>(Physics::ColliderShapeType::Box))
        {
            Math::Vector3 halfExtents = collider.GetHalfExtents();
            if (ImGui::DragFloat3("Half Extents", &halfExtents.x, 0.05f, 0.001f, 100000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
            {
                collider.SetHalfExtents(halfExtents);
                changed = true;
            }
        }
        else
        {
            float radius = collider.GetRadius();
            if (ImGui::DragFloat("Radius", &radius, 0.05f, 0.001f, 100000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
            {
                collider.SetRadius(radius);
                changed = true;
            }
            if (shape == static_cast<int>(Physics::ColliderShapeType::Capsule))
            {
                float height = collider.GetHeight();
                if (ImGui::DragFloat("Height", &height, 0.05f, 0.001f, 100000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
                {
                    collider.SetHeight(height);
                    changed = true;
                }
            }
        }

        bool trigger = collider.IsTrigger();
        if (ImGui::Checkbox("Is Trigger", &trigger))
        {
            collider.SetTrigger(trigger);
            changed = true;
        }
        bool queryEnabled = collider.IsQueryEnabled();
        if (ImGui::Checkbox("Query Enabled", &queryEnabled))
        {
            collider.SetQueryEnabled(queryEnabled);
            changed = true;
        }

        int layer = collider.GetLayer();
        if (ImGui::DragInt("Layer", &layer, 0.2f, 0, Physics::PHYSICS_LAYER_COUNT - 1, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            collider.SetLayer(layer);
            changed = true;
        }
        float friction = collider.GetFriction();
        if (ImGui::DragFloat("Friction", &friction, 0.01f, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
        {
            collider.SetFriction(friction);
            changed = true;
        }
        float restitution = collider.GetRestitution();
        if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
        {
            collider.SetRestitution(restitution);
            changed = true;
        }

        char profile[128];
        CopyToFixedBuffer(profile, collider.GetCollisionProfile());
        if (ImGui::InputText("Collision Profile", profile, sizeof(profile)))
        {
            collider.SetCollisionProfile(profile);
            changed = true;
        }
        char material[128];
        CopyToFixedBuffer(material, collider.GetMaterialName());
        if (ImGui::InputText("Physics Material", material, sizeof(material)))
        {
            collider.SetMaterialName(material);
            changed = true;
        }

        if (!collider.GetAuthoringDiagnostic().empty())
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "%s", collider.GetAuthoringDiagnostic().c_str());
        return changed;
    }

    bool DrawRigidbodyAuthoring(Framework::Rigidbody& rigidbody)
    {
        bool changed = false;
        int motion = static_cast<int>(rigidbody.GetMotionType());
        constexpr const char* MotionNames[] = { "Static", "Kinematic", "Dynamic" };
        if (ImGui::Combo("Motion Type", &motion, MotionNames, static_cast<int>(std::size(MotionNames))))
        {
            rigidbody.SetMotionType(static_cast<Physics::MotionType>(motion));
            changed = true;
        }

        float mass = rigidbody.GetMass();
        if (ImGui::DragFloat("Mass", &mass, 0.05f, 0.001f, 1000000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
        {
            rigidbody.SetMass(mass);
            changed = true;
        }
        float linearDamping = rigidbody.GetLinearDamping();
        if (ImGui::DragFloat("Linear Damping", &linearDamping, 0.01f, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
        {
            rigidbody.SetLinearDamping(linearDamping);
            changed = true;
        }
        float angularDamping = rigidbody.GetAngularDamping();
        if (ImGui::DragFloat("Angular Damping", &angularDamping, 0.01f, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
        {
            rigidbody.SetAngularDamping(angularDamping);
            changed = true;
        }
        float gravityFactor = rigidbody.GetGravityFactor();
        if (ImGui::DragFloat("Gravity Factor", &gravityFactor, 0.01f))
        {
            rigidbody.SetGravityFactor(gravityFactor);
            changed = true;
        }
        bool ccd = rigidbody.IsContinuousCollisionDetectionEnabled();
        if (ImGui::Checkbox("Continuous Collision Detection", &ccd))
        {
            rigidbody.SetContinuousCollisionDetectionEnabled(ccd);
            changed = true;
        }
        bool allowSleeping = rigidbody.IsSleepingAllowed();
        if (ImGui::Checkbox("Allow Sleeping", &allowSleeping))
        {
            rigidbody.SetSleepingAllowed(allowSleeping);
            changed = true;
        }

        int axisLockMask = rigidbody.GetAxisLockMask();
        if (ImGui::TreeNodeEx("Axis Locks", ImGuiTreeNodeFlags_DefaultOpen))
        {
            constexpr std::array<std::pair<const char*, int>, 6> AxisLocks{
                std::pair{ "Position X", static_cast<int>(Physics::PhysicsAxisLockTranslationX) },
                std::pair{ "Position Y", static_cast<int>(Physics::PhysicsAxisLockTranslationY) },
                std::pair{ "Position Z", static_cast<int>(Physics::PhysicsAxisLockTranslationZ) },
                std::pair{ "Rotation X", static_cast<int>(Physics::PhysicsAxisLockRotationX) },
                std::pair{ "Rotation Y", static_cast<int>(Physics::PhysicsAxisLockRotationY) },
                std::pair{ "Rotation Z", static_cast<int>(Physics::PhysicsAxisLockRotationZ) },
            };
            bool axisChanged = false;
            for (const auto& [label, bit] : AxisLocks)
            {
                bool locked = (axisLockMask & bit) != 0;
                if (ImGui::Checkbox(label, &locked))
                {
                    axisLockMask = locked ? axisLockMask | bit : axisLockMask & ~bit;
                    axisChanged = true;
                }
            }
            if (axisChanged)
            {
                rigidbody.SetAxisLockMask(axisLockMask);
                changed = true;
            }
            ImGui::TreePop();
        }

        if (!rigidbody.GetAuthoringDiagnostic().empty())
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "%s", rigidbody.GetAuthoringDiagnostic().c_str());
        return changed;
    }

    void InspectorPanel::DrawMeshRendererMaterialPanel(Framework::MeshRenderer& meshRenderer)
    {
        if (!ImGui::TreeNodeEx("Material", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        DrawMaterialReference(meshRenderer.GetMaterialReference());
        ImGui::TextDisabled(meshRenderer.HasRuntimeMaterialOverride() ? "Runtime material instance. Changes affect only this GameObject." : "Shared material preview. First edit creates a per-object runtime instance.");

        Render::Renderer* renderer = _context ? _context->renderer : nullptr;
        if (!renderer)
        {
            ImGui::TextDisabled("Renderer unavailable.");
            ImGui::TreePop();
            return;
        }

        Asset::AssetManager* assetManager = renderer->GetAssetManager();
        if (!assetManager)
        {
            ImGui::TextDisabled("AssetManager unavailable.");
            ImGui::TreePop();
            return;
        }

        if (meshRenderer.NeedsAssetResolve())
            meshRenderer.ResolveAssets(*assetManager);

        const Asset::MaterialHandle materialAsset = meshRenderer.GetMaterialAsset();
        if (!materialAsset.IsValid())
        {
            ImGui::TextDisabled("Material asset is not resolved.");
            ImGui::TreePop();
            return;
        }

        const uint32_t materialAssetId = materialAsset.raw_value;
        Resource::MaterialHandle sharedMaterial = Resource::MaterialHandle::Invalid();
        if (!_failedMaterialUploads.contains(materialAssetId))
        {
            sharedMaterial = renderer->GetOrUploadMaterial(materialAsset);
            if (!sharedMaterial.IsValid())
                _failedMaterialUploads.insert(materialAssetId);
        }

        if (!sharedMaterial.IsValid())
        {
            ImGui::TextDisabled("Material resource upload failed.");
            if (ImGui::Button("Retry Material Upload"))
                _failedMaterialUploads.erase(materialAssetId);
            ImGui::TreePop();
            return;
        }
        _failedMaterialUploads.erase(materialAssetId);

        Resource::MaterialHandle material = meshRenderer.GetRuntimeMaterialOverride();
        if (!material.IsValid())
            material = sharedMaterial;

        const std::vector<Resource::MaterialParameterInfo> parameters = renderer->GetMaterialParameters(material);
        if (parameters.empty())
        {
            ImGui::TextDisabled("No editable material parameters.");
            ImGui::TreePop();
            return;
        }

        if (!_materialEditError.empty())
            ImGui::TextDisabled("%s", _materialEditError.c_str());

        for (const Resource::MaterialParameterInfo& parameter : parameters)
        {
            ImGui::PushID(parameter.name.c_str());

            std::vector<float> values = parameter.value;
            ImGui::SetNextItemWidth(-72.0f);
            bool changed = DrawMaterialParameterControl(parameter, values);

            ImGui::SameLine();
            if (ImGui::SmallButton("Reset"))
            {
                values = parameter.defaultValue;
                values.resize(parameter.componentCount, 0.0f);
                changed = true;
            }

            if (changed)
            {
                if (!meshRenderer.HasRuntimeMaterialOverride())
                {
                    const Resource::MaterialHandle instance = renderer->CreateMaterialInstance(sharedMaterial);
                    if (!instance.IsValid())
                    {
                        _materialEditError = "Failed to create runtime material instance.";
                        ImGui::PopID();
                        continue;
                    }
                    meshRenderer.SetRuntimeMaterialOverride(instance);
                    material = instance;
                }

                Resource::MaterialParameterValue value{
                    .type = parameter.type,
                    .value = std::move(values),
                };
                if (renderer->SetMaterialParameter(material, parameter.name, value))
                    _materialEditError.clear();
                else
                    _materialEditError = "Failed to update material parameter: " + parameter.name;
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    void InspectorPanel::OnImGuiRender()
    {
        ImGui::Begin(GetName().c_str(), &_isActive);

        if (_context->selectedGameObject != 0 && _context->activeScene)
        {
            auto* scene = _context->activeScene;
            auto* go = scene->GetGameObject(_context->selectedGameObject);
            if (go)
            {
                // 1. GameObject 基础属性
                char nameBuf[256];
                CopyToFixedBuffer(nameBuf, go->GetName());
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                {
                    go->SetName(nameBuf);
                    _context->isDirty = scene->IsEditing();
                }

                ImGui::TextDisabled("UID: %llu", go->GetID());

                bool active = go->IsActive();
                if (ImGui::Checkbox("Active", &active))
                {
                    go->SetActive(active);
                    _context->isDirty = scene->IsEditing();
                }
                ImGui::Separator();

                const auto* goClassInfo = Reflection::TypeRegister::Instance().GetClassByName("GameObject");
                if (goClassInfo)
                {
                    if (DrawReflectedObject(go, goClassInfo))
                        _context->isDirty = scene->IsEditing();
                }

                const auto& components = go->GetAllComponents();
                Framework::Component* componentToRemove = nullptr;
                for (const auto& comp : components)
                {
                    std::string compTypeName = comp->GetReflectedClassName();
                    const auto* compClassInfo = Reflection::TypeRegister::Instance().GetClassByName(compTypeName);

                    if (compClassInfo)
                    {
                        auto shortName = compTypeName.substr(compTypeName.find_last_of(":") + 1);
                        if (ImGui::CollapsingHeader(shortName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::PushID(comp.get());
                            bool authoringPanelHandledDirty = false;
                            bool componentChanged = false;
                            if (auto* collider = dynamic_cast<Framework::Collider*>(comp.get()))
                            {
                                componentChanged = DrawColliderAuthoring(*collider);
                                authoringPanelHandledDirty = true;
                            }
                            else if (auto* rigidbody = dynamic_cast<Framework::Rigidbody*>(comp.get()))
                            {
                                componentChanged = DrawRigidbodyAuthoring(*rigidbody);
                                authoringPanelHandledDirty = true;
                            }
                            else
                                componentChanged = DrawReflectedObject(comp.get(), compClassInfo);
                            if (componentChanged)
                            {
                                if (!authoringPanelHandledDirty)
                                    comp->MarkDirty();
                                _context->isDirty = scene->IsEditing();
                            }
                            if (auto* meshRenderer = dynamic_cast<Framework::MeshRenderer*>(comp.get()))
                                DrawMeshRendererMaterialPanel(*meshRenderer);
                            if (comp.get() != go->transform && ImGui::Button("Remove Component"))
                                componentToRemove = comp.get();
                            ImGui::PopID();
                        }
                    }
                }
                if (componentToRemove)
                {
                    go->RemoveComponent(componentToRemove);
                    _context->isDirty = scene->IsEditing();
                }

                ImGui::Spacing();
                if (ImGui::Button("Add Component", ImVec2(-1, 0)))
                    ImGui::OpenPopup("AddComponentPopup");
                if (ImGui::BeginPopup("AddComponentPopup"))
                {
                    if (ImGui::MenuItem("MeshRenderer"))
                    {
                        go->AddComponent<Framework::MeshRenderer>();
                        _context->isDirty = scene->IsEditing();
                    }
                    if (ImGui::MenuItem("Animator"))
                    {
                        go->AddComponent<Framework::Animator>();
                        _context->isDirty = scene->IsEditing();
                    }
                    if (ImGui::MenuItem("Light"))
                    {
                        go->AddComponent<Framework::LightComponent>();
                        _context->isDirty = scene->IsEditing();
                    }
                    if (ImGui::MenuItem("Collider", nullptr, false, go->GetComponent<Framework::Collider>() == nullptr))
                    {
                        go->AddComponent<Framework::Collider>();
                        _context->isDirty = scene->IsEditing();
                    }
                    if (ImGui::MenuItem("Rigidbody", nullptr, false, go->GetComponent<Framework::Rigidbody>() == nullptr))
                    {
                        go->AddComponent<Framework::Rigidbody>();
                        _context->isDirty = scene->IsEditing();
                    }
                    if (ImGui::MenuItem("ScriptComponent"))
                    {
                        go->AddComponent<Framework::ScriptComponent>();
                        _context->isDirty = scene->IsEditing();
                    }
                    ImGui::EndPopup();
                }

                if (ImGui::Button("Destroy GameObject", ImVec2(-1, 0)))
                {
                    scene->DestroyGameObject(go->GetID());
                    _context->selectedGameObject = Core::InvalidGameObjectID;
                    _context->isDirty = scene->IsEditing();
                }
            }
        }
        else
        {
            ImGui::TextDisabled("No GameObject Selected");
        }

        ImGui::End();
    }
} // namespace ChikaEngine::Editor
