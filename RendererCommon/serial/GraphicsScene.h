#ifndef ILL_GRAPHICS_SCENE_H_
#define ILL_GRAPHICS_SCENE_H_

#include <stdint.h>
#include <set>
#include <unordered_set>

#include "Util/Geometry/GridVolume3D.h"
#include "Util/Geometry/SceneNode.h"
#include "Util/serial/Array.h"
#include "Util/Geometry/BoxIterator.h"
#include "Util/Geometry/BoxOmitIterator.h"
#include "RendererCommon/serial/GraphicsNode.h"

#include "Logging/logging.h"

template<typename Id, typename T, typename LoadArgs, typename Loader> class ConfigurableResourceManager;

namespace illGraphics {
class Camera;
class GraphicsBackend;
struct LightBase;

class Mesh;
struct MeshLoadArgs;
typedef uint32_t MeshId;
typedef ConfigurableResourceManager<MeshId, Mesh, MeshLoadArgs, GraphicsBackend> MeshManager;

class Material;
struct MaterialLoadArgs;
typedef uint32_t MaterialId;
typedef ConfigurableResourceManager<MaterialId, Material, MaterialLoadArgs, GraphicsBackend> MaterialManager;
}

namespace illRendererCommon {

class RendererBackend;
class LightNode;

/**
The base graphics scene.
More docs to come.
*/
class GraphicsScene {
public:
    typedef std::unordered_set<GraphicsNode*> NodeContainer;
    typedef Array<GraphicsNode*> StaticNodeContainer;

    typedef std::unordered_set<LightNode*> LightNodeContainer;
    typedef Array<LightNode*> StaticLightNodeContainer;
    
    virtual inline ~GraphicsScene() {
        delete[] m_sceneNodes;
        delete[] m_staticSceneNodes;
        delete[] m_lightNodes;
        delete[] m_staticLightNodes;
    }

    /**
    Render a scene from a camera angle.  This interacts with the renderer backend directly.
    */
    virtual void render(const illGraphics::Camera& camera) = 0;

    /**
    Returns the grid volume that is used to manage visibility in the scene.
    */
    const GridVolume3D<>& getGridVolume() const {
        return m_grid;
    }

    /**
    Returns the finer grid volume for assisting with spatial queries between objects.
    */
    const GridVolume3D<>& getInteractionGridVolume() const {
        return m_interactionGrid;
    }

    /**
    Returns a reference to the collection of scene nodes at a given cell array index in the visibility culling grid.
    If you have a cell grid index (a 3 element vector) you can call getGridVolume to help convert that into an array index.
    */
    const NodeContainer& getSceneNodeCell(size_t cellArrayIndex) const {
        return m_sceneNodes[cellArrayIndex];
    }

    /**
    Returns a reference to the collection of static scene nodes at a given cell array index in the visibility culling grid.
    If you have a cell grid index (a 3 element vector) you can call getGridVolume to help convert that into an array index.
    */
    const StaticNodeContainer& getStaticNodeCell(size_t cellArrayIndex) const {
        return m_staticSceneNodes[cellArrayIndex];
    }

    /**
    Returns a reference to the collection of light nodes at a given cell array index in the interaction grid.
    If you have a cell grid index (a 3 element vector) you can call getInteractionGridVolume to help convert that into an array index.
    */
    const LightNodeContainer& getLightCell(size_t cellArrayIndex) const {
        return m_lightNodes[cellArrayIndex];
    }

    /**
    Returns a reference to the collection of static light nodes at a given cell array index in the interaction grid.
    If you have a cell grid index (a 3 element vector) you can call getInteractionGridVolume to help convert that into an array index.
    */
    const StaticLightNodeContainer& getStaticLightCell(size_t cellArrayIndex) const {
        return m_staticLightNodes[cellArrayIndex];
    }

    /**
    Gets lights within a bounding box by querying the lights interaction grid.

    @param boundingBox The bounding box to get intersections.
    @param destination The destination set of where to write the intersections.
    */
    void getLights(const Box<>& boundingBox, std::set<LightNode*>& destination) const;

protected:
    /**
    Creates the scene and its 3D uniform grid.

    @param cellDimensions The dimensions of the grid cells in world units.
    @param cellNumber The number of cells in each dimension.
    */
    inline GraphicsScene(RendererBackend * rendererBackend,
            illGraphics::MeshManager * meshManager, illGraphics::MaterialManager * materialManager,
            const glm::vec3& cellDimensions, const glm::uvec3& cellNumber,
            const glm::vec3& interactionCellDimensions, const glm::uvec3& interactionCellNumber,
            bool trackLightsInVisibilityGrid)
        : m_meshManager(meshManager),
        m_materialManager(materialManager),
        m_rendererBackend(rendererBackend),
        m_accessCounter(0),
        m_grid(cellDimensions, cellNumber),
        m_interactionGrid(interactionCellDimensions, interactionCellNumber),
        m_trackLightsInVisibilityGrid(trackLightsInVisibilityGrid)
    {
        m_sceneNodes = new NodeContainer[cellNumber.x * cellNumber.y * cellNumber.z];
        m_staticSceneNodes = new StaticNodeContainer[cellNumber.x * cellNumber.y * cellNumber.z];
        m_lightNodes = new LightNodeContainer[interactionCellNumber.x * interactionCellNumber.y * interactionCellNumber.z];
        m_staticLightNodes = new StaticLightNodeContainer[interactionCellNumber.x * interactionCellNumber.y * interactionCellNumber.z];
    }

private:
    /**
    If true tracks lights in both the lights grid and the main scene grid.
    If false, tracks lights only in the lights grid.

    Forward rendering should leave this as false, and deferred shading should leave this as true.
    */
    bool m_trackLightsInVisibilityGrid;

    /**
    */
    void addNode(GraphicsNode * node);

    /**
    */
    void removeNode(GraphicsNode * node);

    /**
    */
    void moveNode(GraphicsNode * node, const Box<>& prevBounds);

protected:
    RendererBackend * m_rendererBackend;

    illGraphics::MeshManager * m_meshManager;
    illGraphics::MaterialManager * m_materialManager;

    /**
    The counter that keeps track of which nodes have been accessed in some way during some call.
    This is to handle nodes that overlap multiple cells in the grid and keeps them from being processed more than once.
    */
    mutable uint64_t m_accessCounter;
    
    /**
    The 3D uniform grid for the scene.
    This grid is more sparce and is used for the visibility computation.
    */
    GridVolume3D<> m_grid;

    /**
    An additional finer grid for keeping track of interactions with other objects.
    This would speed up spatial queries to find all surrounding lights for example.
    */
    GridVolume3D<> m_interactionGrid;

    /**
    The scene nodes for each cell managed by the main visibility grid.
    */
    NodeContainer * m_sceneNodes;

    /**
    The static scene nodes for each cell managed by the main visibility grid.
    */
    StaticNodeContainer * m_staticSceneNodes;

    /**
    The light nodes for each cell managed by the interaction grid.

    Lights are also kept track of in the scene cells structure as well if trackLightsInMain is true.
    */
    LightNodeContainer * m_lightNodes;

    /**
    The static light nodes for each cell managed by the interaction grid.

    Lights are also kept track of in the static scene cells structure as well if trackLightsInMain is true.
    */
    StaticLightNodeContainer * m_staticLightNodes;

    friend class GraphicsNode;
};

}

#endif