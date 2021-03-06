#ifndef ILL_GRAPHICS_BACKEND_H__
#define ILL_GRAPHICS_BACKEND_H__

#include <glm/glm.hpp>
#include <stdint.h>
#include <vector>

#include "Util/serial/RefCountPtr.h"
#include "Util/Geometry/MeshData.h"

namespace illGraphics {

struct TextureLoadArgs;
class Shader;
class Camera;
class BitmapFont;

class GraphicsBackend {
public:
    virtual void initialize() = 0;
    virtual void uninitialize() = 0;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;

    //virtual void beginFontRender() = 0;
    //virtual void renderCharacter(const Camera& camera, const glm::mat4& transform, const BitmapFont &font, const glm::vec4& color, unsigned char character) = 0;

    virtual void loadTexture(void ** textureData, const TextureLoadArgs& loadArgs) = 0;
    virtual void unloadTexture(void ** textureData) = 0;
    
    virtual void loadMesh(void** meshBackendData, const MeshData<>& meshFrontendData) = 0;
    virtual void unloadMesh(void** meshBackendData) = 0;

    virtual void loadShader(void ** shaderData, uint64_t featureMask) = 0;
    virtual void loadShaderInternal(void ** shaderData, const char * path, unsigned int shaderType, const char * defines) = 0;
    virtual void unloadShader(void **) = 0;

    virtual void loadShaderProgram(void **, const std::vector<RefCountPtr<Shader> >& shaderList) = 0;
    virtual void unloadShaderProgram(void **) = 0;    
};

}

#endif
