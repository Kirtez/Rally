#include "controller/MainController.h"
#include "model/Car.h"
#include "util/Timer.h"

namespace Rally { namespace Controller {
    MainController::MainController() :
            sceneView(SceneView(world)),
            remoteCarListener(MainController_RemoteCarListener(world, sceneView)),
            netView(View::RallyNetView(remoteCarListener)) {
    }

    MainController::~MainController() {
    }

    void MainController::initialize(std::string resourceConfigPath, std::string pluginConfigPath) {
        world.initialize("../../media/world.bullet");

        sceneView.initialize(resourceConfigPath, pluginConfigPath);

        netView.initialize(std::string("81.91.1.185"), 1337, &world.getPlayerCar());
    }

    void MainController::start() {
        Rally::Util::Timer frameTimer;

        frameTimer.reset();
        while(true) {
            float deltaTime = frameTimer.getElapsedSeconds();

            // Allow max 1000 FPS for precision/stability reasons.
            // This is hopefully capped by vsync or atleast computation below.
            if(deltaTime < 0.001f) {
                continue;
            }

            // Don't forget to do this AFTER the epsilon check above
            // (avoiding an infinite loop), but before anything else.
            frameTimer.reset();

            // ADD ANYTHING THAT'S NOT FRAME-TIMING CODE BELOW THIS LINE!

            netView.pullRemoteChanges();

            world.update(deltaTime);

            netView.pushLocalChanges();

            // TODO: Investigate in which order we'll do things (buffer up graphics commands, do some CPU, flip render buffers)
            if(!sceneView.renderFrame()) {
                return;
            }
        }
    }

    void MainController_RemoteCarListener::carUpdated(unsigned short carId,
            Rally::Vector3 position,
            Rally::Vector3 orientation,
            Rally::Vector3 velocity) {
        Rally::Model::RemoteCar& remoteCar = world.getRemoteCar(carId); // carId is upcast to int

        remoteCar.setTargetTransform(position, velocity, Rally::Vector3::NEGATIVE_UNIT_Z.getRotationTo(orientation));

        sceneView.remoteCarUpdated(carId, remoteCar);
    }

    void MainController_RemoteCarListener::carRemoved(unsigned short carId) {
        sceneView.remoteCarRemoved(carId, world.getRemoteCar(carId)); // carId is upcast to int

        world.removeRemoteCar(carId); // carId is upcast to int
    }

} }
