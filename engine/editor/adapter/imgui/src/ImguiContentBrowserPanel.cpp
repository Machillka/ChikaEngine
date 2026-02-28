#include "include/ChikaEngine/ImguiContentBrowserPanel.h"
#include "ChikaEngine/ResourceSystem.h"
#include "ChikaEngine/debug/log_macros.h"
#include "imgui.h"
namespace fs = std::filesystem;
namespace ChikaEngine::Editor
{

    ImguiContentBrowserPanel::ImguiContentBrowserPanel()
    {
        _currentDirectory = Resource::ResourceSystem::Instance().GetAssetRoot();
    }

    void ImguiContentBrowserPanel::OnRender(UIContext& ctx)
    {
        ImGui::Begin(Name());

        // 顶层导航栏
        if (_currentDirectory.empty() || !fs::exists(_currentDirectory))
            _currentDirectory = Resource::ResourceSystem::Instance().GetAssetRoot();

        if (_currentDirectory != fs::path(Resource::ResourceSystem::Instance().GetAssetRoot()))
        {
            if (ImGui::Button("<- Back"))
            {
                _currentDirectory = _currentDirectory.parent_path();
            }
            ImGui::SameLine();
        }
        ImGui::Text("%s", _currentDirectory.string().c_str());
        ImGui::Separator();

        // 渲染文件夹和文件
        static float padding = 16.0f;
        static float thumbnailSize = 64.0f;
        float cellSize = thumbnailSize + padding;

        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = (int)(panelWidth / cellSize);
        if (columnCount < 1)
            columnCount = 1;

        ImGui::Columns(columnCount, 0, false);

        for (auto& directoryEntry : fs::directory_iterator(_currentDirectory))
        {
            const auto& path = directoryEntry.path();
            std::string filenameString = path.filename().string();

            ImGui::PushID(filenameString.c_str());

            // 区分文件夹和文件 (可以用图标，这里用文字颜色区分)
            if (directoryEntry.is_directory())
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
                if (ImGui::Button(filenameString.c_str(), {thumbnailSize, thumbnailSize}))
                {
                    _currentDirectory /= path.filename();
                }
                ImGui::PopStyleColor();
            }
            else
            {
                // TODO: 打包 icon
                // 如果是 Python 文件变绿色
                bool isPython = path.extension() == ".py";
                if (isPython)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));

                ImGui::Button(filenameString.c_str(), {thumbnailSize, thumbnailSize});

                if (isPython)
                    ImGui::PopStyleColor();

                // TODO: 拖拽逻辑 (Drag and Drop) 绑定到 Inspector
                if (ImGui::BeginDragDropSource())
                {
                    std::string relativePath = fs::relative(path, Resource::ResourceSystem::Instance().GetAssetRoot()).string();
                    ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", relativePath.c_str(), (relativePath.size() + 1) * sizeof(char));
                    ImGui::Text("Drag %s", filenameString.c_str());
                    ImGui::EndDragDropSource();
                }
            }

            ImGui::TextWrapped("%s", filenameString.c_str());
            ImGui::NextColumn();
            ImGui::PopID();
        }
        ImGui::Columns(1);

        // 右键上下文菜单 (新建脚本)
        if (ImGui::BeginPopupContextWindow("ContentBrowserContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::BeginMenu("Create"))
            {
                if (ImGui::MenuItem("Python Script"))
                {
                    ImGui::OpenPopup("Create Python Script");
                }
                ImGui::EndMenu();
            }

            // 弹出命名对话框
            if (ImGui::BeginPopupModal("Create Python Script", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::InputText("Class Name", _newScriptName, IM_ARRAYSIZE(_newScriptName));
                if (ImGui::Button("Create", ImVec2(120, 0)))
                {
                    std::string relDir = fs::relative(_currentDirectory, Resource::ResourceSystem::Instance().GetAssetRoot()).string();
                    if (relDir == ".")
                        relDir = ""; // 根目录处理

                    LOG_DEBUG("Imgui", "Creating a script");
                    // Resource::ResourceSystem::Instance().CreatePythonScriptTemplate(relDir, _newScriptName);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::End();
    }

} // namespace ChikaEngine::Editor