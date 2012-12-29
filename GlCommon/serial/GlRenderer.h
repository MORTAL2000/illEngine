#ifndef __GL_RENDERER_H__
#define __GL_RENDERER_H__

#include "Graphics/RendererBackend.h"

namespace GlCommon {

class GlRenderer : public Graphics::RendererBackend {
public:
    void initialize();
    void uninitialize();

    void beginFrame();
    void endFrame();

    void loadTexture(void ** textureData, const Graphics::TextureLoadArgs& loadArgs);
    void unloadTexture(void ** textureData);
    
    void loadMesh(void** meshBackendData, const MeshData<>& meshFrontendData);
    void unloadMesh(void** meshBackendData);

    void loadShader(void ** shaderData, uint64_t featureMask);
    void loadShaderInternal(void ** shaderData, const char * path, unsigned int shaderType, const char * defines);
    void unloadShader(void ** shaderData);

    void loadShaderProgram(void ** programData, const std::vector<RefCountPtr<Graphics::Shader> >& shaderList);
    void unloadShaderProgram(void ** programData);
};

}

#endif
