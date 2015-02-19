
#ifndef __BaseInputManager_h_
#define __BaseInputManager_h_

#include <OgreCamera.h>
#include <OgreEntity.h>
#include <OgreLogManager.h>
#include <OgreRoot.h>
#include <OgreViewport.h>
#include <OgreSceneManager.h>
#include <OgreRenderWindow.h>
#include <OgreConfigFile.h>

#include <OISEvents.h>
#include <OISInputManager.h>
#include <OISKeyboard.h>
#include <OISMouse.h>

#include <SdkTrays.h>
#include <SdkCameraMan.h>

class BaseInputManager : public Ogre::FrameListener, public OIS::KeyListener, public OIS::MouseListener
{
public:
    BaseInputManager(void);
    virtual ~BaseInputManager(void);

    virtual void go(void);

protected:
    virtual bool setup(Ogre::RenderWindow* rw);
    virtual void createFrameListener(Ogre::RenderWindow* rw);
    virtual void destroyScene(void);
    virtual void createResourceListener(void);

    // Ogre::FrameListener
    virtual bool frameRenderingQueued(const Ogre::FrameEvent& evt);

    // OIS::KeyListener
    virtual bool keyPressed( const OIS::KeyEvent &arg );
    virtual bool keyReleased( const OIS::KeyEvent &arg );
    // OIS::MouseListener
    virtual bool mouseMoved( const OIS::MouseEvent &arg );
    virtual bool mousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id );
    virtual bool mouseReleased( const OIS::MouseEvent &arg, OIS::MouseButtonID id );

    // Ogre::WindowEventListener
    //Adjust mouse clipping area
    virtual void windowResized(Ogre::RenderWindow* rw);
    //Unattach OIS before window shutdown (very important under Linux)
    virtual void windowClosed(Ogre::RenderWindow* rw);

    Ogre::Root *root;
    Ogre::Camera* mCamera;
    Ogre::SceneManager* mSceneMgr;
    Ogre::RenderWindow* mWindow;
    Ogre::String mResourcesCfg;
    Ogre::String mPluginsCfg;

	//Fix for 1.9
	//Ogre::OverlaySystem *mOverlaySystem;

    // OgreBites
    //OgreBites::SdkTrayManager* mTrayMgr;
    //OgreBites::SdkCameraMan* mCameraMan;       // basic camera controller
    //OgreBites::ParamsPanel* mDetailsPanel;     // sample details panel
   
	//Fix for 1.9:
	//OgreBites::InputContext mInputContext;
	
	bool mCursorWasVisible;                    // was cursor visible before dialog appeared
    bool mShutDown;

    //OIS Input devices
    OIS::InputManager* mInputManager;
    OIS::Mouse*    mMouse;
    OIS::Keyboard* mKeyboard;
};

#endif // #ifndef __BaseInputManager_h_