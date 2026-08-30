#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <type_traits>
#include <cstring>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <wrl/client.h>

using Microsoft::WRL::ComPtr;


// ============================================================
// D3D11 error handling
// ============================================================

#define checkD3DError(hr) _checkD3DError((hr), __FILE__, __LINE__)

void _checkD3DError(
    HRESULT hr,
    const char* file,
    int line
);


// ============================================================
// Shader program
// ============================================================

class ShaderProgram
{
public:

    enum class InputLayoutType
    {
        None,
        Position2D,
        Position3D,
        QuadWithFlashData,
        QuadWithNoteData,
        QuadWithKeyData
    };

    ShaderProgram();
    ~ShaderProgram();

    void init(
        ID3D11Device* device,
        const std::string& vertexName,
        const std::string& fragmentName,
        InputLayoutType inputLayout = InputLayoutType::None,
        bool verbose = false
    );

    void clean();

    // Bind shaders and remember the context.
    void use(ID3D11DeviceContext* context);
    void unuse();

    // Bind the vertex input layout.
    void bindInputLayout(ID3D11DeviceContext* context);

    // --------------------------------------------------------
    // Resources
    // --------------------------------------------------------

    void texture(
        ID3D11DeviceContext* context,
        const std::string& name,
        ID3D11ShaderResourceView* texture
    );

    // --------------------------------------------------------
    // Uniforms
    // --------------------------------------------------------

    void uniform(const std::string& name, bool value)
    {
        int v = value ? 1 : 0;

        setUniform(
            name,
            reinterpret_cast<const unsigned char*>(&v),
            sizeof(v)
        );
    }

    void uniform(ID3D11DeviceContext* context, const std::string& name, bool value)
    {
        _context = context;

        int v = value ? 1 : 0;

        setUniform(
            name,
            reinterpret_cast<const unsigned char*>(&v),
            sizeof(v)
        );
    }

    template<typename T>
    void uniform(
        const std::string& name,
        const T& value
    )
    {
        setUniform(
            name,
            reinterpret_cast<const unsigned char*>(&value),
            sizeof(T)
        );
    }

    template<typename T>
    void uniform(
        ID3D11DeviceContext* context,
        const std::string& name,
        const T& value
    )
    {
        _context = context;

        setUniform(
            name,
            reinterpret_cast<const unsigned char*>(&value),
            sizeof(T)
        );
    }

    template<typename T>
    void uniforms(
        const std::string& name,
        size_t count,
        const T* values
    )
    {
        if (!values || count == 0)
            return;

        setUniform(
            name,
            reinterpret_cast<const unsigned char*>(values),
            sizeof(T) * count
        );
    }

    template<typename T>
    void uniforms(
        ID3D11DeviceContext* context,
        const std::string& name,
        size_t count,
        const T* values
    )
    {
        if (!values || count == 0)
            return;

        _context = context;

        setUniform(
            name,
            reinterpret_cast<const unsigned char*>(values),
            sizeof(T) * count
        );
    }

    // --------------------------------------------------------
    // Accessors
    // --------------------------------------------------------

    ID3D11VertexShader* vertexShader() const
    {
        return _vertexShader.Get();
    }

    ID3D11PixelShader* pixelShader() const
    {
        return _pixelShader.Get();
    }

    ID3DBlob* vertexShaderBlob() const
    {
        return _vertexShaderBlob.Get();
    }

    ID3D11InputLayout* getInputLayout() const
    {
        return _inputLayout.Get();
    }

private:

    enum class ShaderStage
    {
        Vertex,
        Pixel
    };

    struct ConstantBuffer
    {
        ComPtr<ID3D11Buffer> buffer;

        UINT size = 0;

        // CPU-side copy of the constant buffer.
        std::vector<unsigned char> data;

        // Shader stage owning this buffer.
        ShaderStage stage = ShaderStage::Vertex;

        // Register slot, e.g. b0.
        UINT slot = 0;
    };

    struct UniformInfo
    {
        size_t bufferIndex = 0;

        UINT offset = 0;
        UINT size = 0;

        UINT arrayStride = 0;
        UINT arrayElements = 0;

        ShaderStage stage = ShaderStage::Vertex;
    };

    void loadVertexShader(
        ID3D11Device* device,
        const std::string& source
    );

    void loadPixelShader(
        ID3D11Device* device,
        const std::string& source
    );

    void reflectShader(
        ID3DBlob* shaderBlob,
        ShaderStage stage
    );

    void createInputLayout(
        ID3D11Device* device,
        ID3DBlob* vertexShaderBlob,
        InputLayoutType type
    );

    void setUniform(
        const std::string& name,
        const unsigned char* data,
        size_t size
    );

    void setUniform(
        ID3D11DeviceContext* context,
        const std::string& name,
        const unsigned char* data,
        size_t size
    );

private:

    ComPtr<ID3D11VertexShader> _vertexShader;
    ComPtr<ID3D11PixelShader> _pixelShader;

    ComPtr<ID3D11SamplerState> _sampler;
    ComPtr<ID3D11InputLayout> _inputLayout;

    InputLayoutType _inputLayoutType =
        InputLayoutType::None;

    ID3D11Device* _device = nullptr;
    ID3D11DeviceContext* _context = nullptr;

    // One constant buffer per shader-stage/register pair.
    //
    // Example:
    //   VS b0 -> buffer 0
    //   PS b0 -> buffer 1
    std::vector<ConstantBuffer> _constantBuffers;

    std::vector<unsigned char> _vertexShaderBytecode;
    ComPtr<ID3DBlob> _vertexShaderBlob;

    // A uniform name may exist in both shader stages.
    //
    // Example:
    //   "time" -> [VS time, PS time]
    std::unordered_map<
        std::string,
        std::vector<UniformInfo>
    > _uniforms;

    struct TextureBinding
    {
        UINT slot;
        ShaderStage stage;
    };

    // Texture register lookup.
    std::unordered_map<std::string, TextureBinding> _textures;

    std::string _vertexName = "UNKNOWN_VERTEX_SHADER_NAME";
    std::string _fragmentName = "UNKNOWN_FRAGMENT_SHADER_NAME";
    bool _verbose = false;
};


// ============================================================
// Texture loading
// ============================================================

ComPtr<ID3D11ShaderResourceView> loadTexture(
    ID3D11Device* device,
    const std::string& path,
    unsigned int channels,
    bool sRGB
);


ComPtr<ID3D11ShaderResourceView> loadTexture(
    ID3D11Device* device,
    unsigned char* image,
    unsigned int width,
    unsigned int height,
    unsigned int channels,
    bool sRGB
);


// ============================================================
// Texture arrays
// ============================================================

ComPtr<ID3D11ShaderResourceView> loadTextureArray(
    ID3D11Device* device,
    const std::vector<std::string>& paths,
    bool sRGB,
    int& layers
);


ComPtr<ID3D11ShaderResourceView> loadTextureArray(
    ID3D11Device* device,
    const std::vector<unsigned char*>& images,
    const std::vector<glm::ivec2>& sizes,
    unsigned int channels,
    bool sRGB
);

std::vector<ComPtr<ID3D11ShaderResourceView>>
generate2DViewsOfArray(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* textureArray,
    unsigned int maxSize
);