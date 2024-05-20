#pragma once

#include "ManagerStorage.hpp"
#include "Light.hpp"
#include "Camera.hpp"
#include "SkyBox.h"
#include "tinygltf/tiny_gltf.h"

#define MAX_SSAO_SAMPLE_COUNT 64
#define NOISE_BUFFER_SIZE 16
#define NOISE_BUFFER_SPLIT_SIZE 4


class SceneManager {
    struct Node {
        int meshId = 0;
        std::vector<int> children;
        XMMATRIX transformation = XMMatrixIdentity();
    };

    struct BufferAccessor {
        std::shared_ptr<ID3D11Buffer> buffer;
        UINT byteStride = 0;
        UINT byteOffset = 0;
        UINT count = 0;
        DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    };

    struct Attribute {
        std::string semantic = "EMPTY";
        int verticesAccessorId = 0;
    };

    struct Primitive {
        int materialId = 0;
        D3D_PRIMITIVE_TOPOLOGY mode = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        std::vector<Attribute> attributes;
        int indicesAccessorId = 0;
        std::shared_ptr<VertexShader> VS;
        std::shared_ptr<PixelShader> gBufferPS; // only for deferred render
        std::shared_ptr<PixelShader> PSDefault; // only for forward render
        std::shared_ptr<PixelShader> PSFresnel; // only for forward render
        std::shared_ptr<PixelShader> PSNdf; // only for forward render
        std::shared_ptr<PixelShader> PSGeometry; // only for forward render
        std::shared_ptr<PixelShader> PSShadowSplits; // only for forward render
        std::shared_ptr<PixelShader> PSSSAOMask; // only for forward render
        std::shared_ptr<PixelShader> transparentPSSSAO; // only for transparent
        std::shared_ptr<PixelShader> transparentPSSSAOMask; // only for transparent
        std::shared_ptr<VertexShader> shadowVS;
        std::shared_ptr<PixelShader> shadowPS; // only for alpha cutoff
    };

    struct TextureAccessor {
        int texCoordId = 0;
        int textureId = -1;
        int samplerId = 0;
        bool isSRGB = false;
    };

    enum class AlphaMode {
        OPAQUE_MODE,
        BLEND_MODE,
        ALPHA_CUTOFF_MODE
    };

    struct Material {
        AlphaMode mode = AlphaMode::OPAQUE_MODE;
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
        D3D11_CULL_MODE cullMode = D3D11_CULL_NONE;
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
        Primitive primitive;
        XMMATRIX transformation = XMMatrixIdentity();
        float distance = 0.0f;
        int arrayId = 0;

        TransparentPrimitive(Primitive p) : primitive(p) {};

        ~TransparentPrimitive() = default;

        friend bool operator<(const TransparentPrimitive& p1, const TransparentPrimitive& p2) {
            return p1.distance < p2.distance;
        };
    };

    struct Scene {
        std::vector<int> rootNodes;
        XMMATRIX transformation = XMMatrixIdentity();
        int arraysId = 0;
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

    struct InstancingWorldMatrixBuffer {
        XMMATRIX worldMatrix[MAX_LIGHT];
    };

    struct ViewMatrixBuffer {
        XMMATRIX viewProjectionMatrix;
    };

    struct ShadowMapAlphaCutoffBuffer {
        XMFLOAT4 baseColorFactor;
        XMFLOAT4 alphaCutoffTexCoord;
    };

    struct LightBuffer {
        XMFLOAT4 parameters[MAX_LIGHT];
        XMFLOAT4 pos[MAX_LIGHT];
        XMFLOAT4 color[MAX_LIGHT];
    };

    struct DirectionalLightBuffer {
        XMFLOAT4 direction;
        XMFLOAT4 color;
        XMMATRIX viewProjectionMatrix;
        XMUINT4 splitSizeRatio;
    };

    struct MaterialParamsBuffer {
        XMFLOAT4 baseColorFactor;
        XMFLOAT4 MRONFactors;
        XMFLOAT4 emissiveFactorAlphaCutoff;
        XMINT4 baseColorTA;
        XMINT4 roughMetallicTA;
        XMINT4 normalTA;
        XMINT4 emissiveTA;
        XMINT4 occlusionTA;
    };

    struct ForwardRenderViewMatrixBuffer {
        XMMATRIX viewProjectionMatrix;
        XMINT4 lightParams;
        DirectionalLight::DirectionalLightInfo directionalLight;
        PointLight lights[MAX_LIGHT];
    };

    struct MatricesBuffer {
        XMFLOAT4 cameraPos;
        XMMATRIX projectionMatrix;
        XMMATRIX invProjectionMatrix;
        XMMATRIX viewMatrix;
        XMMATRIX invViewMatrix;
    };

    struct SSAOParamsBuffer {
        XMFLOAT4 parameters;
        XMFLOAT4 sizes;
        XMFLOAT4 samples[MAX_SSAO_SAMPLE_COUNT];
        XMFLOAT4 noise[NOISE_BUFFER_SIZE];
    };

    struct RawPtrTexture {
        ID3D11Texture2D* texture = nullptr;
        ID3D11RenderTargetView* RTV = nullptr;
        ID3D11ShaderResourceView* SRV = nullptr;
    };

