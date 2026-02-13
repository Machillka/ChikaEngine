import sys
import os
import argparse
from clang.cindex import Index
from clang.cindex import CursorKind, TypeKind
from clang.cindex import CompilationDatabase
from jinja2 import Template
from gen_id import get_unique_id


reflect_map = {
    "int": "Int",
    "float": "Float",
    "bool": "Bool",
    "std::string": "String",
    "Math::Vector3": "Vector3",
    "Math::Quaternion": "Quaternion",
}


def type_to_reflect(t):
    if t in reflect_map:
        return reflect_map[t]
    return "Unknown"


class ReflectionContext:
    """反射上下文定义"""

    def __init__(self, build_dir) -> None:
        self.abs_build_dir = os.path.abspath(build_dir)
        self.index = Index.create()

        # 加载 commands
        try:
            self.compdb = CompilationDatabase.fromDirectory(self.abs_build_dir)
        except Exception:
            print(f"[Error] 无法在 {self.abs_build_dir} 找到 compile_commands.json")
            sys.exit(1)
        # 加载 CMake 导出的系统路径
        self.system_includes = self._load_system_includes()
        self.is_msvc = self._check_is_msvc()

    def _load_system_includes(self):
        """读取cmake生成的系统读取C++路径"""
        path = os.path.join(self.abs_build_dir, "system_includes.txt")
        print(f"Path:{path}")
        includes = []

        if os.path.exists(path):
            print("Mother fucker")
            with open(path, "r", encoding="utf-8") as f:
                content = f.read().strip()
                # CMake 的列表通常以分号分隔
                raw_paths = content.split(";")
                for p in raw_paths:
                    if p:
                        # 转换为 libclang 需要的 -isystem 参数
                        includes.append("-isystem")
                        includes.append(p)
        else:
            print("[Warning] 未找到 system_includes.txt")
        return includes

    def _check_is_msvc(self):
        path = os.path.join(self.abs_build_dir, "compiler_info.txt")
        if os.path.exists(path):
            with open(path, "r") as f:
                return "MSVC" in f.read()
        return os.name == "nt"

    def clean_compile_args(self, args):
        """
        清洗从 compile_commands.json 获取的参数。
        移除编译器指令、输出文件、源文件和 --
        """
        clean_args = []
        skip_next = False

        for i, arg in enumerate(args):
            if skip_next:
                skip_next = False
                continue

            if arg.endswith("clang++") or arg.endswith("g++") or arg.endswith("cl.exe"):
                continue

            if arg == "-c":
                continue

            # 4. 移除 '-o' 及其后的文件名
            if arg == "-o":
                skip_next = True
                continue

            if arg == "--":
                skip_next = True
                continue

            # 移除文件本身 (.cpp, .c, .cc)
            if arg.endswith((".cpp", ".c", ".cc", ".h", ".hpp")):
                continue

            # 保留有效参数
            clean_args.append(arg)

        return clean_args

    def get_args_for_file(self, filename):
        """
        做 args 的清理, 把 commands + system includes + platform
        """
        abs_file = os.path.abspath(filename)
        cmds = self.compdb.getCompileCommands(abs_file)

        if not cmds:
            print(f"[Warning] {filename} 不在编译数据库中")
            return []

        # 提取数据 去除头(编译器) 和末尾(源文件名字)
        raw_args = list(cmds[0].arguments)[1:-1]
        cleaned_args = self.clean_compile_args(raw_args)
        # 组合最终参数
        final_args = cleaned_args + self.system_includes

        # Windows/MSVC 必须加的兼容性参数
        if self.is_msvc:
            final_args.extend(
                [
                    "-fms-compatibility",
                    "-fms-extensions",
                    "-Wno-microsoft-enum-value",  # 忽略一些 MSVC 特有的警告
                ]
            )

        return final_args


class CodeRenderer:
    def __init__(self, template_path) -> None:
        with open(template_path, "r") as f:
            self.template = Template(f.read())

    def render(self, header_path, classes, output_path, unique_id):
        print("Rendering")
        codes = self.template.render(
            header_path=header_path, classes=classes, unique_id=unique_id
        )
        with open(output_path, "w") as f:
            f.write(codes)


def get_relative_header_path(absolute_path, engine_root):
    """
    把绝对路径转化成"相对路径"保证头文件引用正确
    e.g
    将 /.../ChikaEngine/engine/framework/temp/fuck.h
    转换为 ChikaEngine/temp/fuck.h
    """
    abs_path = os.path.abspath(absolute_path).replace("\\", "/")

    # 寻找路径中的 "include/" 标记
    marker = "/include/"
    if marker in abs_path:
        # 分割路径，取 include/ 之后的部分
        parts = abs_path.split(marker)
        return parts[-1]  # 返回 ChikaEngine/xxx/xxx.h

    # 对于其他情况
    base_dir = os.path.join(engine_root, "engine").replace("\\", "/")
    try:
        return os.path.relpath(abs_path, base_dir).replace("\\", "/")
    except ValueError:
        return absolute_path


