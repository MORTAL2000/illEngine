#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <new>

#include "ModelAnimationController.h"
#include "Skeleton.h"
#include "SkeletonAnimation.h"

namespace illGraphics {

float ModelAnimationController::updateInternal(float seconds) {
    m_animations[0].m_animTime += seconds;
    m_animations[1].m_animTime += seconds;

    m_transitionWeight += m_transitionDelta * seconds;

    //now the transition is over
    if(m_transitionWeight >= 1.0f) {
        m_currentAnimation = !m_currentAnimation;
		m_transitionDelta = 0.0f;
        m_transitionWeight = 0.0f;
    }

    //find when the next transition should happen
    if(!m_transitionQueue.empty()) {
        const Transition& transition = m_transitionQueue.front();
        
        if((m_transitionDelta != 0.0f && m_animations[!m_currentAnimation].m_animTime >= transition.m_triggerTime)
            || (m_transitionDelta == 0.0f && m_animations[m_currentAnimation].m_animTime >= transition.m_triggerTime)) {
            m_transitionDelta = transition.m_transitionSeconds == 0.0f 
				? 0.0f 
				: 1.0f / transition.m_transitionSeconds;
			
			Animation& nextAnim = m_animations[!m_currentAnimation];
            
            nextAnim.m_animation = transition.m_animation;
            nextAnim.m_animTime = transition.m_beginTime;
            nextAnim.m_lastFrameInfo.clear();
            
			m_transitionWeight = 0.0f;

			if(transition.m_transitionSeconds == 0.0f) {
				m_currentAnimation = !m_currentAnimation;
			}

			float transitionTriggerTime = transition.m_triggerTime;
			m_transitionQueue.pop();

			//return the overtime in the transition as the remaining time for the next update
			return m_animations[m_currentAnimation].m_animTime - transitionTriggerTime;
        }
    }

	return 0.0f;
}

void ModelAnimationController::computeAnimPose(glm::mat4 * skelMats) {
    updateSkeleton(skelMats, m_skeleton->getRootBoneNode(), glm::mat4());
}

void ModelAnimationController::updateSkeleton(glm::mat4 * skelMats, const illGraphics::Skeleton::BoneHeirarchy * currNode, glm::mat4 currXform) {	
	Transform<> transform[2];
    
    //TODO: there's currently a bug related to using relative transforms in animations, for now it's using absolute transforms but decomposing the bind pose xform
    //should fix soon, the decomposing isn't very good for performance

	//for the primary animation
	if(m_animations[m_currentAnimation].m_animation) {
		/*transform[0] = m_animations[m_currentAnimation].m_animation->getTransform(currNode->m_boneIndex,
			m_animations[m_currentAnimation].m_animTime,
			m_animations[m_currentAnimation].m_lastFrameInfo[currNode->m_boneIndex]);*/

        if(!m_animations[m_currentAnimation].m_animation->getTransform(currNode->m_boneIndex,
			m_animations[m_currentAnimation].m_animTime,
            transform[0],
			m_animations[m_currentAnimation].m_lastFrameInfo[currNode->m_boneIndex])) {

            transform[0].set(m_skeleton->getBone(currNode->m_boneIndex).m_relativeTransform);
        }
	}
	else {
		//new(&transform[0]) Transform<>();

        transform[0].set(m_skeleton->getBone(currNode->m_boneIndex).m_relativeTransform);
	}

	//for the secondary animation, see if the bone is in the animation
	if(m_transitionWeight > 0.0f) {
		if(m_animations[!m_currentAnimation].m_animation) {
			/*transform[1] = m_animations[!m_currentAnimation].m_animation->getTransform(currNode->m_boneIndex,
				m_animations[!m_currentAnimation].m_animTime,
				m_animations[!m_currentAnimation].m_lastFrameInfo[currNode->m_boneIndex]);*/

            if(!m_animations[!m_currentAnimation].m_animation->getTransform(currNode->m_boneIndex,
			    m_animations[!m_currentAnimation].m_animTime,
                transform[1],
			    m_animations[!m_currentAnimation].m_lastFrameInfo[currNode->m_boneIndex])) {

                transform[1].set(m_skeleton->getBone(currNode->m_boneIndex).m_relativeTransform);
            }
		}
		else {
			//new(&transform[1]) Transform<>();

            transform[1].set(m_skeleton->getBone(currNode->m_boneIndex).m_relativeTransform);
		}

		//now blend them
		transform[0] = transform[0].interpolate(transform[1], m_transitionWeight);
	}
	
	currXform = currXform * /*m_skeleton->getBone(currNode->m_boneIndex).m_relativeTransform **/ transform[0].getMatrix();

    skelMats[currNode->m_boneIndex] = currXform * m_skeleton->getBone(currNode->m_boneIndex).m_offsetTransform
        * glm::rotate(-90.0f, glm::vec3(1.0f, 0.0, 0.0f));     //TODO: for now hardcoded to rotate this -90 degrees around x since all md5s seem to be flipped
                                                                //figure out how to export models in the right orientation
                                                                //THIS!  is why there's the horrible hack code below and why it took me days to get this working, 1 tiny mistake and it all explodes
    
	for(std::vector<illGraphics::Skeleton::BoneHeirarchy *>::const_iterator iter = currNode->m_children.begin(); iter != currNode->m_children.end(); iter++) {
        updateSkeleton(skelMats, *iter, currXform);
    }
}

}