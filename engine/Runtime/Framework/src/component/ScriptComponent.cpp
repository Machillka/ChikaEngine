#include "ChikaEngine/component/ScriptComponent.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"
namespace py = pybind11;
namespace ChikaEngine::Framework
{
    ScriptComponent::~ScriptComponent()
    {
        _pythonInstance = py::none();
    }

    void ScriptComponent::Awake()
    {
        if (moduleName.empty() || className.empty())
            return;
        try
        {
            // 动态导入 Python 模块
            py::module_ mod = py::module_::import(moduleName.c_str());
            // 获取模块中的类
            py::object pyClass = mod.attr(className.c_str());
            // 实例化对象
            _pythonInstance = pyClass();
            _isLoaded = true;

            _pythonInstance.attr("owner") = py::cast(GetOwner());

            // 尝试调用 Python 的 awake
            if (py::hasattr(_pythonInstance, "awake"))
            {
                _pythonInstance.attr("awake")();
            }
        }
        catch (py::error_already_set& e)
        {
            LOG_ERROR("Python", "Failed to load script {}.{}: {}", moduleName, className, e.what());
            _isLoaded = false;
        }
    }

    void ScriptComponent::Start()
    {
        Component::Start();
        if (_isLoaded && py::hasattr(_pythonInstance, "start"))
        {
            try
            {
                _pythonInstance.attr("start")();
            }
            catch (py::error_already_set& e)
            {
                LOG_ERROR("Python", "{}", e.what());
            }
        }
    }

    void ScriptComponent::Update(float deltaTime)
    {
        if (_isLoaded && py::hasattr(_pythonInstance, "update"))
        {
            try
            {
                _pythonInstance.attr("update")(deltaTime);
            }
            catch (py::error_already_set& e)
            {
                LOG_ERROR("Python", "{}", e.what());
            }
        }
    }

    // TODO: 绑定 其他生命周期函数

} // namespace ChikaEngine::Framework