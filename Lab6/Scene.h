#pragma once

#include "ManagerStorage.hpp"
#include "Light.hpp"
#include "Camera.hpp"
#include "SkyBox.h"
#include "tinygltf/tiny_gltf.h"

#define MAX_SSAO_SAMPLE_COUNT 32
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
        std::shared_ptr<PixelShader> PSDefault;
        std::shared_ptr<PixelShader> PSFresnel;
        std::shared_ptr<PixelShader> PSNdf;
        std::shared_ptr<PixelShader> PSGeometry;
        std::shared_ptr<PixelShader> PSShadowSplits;
        std::shared_ptr<PixelShader> PSSSAO;
        std::shared_ptr<VertexShader> shadowVS;
        std::shared_ptr<PixelShader> shadowPS;
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

    struct ViewMatrixBuffer {
        XMMATRIX viewProjectionMatrix;
        XMFLOAT4 cameraPos;
        XMINT4 lightParams;
        DirectionalLight::DirectionalLightInfo directionalLight;
        SpotLight lights[MAX_LIGHT];
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

    struct ShadowMapViewMatrixBuffer {
        XMMATRIX viewProjectionMatrix;
    };

    struct ShadowMapAlphaCutoffBuffer {
        XMFLOAT4 baseColorFactor;
        XMFLOAT4 alphaCutoffTexCoord;
    };

    struct SSAOParamsBuffer {
        XMFLOAT4 parameters;
        XMFLOAT4 samples[MAX_SSAO_SAMPLE_COUNT];
        XMMATRIX invProjectionMatrix;
        XMMATRIX projectionMatrix;
        XMMATRIX viewMatrix;
        XMFLOAT4 noise[NOISE_BUFFER_SIZE];
    };

    struct RawPtrTexture {
        ID3D11Texture2D* texture = nullptr;
        ID3D11RenderTargetView* RTV = nullptr;
        ID3D11ShaderResourceView* SRV = nullptr;
    };

public:
    enum Mode {
        DEFAULT,
        FRESNEL,
        NDF,
        GEOMETRY,
        SHADOW_SPLITS
    };

    SceneManager();

    HRESULT Init(const std::shared_ptr<Device>& device, const std::shared_ptr<ManagerStorage>& managerStorage,
        const std::shared_ptr<Camera>& camera, const std::shared_ptr<DirectionalLight>& directionalLight, int width, int height);
    HRESULT LoadScene(const std::string& name, UINT& index, UINT& count, const XMMATRIX& transformation = XMMatrixIdentity());
    bool CreateShadowMaps(const std::vector<int>& sceneIndices);
    bool PrepareTransparent(const std::vector<int>& sceneIndices);
    bool Render(
        const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& BRDF,
        const std::shared_ptr<Skybox>& skybox,
        const std::vector<SpotLight>& lights,
        const std::vector<int>& sceneIndices
    );
    bool MakeDepthPrepath(const std::vector<int>& sceneIndices);
    bool Resize(int width, int height);
    void SetMode(Mode mode);
    bool IsInit() const;
    void Cleanup();

    void ExcludeTransparent(bool exclude) {
        excludeTransparent_ = exclude;
    };

    void SetDepthBias(int bias) {
        depthBias_ = bias;
    };

    void SetSlopeScaleBias(float slopeScaleBias) {
        slopeScaleBias_ = slopeScaleBias;
    };

    int GetDepthBias() {
        return depthBias_;
    };

    float GetSlopeScaleBias() {
        return slopeScaleBias_;
    };

    void WithSSAO(bool active) {
        withSSAO_ = active;
    };

    float GetSSAODepthLimit() {
        return SSAODepthLimit_;
    };

    float GetSSAORadius() {
        return SSAORadius_;
    };

    void SetSSAODepthLimit(float limit) {
        SSAODepthLimit_ = limit;
    };

    void SetSSAORadius(float radius) {
        SSAORadius_ = radius;
    };

    ~SceneManager() {
        Cleanup();
    };

private:
    HRESULT CreateDepthStencilView(int width, int height, ID3D11Texture2D** buffer, ID3D11DepthStencilView** DSV, ID3D11ShaderResourceView** SRV);
    HRESULT CreateAuxiliaryForTransparent(int width, int height);
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
    bool CreateShadowMapForNode(int arrayId, int nodeId, const XMMATRIX& transformation = XMMatrixIdentity());
    bool CreateShadowMapForPrimitive(int arrayId, const Primitive& primitive, AlphaMode mode, const XMMATRIX& transformation);
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
        const XMMATRIX& transformation
    );
    void RenderTransparent(
        const std::shared_ptr<ID3D11ShaderResourceView>& irradianceMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& prefilteredMap,
        const std::shared_ptr<ID3D11ShaderResourceView>& BRDF
    );
    void MakeDepthPrepathForNode(int arrayId, int nodeId, const XMMATRIX& transformation = XMMatrixIdentity());
    void MakeDepthPrepathForPrimitive(int arrayId, const Primitive& primitive, AlphaMode mode, const XMMATRIX& transformation);

    int depthBias_ = 4;
    float slopeScaleBias_ = 2* sqrt(2);
    static const UINT shadowMapSize = 4096;
    D3D11_VIEWPORT shadowMapViewport_;
    D3D11_VIEWPORT transparentViewport_;
    D3D11_VIEWPORT downsampleViewport_;
    D3D11_VIEWPORT depthPrepathViewport_;

    std::shared_ptr<Device> device_; // provided externally <-
    std::shared_ptr<ManagerStorage> managerStorage_; // provided externally <-
    std::shared_ptr<Camera> camera_; // provided externally <-
    std::shared_ptr<DirectionalLight> directionalLight_; // provided externally <-
    std::shared_ptr<ID3D11SamplerState> samplerAvg_; // provided externally <-

    ID3D11Buffer* worldMatrixBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* viewMatrixBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* materialParamsBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* shadowMapViewMatrixBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* shadowMapAlphaCutoffBuffer_ = nullptr; // always remains only inside the class #
    ID3D11Buffer* SSAOParamsBuffer_ = nullptr; // always remains only inside the class #

    ID3D11Texture2D* shadowBuffers_[CSM_SPLIT_COUNT]; // always remains only inside the class #
    ID3D11DepthStencilView* shadowDSViews_[CSM_SPLIT_COUNT]; // always remains only inside the class #
    ID3D11ShaderResourceView* shadowSRViews_[CSM_SPLIT_COUNT]; // always remains only inside the class #

    std::shared_ptr<ID3D11DepthStencilState> shadowDepthStencilState_; // provided externally <-
    std::shared_ptr<ID3D11SamplerState> samplerPCF_; // provided externally <-

    ID3D11Texture2D* readMaxTexture_ = nullptr; // always remains only inside the class #
    std::vector<RawPtrTexture> scaledFrames_;  // always remains only inside the class #
    ID3D11Texture2D* transparentDepthBuffer_ = nullptr; // always remains only inside the class #
    ID3D11DepthStencilView* transparentDSView_ = nullptr; // always remains only inside the class #
    ID3D11ShaderResourceView* transparentSRView_ = nullptr; // always remains only inside the class #

    ID3D11Texture2D* depthBuffer_ = nullptr; // always remains only inside the class #
    ID3D11DepthStencilView* depthDSView_ = nullptr; // always remains only inside the class #
    ID3D11ShaderResourceView* depthSRView_ = nullptr; // always remains only inside the class #

    std::shared_ptr<VertexShader> mappingVS_; // provided externally <-
    std::shared_ptr<PixelShader> downsamplePS_; // provided externally <-
    std::shared_ptr<ID3D11DepthStencilState> transparentDepthStencilState_; // provided externally <-
    std::shared_ptr<ID3D11SamplerState> samplerMax_; // provided externally <-

    std::shared_ptr<ID3D11DepthStencilState> depthPrepathDepthStencilState_; // provided externally <-

    std::vector<Scene> scenes_;
    std::vector<SceneArrays> sceneArrays_;

    float SSAODepthLimit_ = 1.0f;
    float SSAORadius_ = 1.0f;
    XMFLOAT4 SSAOSamples_[MAX_SSAO_SAMPLE_COUNT];
    XMFLOAT4 SSAONoise_[NOISE_BUFFER_SIZE];

    Mode currentMode_ = DEFAULT;
    std::vector<TransparentPrimitive> transparentPrimitives_;
    int n = 0;
    bool excludeTransparent_ = true;
    bool withSSAO_ = true;
};