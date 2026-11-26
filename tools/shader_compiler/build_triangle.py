"""用 glslc 生成窗口绘制样例及 GPU 测试共用的 SPIR-V 夹具。"""

import argparse
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SHADERS = ROOT / "assets/shaders"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--glslc", default="glslc")
    args = parser.parse_args()
    lines = ["// 此文件由 tools/shader_compiler/build_triangle.py 生成。",
             "#pragma once", "", "#include <cstdint>", "", "namespace voryx::sample_data", "{"]
    shaders = (("triangle.vert", "triangleVertexSpirv"),
               ("triangle.frag", "triangleFragmentSpirv"),
               ("triangle_alpha.frag", "triangleAlphaFragmentSpirv"),
               ("textured.vert", "texturedVertexSpirv"),
               ("textured.frag", "texturedFragmentSpirv"),
               ("depth.vert", "depthVertexSpirv"),
               ("resource.vert", "resourceVertexSpirv"),
               ("resource.frag", "resourceFragmentSpirv"),
               ("resource_depth.vert", "resourceDepthVertexSpirv"))
    for source, name in shaders:
        binary = subprocess.run([args.glslc, "--target-env=vulkan1.2", "-O",
                                 str(SHADERS / source), "-o", "-"],
                                check=True, stdout=subprocess.PIPE).stdout
        words = struct.unpack(f"<{len(binary) // 4}I", binary)
        lines.append(f"inline constexpr std::uint32_t {name}[] = {{")
        for offset in range(0, len(words), 6):
            lines.append("    " + ", ".join(f"0x{word:08x}u" for word in words[offset:offset + 6]) + ",")
        lines.extend(["};", ""])
    lines.append("} // namespace voryx::sample_data")
    destination = SHADERS / "generated/triangle_spirv.hpp"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
