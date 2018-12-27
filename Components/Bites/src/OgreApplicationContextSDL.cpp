// This file is part of the OGRE project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at https://www.ogre3d.org/licensing.

#include "OgreApplicationContext.h"

#include "OgreRoot.h"
#include "OgreRenderWindow.h"

#include <SDL.h>
#include <SDL_video.h>
#include <SDL_syswm.h>

#include "SDLInputMapping.h"

namespace OgreBites {

ApplicationContextSDL::ApplicationContextSDL(const Ogre::String& appName) : ApplicationContextBase(appName)
{
}

void ApplicationContextSDL::addInputListener(NativeWindowType* win, InputListener* lis)
{
    mInputListeners.insert(std::make_pair(SDL_GetWindowID(win), lis));
}


void ApplicationContextSDL::removeInputListener(NativeWindowType* win, InputListener* lis)
{
    mInputListeners.erase(std::make_pair(SDL_GetWindowID(win), lis));
}

NativeWindowPair ApplicationContextSDL::createWindow(const Ogre::String& name, Ogre::uint32 w, Ogre::uint32 h, Ogre::NameValuePairList miscParams)
{
    NativeWindowPair ret = {NULL, NULL};
    parseWindowOptions(w, h, miscParams);

    Ogre::ConfigOptionMap& ropts = mRoot->getRenderSystem()->getConfigOptions();

    if(!SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_InitSubSystem(SDL_INIT_VIDEO);
    }

    Uint32 flags = SDL_WINDOW_RESIZABLE;

    if(ropts["Full Screen"].currentValue == "Yes"){
       flags = SDL_WINDOW_FULLSCREEN;
    } else {
       flags = SDL_WINDOW_RESIZABLE;
    }

    ret.native = SDL_CreateWindow(name.c_str(),
                                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, flags);

#if OGRE_PLATFORM == OGRE_PLATFORM_EMSCRIPTEN
    SDL_GL_CreateContext(ret.native);
    miscParams["currentGLContext"] = "true";
#else
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(ret.native, &wmInfo);
#endif

#if OGRE_PLATFORM == OGRE_PLATFORM_LINUX
    miscParams["parentWindowHandle"] = Ogre::StringConverter::toString(size_t(wmInfo.info.x11.window));
#elif OGRE_PLATFORM == OGRE_PLATFORM_WIN32
    miscParams["externalWindowHandle"] = Ogre::StringConverter::toString(size_t(wmInfo.info.win.window));
#elif OGRE_PLATFORM == OGRE_PLATFORM_APPLE
    assert(wmInfo.subsystem == SDL_SYSWM_COCOA);
    miscParams["externalWindowHandle"] = Ogre::StringConverter::toString(size_t(wmInfo.info.cocoa.window));
#endif

    ret.render = mRoot->createRenderWindow(name, w, h, false, &miscParams);
    mWindows.push_back(ret);
    return ret;
}

void ApplicationContextSDL::setWindowGrab(NativeWindowType* win, bool _grab)
{
    SDL_bool grab = SDL_bool(_grab);

    SDL_SetWindowGrab(win, grab);
    SDL_SetRelativeMouseMode(grab);
}

void ApplicationContextSDL::shutdown()
{
    ApplicationContextBase::shutdown();

    for(WindowList::iterator it = mWindows.begin(); it != mWindows.end(); ++it)
    {
        if(it->native)
            SDL_DestroyWindow(it->native);
    }
    if(!mWindows.empty()) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    mWindows.clear();
}

void ApplicationContextSDL::pollEvents()
{
    if(mWindows.empty())
    {
        // SDL events not initialized
        return;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            mRoot->queueEndRendering();
            break;
        case SDL_WINDOWEVENT:
            if(event.window.event != SDL_WINDOWEVENT_RESIZED)
                continue;

            for(WindowList::iterator it = mWindows.begin(); it != mWindows.end(); ++it)
            {
                if(event.window.windowID != SDL_GetWindowID(it->native))
                    continue;

                Ogre::RenderWindow* win = it->render;
                win->windowMovedOrResized();
                windowResized(win);
            }
            break;
        default:
            _fireInputEvent(convert(event), event.window.windowID);
            break;
        }
    }

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
    // hacky workaround for black window on OSX
    for(const auto& win : mWindows)
    {
        SDL_SetWindowSize(win.native, win.render->getWidth(), win.render->getHeight());
        win.render->windowMovedOrResized();
    }
#endif
}

}