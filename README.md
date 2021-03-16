# JustDX12
JustDX12 is a testing area in C++ for various DirectX12 rendering techniques, with a primary focus on abstracting away the book keeping required to program for DirectX at a low level (memory management, descriptor binding, etc.), but maintaining the bottom level access required to experiment with techniques or strategies that you don't necessarily get with a high level rendering API abstraction.

At a high level, this project can be thought of as a DirectX wrapper, somewhere between an actual game engine's graphics API wrapper and a Microsoft DX12 code sample.

## Compile Instructions
This project has been built using Visual Studio 2019 using Nuget and vcpkg as package managers. Nuget is required to compile, but vcpkg is optional since its dependencies can be installed through an export to Nuget that I've supplied.

### Required Files:
- [Compile Dependencies](https://drive.google.com/file/d/1GxMN1-Gc9TFypeqS1WbTwGxA88JLyOxi/view?usp=sharing)
- [Models and Assets](https://drive.google.com/file/d/1H9J5z3U2uTGl17eIUiBZDj89UbGF7P9t/view?usp=sharing): Includes a simple full screen squad, the Bistro scene from Amazon's Lumberyard GDC showcase, and the Genesis Scan obtained from The Royal Armoury (Livrustkammaren)
  - Bistro obtained through [NVIDIA's ORCA](https://developer.nvidia.com/orca/amazon-lumberyard-bistro), without modification
    - Licensed under Creative Commons [CC-BY 4.0](https://creativecommons.org/licenses/by/4.0/)
  - Genesis Scan (referred to in project as 'Head') obtained from [Sketchfab](https://sketchfab.com/3d-models/the-genesis-scan-94a02238914d4282bb1098950ae2506f), with decimation to reduce poly count, reduction in texture file size of model
    - Licensed under Attribution-ShareAlike 4.0 International [(CC BY-SA 4.0)](https://creativecommons.org/licenses/by-sa/4.0/)

### Steps to Compile:
1. Clone/Download the project wherever you want it.
2. Install Visual Studio 2019 with the "Desktop development with C++" component, making sure the "NuGet package manager" component is included.
4. Open the .sln file from the cloned repo.
5. In the Tools tab of Visual Studio, go to Nuget Package Manager and then click Package Manager Console
6. (64-bit only) Put the following command into the Package Manager Console: `Install-Package vcpkg-export-20210315-145514 -Source "DEPENDENCIES"`, where `DEPENDENCIES` is replaced with the path to the folder you downloaded called *CompileDependencies*, containing `vcpkg-export-20210315-145514.1.0.0.nupkg`
   - (32-bit only) Install [vcpkg](https://docs.microsoft.com/en-us/cpp/build/install-vcpkg?view=msvc-160&tabs=windows) and integrate it with your Visual Studio install.
   - (32-bit only) Install the following with vcpkg:
     - imgui[dx12-binding]:x64-windows
     - imgui[win32-binding]:x64-windows --recurse
     - assimp:x64-windows
     - directxtex[dx12]:x64-windows
7. At this point you should have all the dependencies needed to compile the 64-bit version.
8. Take the *dxcompiler.dll* and *dxil.dll* and put them in the same directory as your compiled `.exe` file.
9. Extract the contents of the folder downloaded called *RequiredModels* and place them into the *Models* folder.
10. Run the program, it should hang until the Bistro model is loaded (takes several seconds), at which point you should be dropped into the Bistro scene.

## Usage
This project is more meant to emulate a game engine's rendering pipeline than it is to actually be any fun to play, so controls are fairly simple.
- WASD: Movement
- Mouse: Look around
- Shift: Sprint
- Ctrl: Toggle between Mouse movement controlling camera and mouse movement showing a cursor to interact with the Settings UI
- Esc: Exit program (only applies when not in camera movement mode).
