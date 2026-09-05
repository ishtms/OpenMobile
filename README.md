# OpenMobile

**Android and iOS plugins for Unreal Engine**

OpenMobile is a collection of mobile plugins I've built up over years of working with Unreal Engine.

Getting these things right can be frustrating. An ad, a vibration, or a sensor reading sounds simple until you're dealing with SDK setup, native code, app lifecycle issues, and differences between Android and iOS.

I'm open sourcing this because I want Unreal Engine to be better on mobile. I've spent a lot of time working through these problems, and I'd like that work to be useful to other developers too.

## What's included

| Feature | What you can do |
| --- | --- |
| [Ads](Services/OpenMobileAds) | Show ads through AdMob, handle consent, and add mediation networks. |
| [Haptics](Native/OpenMobileHaptics) | Create vibration patterns and play feedback from gameplay, UMG, Gameplay Abilities, or Sequencer. |
| [Sensors](Native/OpenMobileSensors) | Read motion, orientation, and other device sensors, with recording and replay for supported streams. |
| [Device info](Native/OpenMobileDevice) | Check battery and storage, monitor thermal and network status, and read display information. |
| [Permissions](Foundation/OpenMobilePermissions) | Check and request permissions from your game. |
| [Photo picker](Native/OpenMobileMedia) | Let players pick a photo through the native system picker and get an Unreal texture back. |

[AdMob mediation adapters](Adapters/Ads) are included for AppLovin, Chartboost, Liftoff Monetize, Meta, and Unity Ads.

Each feature is a separate plugin with Blueprint and C++ APIs. Enable the ones your project needs and add others when you need them.

## Getting started

The current plugins target **Unreal Engine 5.8**. Place this repository in your project's `Plugins/OpenMobile` folder, then enable the plugins you want in Unreal's Plugin Browser and build your project.

Plugin settings live under **Project Settings > OpenMobile**. Features such as ads also need provider setup. Try things on a real Android or iOS device, since available features depend on the hardware and OS.

## Help make it better

If you use OpenMobile in a project, I'd love to hear how it goes. Fixes, examples, and testing on different phones are all welcome.

If something breaks, [open an issue](/ishtms/OpenMobile/issues) with your Unreal version, device, and what happened. If you've worked out a fix, [send a pull request](/ishtms/OpenMobile/pulls).