    struct RawPtrDepthBuffer {
        ID3D11Texture2D* texture = nullptr;
        ID3D11DepthStencilView* DSV = nullptr;
        ID3D11ShaderResourceView* SRV = nullptr;
    };

    struct SphereVertex {
        XMFLOAT3 pos;
    };

public:
    enum class Mode {
        DEFAULT,
        FRESNEL,
        NDF,
        GEOMETRY,
        SHADOW_SPLITS,
        SSAO_MASK
    };

    // general settings
    bool excludeTransparent = true;
    bool deferredRender = true;

    // default mode settings
    bool withSSAO = true;

    // shadows settings
    int depthBias = 4;
    float slopeScaleBias = 2 * sqrt(2);

    // SSAO settings
    float SSAODepthLimit = 0.000001f;
    float SSAORadius = 2.1f;

    // deferred render point light settings
    float smoothClampRadianceLimit = 1.0f;
    float clampRadianceLimit = 0.5f;

    SceneManager();

    HRESULT Init(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage,
        const std::shared_ptr<Camera>& camera, const std::shared_ptr<DirectionalLight>& directionalLight, int width, int height,
        const std::shared_ptr<ID3DUserDefinedAnnotation>& annotation = nullptr);
    HRESULT LoadScene(const std::string& name, UINT& index, UINT& count, const XMMATRIX& transformation = XMMatrixIdentity());
    bool Render(
        const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& BRDF,
        const std::shared_ptr<Skybox>& skybox,
        const std::vector<PointLight>& lights,
        const std::vector<int>& sceneIndices
    );
    bool Resize(int width, int height);
    void Cleanup();

    void SetMode(Mode mode) {
        currentMode_ = mode;
    };

    void SetRenderTarget(const std::shared_ptr<ID3D11RenderTargetView>& renderTarget) {
        renderTarget_ = renderTarget;
    };

    void SetViewport(const D3D11_VIEWPORT& viewport) {
        renderTargetViewport_ = viewport;
    };

    bool IsInit() const {
        return isInit_;
    };

