#include "view/SSAOView.h"

#include <OgreTechnique.h>
#include <OgreCamera.h>
#include <OgreCompositorManager.h>

#include <sstream>
#include <set>
#include <cmath>

namespace Rally { namespace View {

    SSAOView::SSAOView() :
            viewport(NULL),
            car(NULL),
            blurBaseWeight(0.20f),
            blurWeights(0.18f, 0.12f, 0.6f, 0.4f),
            blurHorizontalOffsets(0.001f, 0.002f, 0.003f, 0.004f),
            blurVerticalOffsets(0.001f, 0.002f, 0.003f, 0.004f) {
    }

    SSAOView::~SSAOView() {
        ssaoSelectShaderParameters.setNull();
        blurHorizontalShaderParameters.setNull();
        blurVerticalShaderParameters.setNull();
    }

    void SSAOView::attachTo(Ogre::Viewport* viewport, const Rally::Model::Car* car) {
        this->viewport = viewport;
        this->car = car;

        compositorInstance = Ogre::CompositorManager::getSingleton().addCompositor(viewport, "compositors/SSAOCompositor");
        if(compositorInstance == NULL) {
            throw std::runtime_error("Compositor for SSAO could not be instantiated.");
        }

        compositorInstance->addListener(this);
    }

    void SSAOView::setEnabled(bool enabled) {
        Ogre::CompositorManager::getSingleton().setCompositorEnabled(viewport, "compositors/SSAOCompositor", enabled);
    }

    void SSAOView::detach() {
        ssaoSelectShaderParameters.setNull();
        blurHorizontalShaderParameters.setNull();
        blurVerticalShaderParameters.setNull();
        Ogre::CompositorManager::getSingleton().removeCompositor(viewport, "compositors/SSAOCompositor");
    }

    void SSAOView::notifyMaterialSetup(Ogre::uint32 compositorPassId, Ogre::MaterialPtr& clonedMaterial) {
        if(compositorPassId == 100) {
            ssaoSelectShaderParameters = clonedMaterial->getBestTechnique()->getPass(0)->getFragmentProgramParameters();
        } else if(compositorPassId == 200) {
            blurHorizontalShaderParameters = clonedMaterial->getBestTechnique()->getPass(0)->getFragmentProgramParameters();
        } else if(compositorPassId == 300) {
            blurVerticalShaderParameters = clonedMaterial->getBestTechnique()->getPass(0)->getFragmentProgramParameters();
        }
    }

    void SSAOView::notifyMaterialRender(Ogre::uint32 compositorPassId, Ogre::MaterialPtr& clonedMaterial) {
        if(compositorPassId == 100) {
            float effectFactor = car->getEffectFactor();

            ssaoSelectShaderParameters->setNamedConstant("sceneProjectionMatrix",
                viewport->getCamera()->getProjectionMatrix());
            ssaoSelectShaderParameters->setNamedConstant("effectFactor", effectFactor);

            recalculateBlur(effectFactor);
        } else if(compositorPassId == 200) {
            blurHorizontalShaderParameters->setNamedConstant("baseWeight", blurBaseWeight);
            blurHorizontalShaderParameters->setNamedConstant("weights", blurWeights);
            blurHorizontalShaderParameters->setNamedConstant("offsets", blurHorizontalOffsets);
        } else if(compositorPassId == 300) {
            blurVerticalShaderParameters->setNamedConstant("baseWeight", blurBaseWeight);
            blurVerticalShaderParameters->setNamedConstant("weights", blurWeights);
            blurVerticalShaderParameters->setNamedConstant("offsets", blurVerticalOffsets);
        }
    }

    void SSAOView::recalculateBlur(float effectFactor) {
        // Thui is more or less a copy with some tweaks of the blur used for bloom.

        float pixelWidth = 1.0f / viewport->getActualWidth();
        float pixelHeight = 1.0f / viewport->getActualHeight();

        effectFactor *= effectFactor;

        float sigma = 2.5f - effectFactor;

        // This is an optimized version of the normal distribution.
        // It's not entirely accurate (up to the tail after the highest x.
        // All linear factors are removed, as the values will be normalized anyway.
        // Some JavaScript for your convenience (run it in the web console):
        // var weights = [-4, -3, -2, -1, 0, 1, 2, 3, 4].map(function(x){ var my = 0; var sigma=1.4*1.4; return Math.pow(Math.E, -(x-my)*(x-my)/(2*sigma*sigma))/(sigma*Math.sqrt(2*Math.PI)); }).map(function(x){ return Math.round(x*100); });weights;
        // Array [ 3, 6, 12, 18, 20, 18, 12, 6, 3 ]
        // weights.reduce(function(previous, current){ return previous+current; })
        // 98
        // The sum should be 100, so we normalize. Also, we sub-sample between pixels
        // and let opengl interpolate the values so that we need half of the weights.
        // Of course, the weights are mirrored for both sides of pixel 0, and both directions.
        float weights[9];
        float weightSum = 0;
        float base = 0.1f; // This is arbitrary, should be 0 < base < 1.
        for(int x = 0; x < 9; x++) {
            weights[x] = std::exp(-x*x/(2*sigma*sigma));
            weightSum += weights[x];
        }

        weightSum = 2.0f*weightSum - weights[0]; // Mirror it, as in -9, -8, ..., -2, 0, 1, ... 8, 9.
        float normalizationFactor = 1.0f / weightSum;
        for(int x = 0; x < 9; x++) {
            weights[x] *= normalizationFactor;
        }

        blurBaseWeight = weights[0];
        for(int x = 1; x < 9; x += 2) {
            int blurIndex = x / 2;

            float factor = weights[x] + weights[x+1];
            blurWeights[blurIndex] = factor;// + 0.3f*factor*effectFactor; // Some extra effect factor ;)

            float normalizedOffset = (x + factor/weights[x]);
            blurHorizontalOffsets[blurIndex] = normalizedOffset*pixelWidth;
            blurVerticalOffsets[blurIndex] = normalizedOffset*pixelHeight;
        }
    }

} }
