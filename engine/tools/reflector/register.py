import argparse
import os
from jinja2 import Template
from gen_id import get_unique_id


def main():
    parser = argparse.ArgumentParser(description="ChikaEngine Registry Generator")
    parser.add_argument("--engine-root", required=True, help="Root path of the engine")
    parser.add_argument("--output", required=True, help="Output .cpp file path")
    parser.add_argument("--template", required=True, help="Path to registry.cpp.j2")
    # 接收任意数量的输入头文件
    parser.add_argument(
        "--headers",
        nargs="*",
        default=[],
        help="List of header files containing reflection",
    )

    args = parser.parse_args()

    # 1. 计算所有的锚点函数名
    anchor_functions = []
    # 简单的去重排序，保证生成的代码稳定，不会因为文件顺序变化导致重新编译
    sorted_headers = sorted(list(set(args.headers)))

    for header in sorted_headers:
        if header.strip():
            func_name = get_unique_id(header, args.engine_root)
            anchor_functions.append(func_name)

    # 2. 加载 Jinja2 模板
    with open(args.template, "r", encoding="utf-8") as f:
        template_content = f.read()
        template = Template(template_content)

    # 3. 渲染代码
    print(f"[Registry] Linking {len(anchor_functions)} reflection units...")
    rendered_code = template.render(anchor_functions=anchor_functions)

    # 4. 写入文件 (只有内容变化时才写入，避免触发不必要的重编译)
    needs_write = True
    if os.path.exists(args.output):
        with open(args.output, "r", encoding="utf-8") as f:
            if f.read() == rendered_code:
                needs_write = False

    if needs_write:
        # 确保目录存在
        os.makedirs(os.path.dirname(args.output), exist_ok=True)
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(rendered_code)


if __name__ == "__main__":
    main()
