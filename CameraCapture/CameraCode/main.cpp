#include <VmbCPP/VmbCPP.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace VmbCPP;

// Setting up call back for queueing frames
class FrameObserver : public IFrameObserver
{
public:
   FrameObserver(CameraPtr pCamera);
   void FrameReceived(const FramePtr pFrame);
};
FrameObserver::FrameObserver(CameraPtr pCamera) : IFrameObserver(pCamera) {}

// Frame callback for processing what i want to do with each frame
void FrameObserver::FrameReceived(const FramePtr pFrame){
   m_pCamera->QueueFrame(pFrame);
}


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

    camera1->StartContinuousImageAcquisition(5, IFrameObserverPtr( new FrameObserver(camera1) ) );

    // Cleanup
    camera1->Close();
    system.Shutdown();

    return 0;
}