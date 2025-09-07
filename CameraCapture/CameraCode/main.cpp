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
    VmbError_t err = system.Startup();


    CameraPtr camera1;
    err = system.OpenCameraByID("169.254.24.139", VmbAccessModeFull, camera1); // OpenCameraById returns an error. If not error, will edit camera1 object to read only from ID
    if (VmbErrorSuccess != err) {
        std::cerr << "Failed to open camera: " << err << std::endl;
        system.Shutdown();
        return -1;
    }
    FeaturePtr pixelFormatFeature;

    std::cout << "Capturing frames every second..." << std::endl;

    // Capture 5 frames
    for (int i = 1; i <= 5; ++i) {
        FramePtr frame;
        camera1->AcquireSingleImage(frame, 2000);  // 2 second timeout. Runs the member function AcquireSingleImage

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
    camera1->Close();
    system.Shutdown();

    return 0;
}