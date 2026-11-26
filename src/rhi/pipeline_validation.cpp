#include "pipeline_validation.hpp"
#include "binding_validation.hpp"
#include <algorithm>
#include <array>

namespace voryx::rhi_detail
{
namespace
{
// 只读取声明绑定所需的 SPIR-V 指令，完整模块由离线工具验证。
enum Opcode : std::uint32_t
{
    EntryPoint = 15,
    TypeFloat = 22,
    TypeImage = 25,
    TypeSampler = 26,
    TypeStruct = 30,
    TypePointer = 32,
    Variable = 59,
    Decorate = 71,
};
enum Storage : std::uint32_t
{
    UniformConstant = 0,
    Uniform = 2,
    PushConstant = 9,
    StorageBuffer = 12,
    PhysicalStorageBuffer = 5349,
};
enum Decoration : std::uint32_t
{
    Block = 2,
    BufferBlock = 3,
    Binding = 33,
    DescriptorSet = 34
};
using Words = std::span<const std::uint32_t>;

template<typename Match>
Words FindInstruction(Words words, Opcode opcode, Match match) noexcept
{
    for (std::size_t index = 5; index < words.size();) {
        const auto count = words[index] >> 16;
        if ((words[index] & 0xffffu) == opcode) {
            const auto inst = words.subspan(index, count);
            if (match(inst)) {
                return inst;
            }
        }
        index += count;
    }
    return {};
}

Words Declaration(Words words, Opcode opcode, std::uint32_t id) noexcept
{
    return FindInstruction(words, opcode, [id](Words inst) {
        return inst.size() >= 2 && inst[1] == id;
    });
}
Words Attribute(Words words, std::uint32_t id, Decoration decoration) noexcept
{
    return FindInstruction(words, Decorate, [id, decoration](Words inst) {
        return inst.size() >= 3 && inst[1] == id && inst[2] == decoration;
    });
}
bool MatchesType(Words words, Words variable, BindingType type) noexcept
{
    const auto pointer = Declaration(words, TypePointer, variable[1]);
    if (pointer.size() != 4 || pointer[2] != variable[3]) {
        return false;
    }
    const auto id = pointer[3];
    if (type == BindingType::UniformBuffer) {
        return variable[3] == Uniform
               && !Declaration(words, TypeStruct, id).empty()
               && !Attribute(words, id, Block).empty()
               && Attribute(words, id, BufferBlock).empty();
    }
    if (variable[3] != UniformConstant) {
        return false;
    }
    if (type == BindingType::Sampler) {
        return Declaration(words, TypeSampler, id).size() == 2;
    }
    const auto image = Declaration(words, TypeImage, id);
    // Dim=2D、非深度比较、非数组、非多重采样、采样图像、格式未知。
    if (image.size() != 9 || image[3] != 1 || image[4] != 0 || image[5] != 0
        || image[6] != 0 || image[7] != 1 || image[8] != 0) {
        return false;
    }
    const auto component = Declaration(words, TypeFloat, image[2]);
    return component.size() == 3 && component[2] == 32;
}

bool ValidShader(Words words, std::uint32_t stage, BindingVisibility visibility,
                 std::span<const BindingLayoutEntry> layout) noexcept
{
    if (words.size() <= 5 || words.size() > 262144 || words[0] != 0x07230203u
        || words[1] < 0x00010000u || words[1] > 0x00010500u || words[3] == 0
        || words[4] != 0) {
        return false;
    }
    bool entry = false;
    std::array<Words, maxBindingSlots> variables{};
    std::size_t variableCount = 0;
    for (std::size_t index = 5; index < words.size();) {
        const auto count = words[index] >> 16;
        const auto opcode = words[index] & 0xffffu;
        if (count == 0 || count > words.size() - index) {
            return false;
        }
        if (opcode == Variable) {
            if (count < 4) {
                return false;
            }
            const auto storage = words[index + 3];
            if (storage == PushConstant || storage == StorageBuffer
                || storage == PhysicalStorageBuffer) {
                return false;
            }
            if (storage == Uniform || storage == UniformConstant) {
                if (variableCount >= layout.size()) {
                    return false;
                }
                variables[variableCount++] = words.subspan(index, count);
            }
        }
        if (opcode == EntryPoint) {
            // OpEntryPoint 必须只有一个 main，且执行模型与描述中的阶段相符。
            if (entry || count < 5 || words[index + 1] != stage
                || words[index + 2] == 0 || words[index + 2] >= words[3]
                || words[index + 3] != 0x6e69616du || words[index + 4] != 0) {
                return false;
            }
            entry = true;
        }
        index += count;
    }
    std::uint32_t slots = 0;
    for (const auto variable : std::span(variables).first(variableCount)) {
        const auto set = Attribute(words, variable[2], DescriptorSet);
        const auto binding = Attribute(words, variable[2], Binding);
        if (set.size() != 4 || set[3] != 0 || binding.size() != 4
            || binding[3] >= 32 || (slots & (1u << binding[3])) != 0) {
            return false;
        }
        slots |= 1u << binding[3];
        const auto slot =
            std::ranges::find(layout, binding[3], &BindingLayoutEntry::slot);
        if (slot == layout.end()
            || (static_cast<unsigned>(slot->visibility)
                & static_cast<unsigned>(visibility))
                   == 0
            || !MatchesType(words, variable, slot->type)) {
            return false;
        }
    }
    return entry;
}
} // namespace

bool ValidGraphicsPipeline(const GraphicsPipelineDesc &desc,
                           std::span<const BindingLayoutEntry> layout) noexcept
{
    if (!ValidShader(desc.vertexSpirv, 0, BindingVisibility::Vertex, layout)
        || !ValidShader(desc.fragmentSpirv, 4, BindingVisibility::Fragment,
                        layout)
        || desc.colorFormat > PipelineColorFormat::BGRA8_UNorm
        || desc.cullMode > CullMode::Back
        || desc.frontFace > FrontFace::Clockwise
        || desc.blendMode > BlendMode::PremultipliedAlpha
        || desc.depthCompare > DepthCompare::Always
        || (desc.depthWrite && !desc.depthTest)
        || desc.vertexAttributes.size() > 16 || desc.vertexStrideBytes > 2048
        || (desc.vertexAttributes.empty() != (desc.vertexStrideBytes == 0))) {
        return false;
    }
    std::uint32_t locations = 0;
    for (const auto &attribute : desc.vertexAttributes) {
        if (attribute.location >= 16 || attribute.format > VertexFormat::Float4
            || attribute.offsetBytes % 4 != 0
            || desc.vertexStrideBytes % 4 != 0) {
            return false;
        }
        const std::array<std::uint32_t, 3> sizes{8, 12, 16};
        const auto size = sizes[static_cast<std::size_t>(attribute.format)];
        if (attribute.offsetBytes > desc.vertexStrideBytes
            || size > desc.vertexStrideBytes - attribute.offsetBytes
            || (locations & (1u << attribute.location)) != 0) {
            return false;
        }
        locations |= 1u << attribute.location;
    }
    return true;
}
} // namespace voryx::rhi_detail
