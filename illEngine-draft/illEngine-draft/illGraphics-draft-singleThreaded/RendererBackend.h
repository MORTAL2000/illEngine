#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <glm/glm.hpp>
#include <stdint.h>
#include <string>
#include <vector>

#include "../illUtil-draft-singleThreaded/RefCountPtr.h"
#include "../illUtil-draft-singleThreaded/Geometry/MeshData.h"

namespace Graphics {

struct TextureLoadArgs;
class Shader;

class RendererBackend {
public:
    virtual void initialize() = 0;
    virtual void uninitialize() = 0;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;

    virtual void loadTexture(void ** textureData, const TextureLoadArgs& loadArgs) = 0;
    virtual void unloadTexture(void ** textureData) = 0;

    virtual void loadMesh(void** meshBackendData, const MeshData<>& meshFrontendData) = 0;
    virtual void unloadMesh(void** meshBackendData) = 0;

    virtual void loadShader(void ** shaderData, uint64_t featureMask) = 0;
    virtual void loadShaderInternal(void ** shaderData, const std::string& path, unsigned int shaderType, const std::string& defines) = 0;
    virtual void unloadShader(void **) = 0;

    virtual void loadShaderProgram(void **, const std::vector<RefCountPtr<Shader> >& shaderList) = 0;
    virtual void unloadShaderProgram(void **) = 0;
};

}

#endif