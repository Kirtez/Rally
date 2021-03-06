#include "controller/MainController.h"
#include "model/Car.h"
#include "util/Timer.h"
#include "view/InputInit.h"
#include "view/SceneView.h"

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

        inputInit.setup();
    }

    void MainController::start() {
        Rally::Util::Timer frameTimer;

        sceneView.addLogicListener(*this);

        frameTimer.reset();
        while(true) {
            float deltaTime = frameTimer.getElapsedSeconds();

            // Allow max 1000 FPS for precision/stability reasons.
            // This is hopefully capped by vsync or atleast computation below.
            if(deltaTime < 0.01667f) {
                continue;
            }

            // Don't forget to do this AFTER the epsilon check above
            // (avoiding an infinite loop), but before anything else.
            frameTimer.reset();

            // ADD ANYTHING THAT'S NOT FRAME-TIMING CODE BELOW THIS LINE!

            // PLEASE CONSIDER ADDING YOUR CODE TO updateLogic()!
            // Performance penalty dragns lurk here.

            // TODO: Investigate in which order we'll do things (buffer up graphics commands, do some CPU, flip render buffers)
            if(!sceneView.renderFrame(deltaTime)) {
                return;
            }
        }
    }

    void MainController::updateLogic(float deltaTime) {
        netView.pullRemoteChanges();

        updateInput();

        world.update(deltaTime);

        netView.pushLocalChanges();
    }

    void MainController::updateInput() {
        Rally::Model::Car& car = world.getPlayerCar();

        // Accelerate and break
        car.setAccelerationRequested(inputInit.isKeyPressed("up") || inputInit.isKeyPressed("w"));
        car.setBreakingRequested(inputInit.isKeyPressed("down") || inputInit.isKeyPressed("s") || inputInit.isKeyPressed("x"));

        // Steering
        bool left = inputInit.isKeyPressed("left") || inputInit.isKeyPressed("a");
        bool right = inputInit.isKeyPressed("right") || inputInit.isKeyPressed("d");
        if(left && !right) {
            car.setSteeringRequested(1);
        } else if(!left && right) {
            car.setSteeringRequested(-1);
        } else {
            // neither left nor right, or left and right
            car.setSteeringRequested(0);
        }

        if(inputInit.isKeyPressedDebounced("i")) {
		    sceneView.toggleDebugDraw();
        }

        if(inputInit.isKeyPressedDebounced("f")) {
            world.gravityGlitch();
        }

        if(inputInit.isKeyPressedDebounced("space")) {
		    world.getPlayerCar().cycleCarType();
		    sceneView.playerCarTypeUpdated();
        }

        if(inputInit.isKeyPressedDebounced("o")) {
		    sceneView.toggleReflections();
        }

        if(inputInit.isKeyPressedDebounced("p")) {
		    sceneView.togglePostProcessing();
        }

        if(inputInit.isKeyPressed("u")) {
            std::cout << car.getPosition() << std::endl;
        }
		if(inputInit.isKeyPressedDebounced("k")) {
            sceneView.toggleKeyMenu();
        }
		if(inputInit.isKeyPressedDebounced("t")) {
            sceneView.toggleTrack();
			world.resetHighScore();
        }
    }

    void MainController_RemoteCarListener::carUpdated(unsigned short carId,
            Rally::Vector3 position,
            Rally::Quaternion orientation,
            Rally::Vector3 velocity,
            char carType,
            Rally::Vector4 tractionVector) {
        Rally::Model::RemoteCar& remoteCar = world.getRemoteCar(carId); // carId is upcast to int

        remoteCar.setTargetTransform(position, velocity, orientation);

        remoteCar.setTractionVector(tractionVector);

        bool carTypeChanged = false;
        if(remoteCar.getCarType() != carType) {
            remoteCar.setCarType(carType);
            carTypeChanged = true;
        }

        sceneView.remoteCarUpdated(carId, remoteCar, carTypeChanged);
    }

    void MainController_RemoteCarListener::carRemoved(unsigned short carId) {
        sceneView.remoteCarRemoved(carId, world.getRemoteCar(carId)); // carId is upcast to int

        world.removeRemoteCar(carId); // carId is upcast to int
    }
} }
