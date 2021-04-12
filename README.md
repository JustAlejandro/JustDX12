# JustDX12
JustDX12 is a testing area in C++ for various DirectX12 rendering techniques, with a primary focus on abstracting away the book keeping required to program for DirectX at a low level (memory management, descriptor binding, etc.), but maintaining the bottom level access required to experiment with techniques or strategies that you don't necessarily get with a high level rendering API abstraction.

At a high level, this project can be thought of as a DirectX wrapper, somewhere between an actual game engine's graphics API wrapper and a Microsoft DX12 code sample.

## Usage
This project is more meant to emulate a game engine's rendering pipeline than it is to actually be any fun to play, so controls are fairly simple.
- WASD: Movement
- Mouse: Look around
- Shift: Sprint
- Ctrl: Toggle between Mouse movement controlling camera and mouse movement showing a cursor to interact with the Settings UI
- Esc: Exit program.

To load and unload scenes just use the model submenu in the UI.

Some scenes and example scene files are supplied in the Required Files, which is the way JustDX12 handles loading and unloading of multiple models at once. Ex: `defaultScene.csv` contains the bistro scene as a single model and `bistroSeperated.csv` contains the bistro with each mesh as it's own model, and was used for testing.

## Hardware Requirements
JustDX12 is primarily being used as a testing area at the moment for DirectX12 Ultimate, using DXR 1.1, Variable Rate Shading Tier 2, and Mesh Shaders. Because of this, any hardware not supporting all those features will not work with JustDX12.

### Verified Working Hardware
 - RTX 3090
 - RTX 2080Ti

## Current Feature Set
 - DXR 1.1 Shadows
   - Supporting transparency masking
 - DXR 1.1 Reflections
   - Reflection are still early in implementation, so exist more as a proof of concept than as a physically based reliable system.
 - Image Based Variable Rate Shading
   - Based on AMD's [FidelityFX VRS](https://github.com/GPUOpen-Effects/FidelityFX-VariableShading), with simplifications and ability to decrease detail on an Average Luminance value per tile.
 - Mesh Shaders (Broken at the moment, but refactoring to work with DXR)
   - Mostly a retooling of the Microsoft's [Mesh Shaders Sample](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12MeshShaders), but with improvements to the culling cone baking procedure (to be uploaded on a seperate repo soon).
 - Async Compute Queue
   - Framework is setup to support asynchronous workloads
     - Unfortunately NVidia hardware doesn't opt for async when workloads are heavy, so not fully verified as working.
 - Heavy multi-threading of command list building
   - Current implementation calls for each 'stage' of the rendering process to be backed by a thread that builds the command list that performs the actions it desires (raster/compute).
     - Result of this is that the CPU loop is very fast, but consumes a lot of threads.
 - Generalized DirectX threads
   - Model loading and processing happens on a seperate thread and uses the Copy queue to prevent hanging waiting for model loading
     - Same strategy is used for texture loading, so texture loading, model loading, and rendering can all happen simultaneously.
 - SSAO
   - Rudimentary SSAO implementation with a set of random vectors per pixel to keep the result stable over time.
 - Screen Space Shadows
   - Simple raycast in screen space checking for intersection within a cutoff value. Planned to be used in future to assist other effects, not to exist on it's own as it does now.

## Images

![img](https://i.imgur.com/v2oQkSj.jpg)
![img](https://i.imgur.com/tJSEzLF.jpg)

## Current Plans (in no particular order)
 - [ ] Restore mesh shader object functionality.
 - [ ] Publish better meshlet object exporter.
 - [ ] Restore ability for meshlet objects to be represented in RT structure.
 - [ ] Properly include reflections in lighting model (just pasted on top of direct lighting at the moment).
 - [ ] Begin work on DXR 1.0 reflections for recursive possibility.
 - [ ] Make multi-threading less brute force (queue -> thread pool) system, not raw (queue -> worker).
 - [ ] Add conventional shadow mapping techniques (redundant with DXR shadows, but would have much better performance, and a hybrid approach could be attempted)
 - [ ] Better resource management (right now all consistently CPU modified data is on the Upload heap, which is bad)
 - [ ] Make use of placed resources or manual suballocation so there isn't constant GPU resource allocation and deallocation of a small size.
 - [ ] Add some form of TAA (integration with that and VRS could result in cycling high detail pixels to reduce workload)
 - [ ] Better built in visualization/debug tools to reduce leaning on Pix for everything.
 - [ ] Calibrate lighting so it's not based on the "looks real enough" model.
   - [ ] Explore better models since Bistro model is lacking in material parameters and Genesis Scan isn't even PBR
 - [ ] Use IBL instead of basic 'ambient' lighting for closer approximation to reality.
   - [ ] Possibly exploring RT GI in comparison.

# Compile Instructions
This project has been built using Visual Studio 2019 using Nuget and vcpkg as package managers. Nuget is required to compile, but vcpkg is optional since its dependencies can be installed through an export to Nuget that I've supplied.

### Required Files:
- [Compile Dependencies](https://drive.google.com/file/d/1GxMN1-Gc9TFypeqS1WbTwGxA88JLyOxi/view?usp=sharing)
- [Models and Assets Large Size](https://drive.google.com/drive/folders/1rjROhcUtOFIjfCgME2v11kso1BDPMogQ?usp=sharing)
- [Models and Assets](https://drive.google.com/drive/folders/1WVsMfeiAx80l6BVxj-8mMCvyrbrxpe9i?usp=sharing): Includes a simple full screen squad, a simple reflective sphere, the Bistro scene from Amazon's Lumberyard GDC showcase, and the Genesis Scan obtained from The Royal Armoury (Livrustkammaren)
  - Bistro obtained through [NVIDIA's ORCA](https://developer.nvidia.com/orca/amazon-lumberyard-bistro), with texture sizes reduced to lower download size (optional)
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
