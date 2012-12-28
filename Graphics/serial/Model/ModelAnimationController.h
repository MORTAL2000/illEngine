#ifndef __MODEL_ANIMATION_CONTROLLER_H__
#define __MODEL_ANIMATION_CONTROLLER_H__

#include <map>

#include <glm/glm.hpp>
#include "serial-illUtil/RefCountPtr.h"

namespace Graphics {

class Skeleton;
class SkeletonAnimation;

//TODO: put this together right, at the moment it's just quickly thrown together and doesn't even support blending
//This is in no way how it'll be in the end
class ModelAnimationController {
public:
    struct Animation {

    };

    ModelAnimationController()
        : m_skeleton(NULL),
        m_animation(NULL),
        m_animTime(0.0f)
    {}

    inline void alloc(unsigned int numBones) {
        m_skelMats = new glm::mat4[numBones];
    }

    ~ModelAnimationController() {
        delete[] m_skelMats;
    }

    void update(float seconds);         //it's fine to do this every update since this would be also used for transforming the hitboxes and 
    void computeAnimPose();

    float m_animTime;

    Skeleton * m_skeleton;
    SkeletonAnimation * m_animation;

    std::map<unsigned int, glm::mat4> m_animationTest;      //This is horrible
    glm::mat4 * m_skelMats;                                 //This is the bone array that goes to the shader
};

}

#endif