def get_clean_type_name(cursor_type):
    """
    传入当前的节点的type类型
    获得化简后的 type
    """
    # Canonical Type (去除 typedef 别名，但保留基础结构)
    canonical = cursor_type.get_canonical()

    # 2. 原始拼写
    spelling = cursor_type.spelling

    # 无效类型 ( 被解析成 int
    if cursor_type.kind == TypeKind.INT and "int" not in spelling:  # type: ignore
        return "UNKNOWN_TYPE_ERROR"

    # 4. STL 容器清洗逻辑
    # 原因是 string 可能会作为别名被解析
    if "std::" in spelling or "std::" in canonical.spelling:
        # 处理 string
        if "string" in spelling and "std::" in spelling:
            return "std::string"

        # TODO: 递归清理 vector 等容器 (虽然大概率用不到)
    return spelling


def visit(node, depth=0):
    print("  " * depth, node.kind, node.spelling)
    for child in node.get_children():
        visit(child, depth + 1)


def process_params(node):
    params = []
    for child in node.get_children():
        if child.kind == CursorKind.PARM_DECL:  # type: ignore
            params.append(
                {"name": child.spelling, "type": get_clean_type_name(child.type)}
            )
    return params


def process_class(node, namespace):
    data = {
        "name": node.spelling,
        "namespace": namespace,
        "fields": [],
        "functions": [],
        "full_name": namespace + "::" + node.spelling,
    }
    for child in node.get_children():
        if child.kind == CursorKind.FIELD_DECL and is_reflected_field(child):  # type: ignore
            data["fields"].append(
                {
                    "name": child.spelling,
                    "raw_type": get_clean_type_name(child.type),
                    "reflect_type": type_to_reflect(get_clean_type_name(child.type)),
                }
            )
        if child.kind == CursorKind.CXX_METHOD and is_reflected_function(child):  # type: ignore
            params = process_params(child)
            data["functions"].append(
                {
                    "name": child.spelling,
                    "return": get_clean_type_name(child.type),
                    "params": params,
                }
            )
    return data


def is_reflected_class(node) -> bool:
    for child in node.get_children():
        if child.kind == CursorKind.ANNOTATE_ATTR and child.spelling == "reflect-class":  # pyright: ignore[reportAttributeAccessIssue]
            return True
    return False


def is_reflected_field(node) -> bool:
    for child in node.get_children():
        if child.kind == CursorKind.ANNOTATE_ATTR and child.spelling == "reflect-field":  # pyright: ignore[reportAttributeAccessIssue]
            return True
    return False


def is_reflected_function(node) -> bool:
    for child in node.get_children():
        if (
            child.kind == CursorKind.ANNOTATE_ATTR  # pyright: ignore[reportAttributeAccessIssue]
            and child.spelling == "reflect-function"
        ):
            return True
    return False


def is_defined_in_current_file(node, current_file_path):
    if not node.location.file:
        return False
    # 转换为绝对路径进行比较
    node_file = os.path.abspath(node.location.file.name).replace("\\", "/")
    current_file = os.path.abspath(current_file_path).replace("\\", "/")
    return node_file == current_file


def find_reflection(node, current_file_path, namespace="", res=None):
    if res is None:
        res = []
    if node.kind == CursorKind.NAMESPACE:  # pyright: ignore[reportAttributeAccessIssue]
        # TODO[x]: 取消绝对路径
        if namespace == "":
            namespace = node.spelling
        else:
            namespace += "::" + node.spelling
    if node.kind == CursorKind.CLASS_DECL and node.is_definition():  # pyright: ignore[reportAttributeAccessIssue]
        # 仅对当前文件定义的类型进行反射代码生成 防止重复 —— 因为 include 的直接 copy
        if is_reflected_class(node) and is_defined_in_current_file(
            node, current_file_path
        ):
            class_info = process_class(node, namespace)
            res.append(class_info)
    # 递归查找其他 class
    for child in node.get_children():
        find_reflection(child, current_file_path, namespace, res)
    return res


def read():
    parser = argparse.ArgumentParser(description="ChikaReflect Parser")

    parser.add_argument(
        "--input", type=str, required=True, help="Path to the input file"
    )
    parser.add_argument(
        "--output", type=str, required=True, help="Path to the output file"
    )
    parser.add_argument(
        "--build-dir", type=str, required=True, help="Path to the build directory"
    )
    parser.add_argument(
        "--engine-root", type=str, required=True, help="Path to the engine directory"
    )

    args = parser.parse_args()
    file_path = args.input
    output_path = args.output
    build_dir = args.build_dir
    engine_root = args.engine_root

    print(f"args: --input = {file_path}, output = {output_path}, build = {build_dir}")

    source_file = file_path
    ctx = ReflectionContext(build_dir)
    args = ctx.get_args_for_file(source_file)
    args.append("-D__REFLECTION_PARSER__")

    tu = ctx.index.parse(source_file, args=args)

    found_fatal = False
    for diag in tu.diagnostics:
        if diag.severity >= 3:  # Error or Fatal
            print(f"[Clang Error] {diag.spelling} (Line {diag.location.line})")
            found_fatal = True
    res = []
    if found_fatal:
        print("解析过程中出现错误")
    else:
        print("--- 解析成功，提取数据ing ---")
        res = find_reflection(tu.cursor, source_file)
        print(res)

    if res:
        code_render = CodeRenderer("templates/reflection.cpp.j2")
        unique_id = get_unique_id(source_file, engine_root)
        code_render.render(
            get_relative_header_path(source_file, engine_root),
            res,
            output_path,
            unique_id,
        )

    # visit(tu.cursor)


def main():
    read()


if __name__ == "__main__":
    main()
