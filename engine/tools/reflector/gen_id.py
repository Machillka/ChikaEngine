import os


def get_unique_id(file_path, engine_root):
    """
    [关键] 计算唯一 ID
    """
    try:
        # 转为相对于 Engine Root 的路径，生成的 ID 一致
        # 例如: engine/framework/Temp.h
        rel_path = os.path.relpath(file_path, engine_root)
    except ValueError:
        # 如果不在 engine_root 下 (比如外部插件)，就用原路径
        rel_path = file_path

    # 替换所有非 C++ 标识符字符
    safe_name = (
        rel_path.replace("/", "_")
        .replace("\\", "_")
        .replace(".", "_")
        .replace("-", "_")
    )

    return f"ChikaReflect_Link_{safe_name}"
