#include <VmbCPP/VmbCPP.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace VmbCPP;

//TODO:     Find a function for me to set pixel formats
//          Find a function to help set exposure levels and other editing of the camera format

int main() {
    // Start VimbaX
    VmbSystem& system = VmbSystem::GetInstance();
    system.Startup();

    // Get first camera
    CameraPtrVector cameras; // List of all cameras found by VimbaX
    system.GetCameras(cameras);
    if (cameras.empty()) {
        std::cout << "No cameras found" << std::endl;
        system.Shutdown();
        return 1;
    }
    CameraPtr camera1object = cameras[0]; // Initialises a "smart pointer" to the CameraPtr Object

    // Setup camera configuration
    camera1object->Open(VmbAccessModeFull);
    FeaturePtr pixelFormatFeature;

    std::cout << "Capturing frames every second..." << std::endl;

    // Capture 5 frames
    for (int i = 1; i <= 5; ++i) {
        FramePtr frame;
        camera1object->AcquireSingleImage(frame, 2000);  // 2 second timeout. Runs the member function AcquireSingleImage

        // Get raw pixel data
        VmbUchar_t* buffer;
        VmbUint32_t size;
        frame->GetImage(buffer); // Sets buffer as a pointer to array inside FramePtr object
        frame->GetBufferSize(size); // Directly writes the number of bytes to size

        // Print first 20 pixel values
        std::cout << "Frame " << i << ": ";
        for (int j = 0; j < 20 && j < size; j++) {
            std::cout << (int)buffer[j] << " ";
        }
        std::cout << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cleanup
    camera1object->Close();
    system.Shutdown();

    return 0;
}