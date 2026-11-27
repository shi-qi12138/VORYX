#include "opengl_pipeline.hpp"

#include "opengl_api.hpp"
#include "opengl_context.hpp"
#include "opengl_failure.hpp"

#include <algorithm>
#include <string>

namespace voryx::opengl_backend
{
namespace
{
constexpr unsigned int vertexShader = 0x8B31;
constexpr unsigned int fragmentShader = 0x8B30;
constexpr unsigned int compileStatus = 0x8B81;
constexpr unsigned int linkStatus = 0x8B82;
constexpr unsigned int infoLogLength = 0x8B84;
constexpr unsigned int activeUniforms = 0x8B86;
constexpr unsigned int activeUniformBlocks = 0x8A36;

template<typename Query, typename GetLog>
void Verify(const Api &api, unsigned int object, unsigned int status,
            Query query, GetLog getLog, std::string_view phase)
{
    int succeeded = 0;
    query(object, status, &succeeded);
    api.Check();
    if (succeeded != 0) {
        return;
    }

    int length = 0;
    query(object, infoLogLength, &length);
    std::string message(phase);
    if (length > 1) {
        std::string log(static_cast<std::size_t>(std::min(length, 8192)), '\0');
        int written = 0;
        getLog(object, static_cast<int>(log.size()), &written, log.data());
        log.resize(static_cast<std::size_t>(written));
        message.append(": ").append(log);
    }
    api.Check();
    throw Failure{ErrorCode::InvalidArgument, {}, std::move(message)};
}

class Shader
{
public:
    Shader(const Api &api, unsigned int stage) noexcept
        : api(api)
        , name(api.createShader(stage))
    {
    }

    ~Shader()
    {
        if (name != 0) {
            api.deleteShader(name);
        }
    }

    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;

    void Compile(std::string_view source, std::string_view phase) const
    {
        api.Check();
        const char *text = source.data();
        const auto length = static_cast<int>(source.size());
        api.shaderSource(name, 1, &text, &length);
        api.compileShader(name);
        Verify(api, name, compileStatus, api.getShaderiv, api.getShaderInfoLog,
               phase);
    }

    unsigned int Name() const noexcept
    {
        return name;
    }

private:
    const Api &api;
    const unsigned int name;
};
} // namespace

Pipeline::Pipeline(Context &context, const Api &api,
                   const GraphicsPipelineDesc &desc)
    : context(context)
    , api(api)
    , attributes(desc.vertexAttributes.begin(), desc.vertexAttributes.end())
    , stride(desc.vertexStrideBytes)
{
}

Pipeline::~Pipeline()
{
    if (program != 0) {
        try {
            const Context::Current current(context);
            api.deleteProgram(program);
        } catch (const Failure &) {
            // 无法激活时由上下文销毁回收程序。
        }
    }
}

void Pipeline::Link(std::string_view vertex, std::string_view fragment)
{
    const Shader vertexObject(api, vertexShader);
    vertexObject.Compile(vertex, "GLSL vertex compilation");
    const Shader fragmentObject(api, fragmentShader);
    fragmentObject.Compile(fragment, "GLSL fragment compilation");

    program = api.createProgram();
    api.Check();
    api.attachShader(program, vertexObject.Name());
    api.attachShader(program, fragmentObject.Name());
    api.linkProgram(program);
    Verify(api, program, linkStatus, api.getProgramiv, api.getProgramInfoLog,
           "GLSL program link");
    api.detachShader(program, vertexObject.Name());
    api.detachShader(program, fragmentObject.Name());

    int uniforms = 0;
    int uniformBlocks = 0;
    api.getProgramiv(program, activeUniforms, &uniforms);
    api.getProgramiv(program, activeUniformBlocks, &uniformBlocks);
    api.Check();
    if (uniforms != 0 || uniformBlocks != 0) {
        throw Failure{ErrorCode::Unsupported,
                      {},
                      "OpenGL GLSL resources are not supported"};
    }
}

void Pipeline::Use() const
{
    api.useProgram(program);
}

std::span<const VertexAttribute> Pipeline::Attributes() const noexcept
{
    return attributes;
}

std::uint32_t Pipeline::Stride() const noexcept
{
    return stride;
}
} // namespace voryx::opengl_backend