    ~SceneManager() {
        Cleanup();
    };

private:
    HRESULT GenerateSphere();
    HRESULT CreateDepthStencilView(int width, int height, RawPtrDepthBuffer& buffer);
    HRESULT CreateAuxiliaryForDeferredRender();
    HRESULT CreateAuxiliaryForShadowMaps();
    HRESULT CreateAuxiliaryForTransparent();
    HRESULT CreateTexture(RawPtrTexture& texture, int width, int height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
    HRESULT CreateTexture(RawPtrTexture& texture, int i);
    HRESULT CreateBuffers();

    HRESULT CreateBufferAccessors(const tinygltf::Model& model, SceneArrays& arrays);
    HRESULT CreateSamplers(const tinygltf::Model& model, SceneArrays& arrays);
    HRESULT CreateTextures(const tinygltf::Model& model, SceneArrays& arrays, const std::string& gltfFileName);
    HRESULT CreateMaterials(const tinygltf::Model& model, SceneArrays& arrays);
    HRESULT CreateMeshes(const tinygltf::Model& model, SceneArrays& arrays);
    HRESULT CreateNodes(const tinygltf::Model& model, SceneArrays& arrays);
    DXGI_FORMAT GetFormat(const tinygltf::Accessor& accessor, UINT& size);
    DXGI_FORMAT GetFormatScalar(const tinygltf::Accessor& accessor, UINT& size);
    DXGI_FORMAT GetFormatVec2(const tinygltf::Accessor& accessor, UINT& size);
    DXGI_FORMAT GetFormatVec3(const tinygltf::Accessor& accessor, UINT& size);
    DXGI_FORMAT GetFormatVec4(const tinygltf::Accessor& accessor, UINT& size);
    D3D11_TEXTURE_ADDRESS_MODE GetSamplerMode(int m);
    void ParseAttributes(const SceneArrays& arrays, const tinygltf::Primitive& primitive,
        std::vector<Attribute>& attributes, std::vector<std::string>& baseDefines, std::vector<D3D11_INPUT_ELEMENT_DESC>& desc);
    HRESULT CreateShaders(Primitive& primitive, const SceneArrays& arrays,
        const std::vector<std::string>& baseDefines, const std::vector<D3D11_INPUT_ELEMENT_DESC>& desc);

    bool CreateShadowMaps(const std::vector<int>& sceneIndices);
    bool CreateShadowMapForNode(int arrayId, int nodeId, const XMMATRIX& transformation = XMMatrixIdentity());
    bool CreateShadowMapForPrimitive(int arrayId, const Primitive& primitive, AlphaMode mode, const XMMATRIX& transformation);
    bool PrepareTransparent(const std::vector<int>& sceneIndices);
    bool PrepareTransparentForNode(int arrayId, int nodeId, const XMMATRIX& transformation = XMMatrixIdentity());
    bool AddPrimitiveToTransparentPrimitives(int arrayId, const Primitive& primitive, const XMMATRIX& transformation);
    void RenderNode(
        int arrayId,
        int nodeId,
        const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& BRDF,
        const XMMATRIX& transformation = XMMatrixIdentity()
    );
    void RenderPrimitive(
        int arrayId,
        const Primitive& primitive,
        const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& BRDF,
        const XMMATRIX& transformation,
        bool transparent = false
    );
    void RenderTransparent(
        const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& BRDF
    );
    void RenderAmbientLight(
        const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& BRDF
    );
    void RenderDirectionalLight();
    void RenderPointLights(const std::vector<PointLight>& lights);

    std::shared_ptr<Device> device_; // provided externally <-
    std::shared_ptr<ManagerStorage> managerStorage_; // provided externally <-
    std::shared_ptr<Camera> camera_; // provided externally <-
    std::shared_ptr<DirectionalLight> directionalLight_; // provided externally <-
    std::shared_ptr<ID3DUserDefinedAnnotation> annotation_; // provided externally <-

    std::shared_ptr<ID3D11RenderTargetView> renderTarget_; // provided externally <-

    ID3D11Buffer* worldMatrixBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* instancingWorldMatrixBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* viewMatrixBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* shadowMapAlphaCutoffBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* directionalLightBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* lightBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* materialParamsBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* forwardRenderViewMatrixBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* matricesBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* SSAOParamsBuffer_ = nullptr; // always remains only inside the class #

    std::shared_ptr<ID3D11DepthStencilState> shadowDepthStencilState_; // provided externally <-
    std::shared_ptr<ID3D11DepthStencilState> transparentDepthStencilState_; // provided externally <-
    std::shared_ptr<ID3D11DepthStencilState> pointLightDepthStencilState_; // provided externally <-
    std::shared_ptr<ID3D11DepthStencilState> generalDepthStencilState_; // provided externally <-

    std::shared_ptr<ID3D11RasterizerState> pointLightRasterizerState_; // provided externally <-
    std::shared_ptr<ID3D11RasterizerState> generalRasterizerState_; // provided externally <-

    std::shared_ptr<ID3D11BlendState> generalBlendState_; // provided externally <-

    std::shared_ptr<ID3D11SamplerState> samplerAvg_; // provided externally <-
    std::shared_ptr<ID3D11SamplerState> samplerPCF_; // provided externally <-
    std::shared_ptr<ID3D11SamplerState> samplerMax_; // provided externally <-
    std::shared_ptr<ID3D11SamplerState> generalSampler_; // provided externally <-

    ID3D11Texture2D* readMaxTexture_ = nullptr; // always remains only inside the class #

    RawPtrDepthBuffer depth_; // always remains only inside the class #
    RawPtrDepthBuffer depthCopy_; // always remains only inside the class #
    RawPtrDepthBuffer transparentDepth_; // always remains only inside the class #

    RawPtrTexture normals_; // always remains only inside the class #
    RawPtrTexture features_; // always remains only inside the class #
    RawPtrTexture color_; // always remains only inside the class #
    RawPtrTexture emissive_; // always remains only inside the class #

    std::vector<D3D11_INPUT_ELEMENT_DESC> sphereVertexDesc_ = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    UINT sphereNumIndices_ = 0;
    ID3D11Buffer* sphereVertexBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* sphereIndexBuffer_ = nullptr; // always remains only inside the class #

    std::shared_ptr<VertexShader> mappingVS_; // provided externally <-
    std::shared_ptr<PixelShader> downsamplePS_; // provided externally <-

    std::shared_ptr<VertexShader> pointLightVS_; // provided externally <-
    std::shared_ptr<PixelShader> pointLightPS_; // provided externally <-
    std::shared_ptr<PixelShader> pointLightPSFresnel_; // provided externally <-
    std::shared_ptr<PixelShader> pointLightPSNDF_; // provided externally <-
    std::shared_ptr<PixelShader> pointLightPSGeometry_; // provided externally <-

    std::shared_ptr<PixelShader> directionalLightPS_; // provided externally <-
    std::shared_ptr<PixelShader> directionalLightPSShadowSplits_; // provided externally <-

    std::shared_ptr<PixelShader> ambientLightPS_; // provided externally <-
    std::shared_ptr<PixelShader> ambientLightPSSSAO_; // provided externally <-
    std::shared_ptr<PixelShader> ambientLightPSSSAOMask_; // provided externally <-

    std::vector<Scene> scenes_;
    std::vector<SceneArrays> sceneArrays_;
    std::vector<TransparentPrimitive> transparentPrimitives_;
    std::vector<RawPtrDepthBuffer> shadowSplits_;  // always remains only inside the class #
    std::vector<RawPtrTexture> scaledFrames_;  // always remains only inside the class #

    XMFLOAT4 SSAOSamples_[MAX_SSAO_SAMPLE_COUNT];
    XMFLOAT4 SSAONoise_[NOISE_BUFFER_SIZE];

    Mode currentMode_ = Mode::DEFAULT;

    D3D11_VIEWPORT viewport_;
    static const UINT shadowMapSize = 4096;
    D3D11_VIEWPORT renderTargetViewport_;
    int width_ = 0;
    int height_ = 0;
    int n_ = 0;

    bool isInit_ = false;
};