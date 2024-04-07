#pragma once

#include "ManagerStorage.hpp"
#include "Light.h"
#include "Camera.hpp"
#include "SkyBox.h"
#include "tinygltf/tiny_gltf.h"


class SceneManager {
    struct Node {
        UINT meshId = 0;
        std::vector<int> children;
        XMMATRIX transformation = XMMatrixIdentity();
    };

    struct BufferAccessor {
        ID3D11Buffer* buffer = nullptr;
        UINT byteStride = 0;
        UINT byteOffset = 0;
        UINT count = 0;
        DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT;

        BufferAccessor() = default;

        ~BufferAccessor() {
            SAFE_RELEASE(buffer);
        };
    };

    struct Attribute {
        std::string semantic = "POSITION";
        UINT verticesAccessorId = 0;

        friend bool operator<(const Attribute& a1, const Attribute& a2) {
            return a1.semantic < a2.semantic;
        };
    };

    struct Primitive {
        int materialId = 0;
        D3D_PRIMITIVE_TOPOLOGY mode = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        std::vector<Attribute> attributes;
        int indicesAccessorId = 0;
        std::shared_ptr<VertexShader> VS;
        std::shared_ptr<PixelShader> PSDefault;
        std::shared_ptr<PixelShader> PSFresnel;
        std::shared_ptr<PixelShader> PSNdf;
        std::shared_ptr<PixelShader> PSGeometry;
        std::shared_ptr<VertexShader> shadowVS;
        std::shared_ptr<PixelShader> shadowPS;
    };

    struct TextureAccessor {
        int texCoordId = 0;
        int textureId = -1;
        int samplerId = 0;
        bool isSRGB = false;
    };

    enum AlphaMode {
        OPAQUE_MODE,
        BLEND_MODE,
        ALPHA_CUTOFF_MODE
    };

    struct Material {
        AlphaMode mode = OPAQUE_MODE;
        XMFLOAT4 baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        XMFLOAT3 emissiveFactor = { 0.0f, 0.0f, 0.0f };
        float alphaCutoff = 0.5f;
        float occlusionStrength = 1.0f;
        float normalScale = 1.0f;
        TextureAccessor baseColorTA;
        TextureAccessor roughMetallicTA;
        TextureAccessor normalTA;
        TextureAccessor emissiveTA;
        TextureAccessor occlusionTA;
        std::shared_ptr<ID3D11RasterizerState> rasterizerState;
        std::shared_ptr<ID3D11DepthStencilState> depthStencilState;
        std::shared_ptr<ID3D11BlendState> blendState;
    };

    struct Mesh {
        std::vector<Primitive> opaquePrimitives;
        std::vector<Primitive> transparentPrimitives;
        std::vector<Primitive> primitivesWithAlphaCutoff;
    };

    struct TransparentPrimitive {
        const Primitive& primitive;
        float distance = 0.0f;

        TransparentPrimitive(const Primitive& p) : primitive(p) {};

        ~TransparentPrimitive() = default;

        friend bool operator<(const TransparentPrimitive& p1, const TransparentPrimitive& p2) {
            return p1.distance < p2.distance;
        };
    };

    struct Scene {
        std::vector<int> rootNodes;
        XMMATRIX transformation = XMMatrixIdentity();
        UINT arraysId = 0;
    };

    struct SceneArrays {
        std::vector<Node> nodes;
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<std::shared_ptr<Texture>> textures;
        std::vector<std::shared_ptr<ID3D11SamplerState>> samplers;
        std::vector<BufferAccessor> accessors;
    };

    struct WorldMatrixBuffer {
        XMMATRIX worldMatrix;
    };

    struct ViewMatrixBuffer {
        XMMATRIX viewProjectionMatrix;
        XMFLOAT4 cameraPos;
        XMINT4 lightParams;
        SpotLight lights[MAX_LIGHT];
        DirectionalLight directionalLight;
    };

    struct MaterialParamsBuffer {
        XMFLOAT4 baseColorFactor;
        XMFLOAT4 MRONFactors;
        XMFLOAT4 emissiveFactorAlphaCutoff;
        XMUINT4 baseColorTA;
        XMUINT4 roughMetallicTA;
        XMUINT4 normalTA;
        XMUINT4 emissiveTA;
        XMUINT4 occlusionTA;
    };

public:
    enum Mode {
        DEFAULT,
        FRESNEL,
        NDF,
        GEOMETRY,
        SHADOW_MAP
    };

    SceneManager() = default;

    HRESULT Init(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage,
        const std::shared_ptr<Camera>& camera);
    HRESULT LoadScene(const std::string& name, UINT& index, UINT& count, const XMMATRIX& transformation = XMMatrixIdentity());
    bool SetCurrentScene(UINT index);
    int GetCurrentScene() const;
    void SetMode(Mode mode);
    bool IsInit() const;
    void Cleanup();

    ~SceneManager() {
        Cleanup();
    };

private:
    HRESULT CreateBufferAccessors(const tinygltf::Model& model, SceneArrays& arrays);
    HRESULT CreateSamplers(const tinygltf::Model& model, SceneArrays& arrays);
    HRESULT CreateTextures(const tinygltf::Model& model, SceneArrays& arrays, const std::string& gltfFileName);
    HRESULT CreateMaterials(const tinygltf::Model& model, SceneArrays& arrays);
    HRESULT CreateMeshes(const tinygltf::Model& model, SceneArrays& arrays);
    HRESULT CreateNodes(const tinygltf::Model& model, SceneArrays& arrays);
    DXGI_FORMAT GetFormat(const tinygltf::Accessor& accessor);
    DXGI_FORMAT GetFormatScalar(const tinygltf::Accessor& accessor);
    DXGI_FORMAT GetFormatVec2(const tinygltf::Accessor& accessor);
    DXGI_FORMAT GetFormatVec3(const tinygltf::Accessor& accessor);
    DXGI_FORMAT GetFormatVec4(const tinygltf::Accessor& accessor);
    D3D11_TEXTURE_ADDRESS_MODE GetSamplerMode(int m);
    void ParseAttributes(const SceneArrays& arrays, const tinygltf::Primitive& primitive,
        std::vector<Attribute>& attributes, std::vector<std::string>& baseDefines, std::vector<D3D11_INPUT_ELEMENT_DESC>& desc);
    HRESULT CreateShaders(Primitive& primitive, const SceneArrays& arrays,
        const std::vector<std::string>& baseDefines, const std::vector<D3D11_INPUT_ELEMENT_DESC>& desc);

    static const int depthBias_ = 16;
    static const float slopeScaleBias_;

    std::shared_ptr<Device> device_; // provided externally <-
    std::shared_ptr<ManagerStorage> managerStorage_; // provided externally <-
    std::shared_ptr<Camera> camera_; // provided externally <-

    ID3D11Buffer* worldMatrixBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* viewMatrixBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* materialParamsBuffer_ = nullptr; // always remains only inside the class #

    std::vector<Scene> scenes_;
    std::vector<SceneArrays> sceneArrays_;

    UINT currentScene_ = 0;
    Mode currentMode_ = DEFAULT;
    std::vector<TransparentPrimitive> transparentPrimitives_;
};