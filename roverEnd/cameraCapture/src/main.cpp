#include <VmbCPP/Include/VmbCPP.h>
#include <iostream>
#include <napi.h>
        
using namespace VmbCPP;

// =================== Bridging class the VimbaX will call, and will then call JScript ==============================
class FrameObserver : public IFrameObserver
{
private:
    Napi::ThreadSafeFunction tsfnJScriptCallback;
public:
   FrameObserver(CameraPtr pCamera, Napi::ThreadSafeFunction threadsafefunction);
   void FrameReceived(const FramePtr pFrame);
};

// the : is a constructor initialisor list. easier way than manually assigning all the pointers.
FrameObserver::FrameObserver(CameraPtr pCamera, Napi::ThreadSafeFunction tsfnJScriptCallback) : IFrameObserver(pCamera), tsfnJScriptCallback(tsfnJScriptCallback) {}

// Frame callback for processing what i want to do with each frame. Will 
void FrameObserver::FrameReceived(const FramePtr pFrame){

    VmbUchar_t* pBuffer; // Pointer to array beginning (char is 8 bit). VimbaX already created it. need to double check the camera settings or it might send nonsense
    VmbUint32_t bufferSize; // Stores the size of the buffer that is being pointed to

    pFrame->GetBuffer(pBuffer);
    pFrame->GetBufferSize(bufferSize);

    // Vector is a fancy array given by C++
    std::vector<uint8_t>* frameData = new std::vector<uint8_t>(pBuffer, pBuffer + bufferSize); // Beginning to end of array

    auto translateFunction = [](Napi::Env env, Napi::Function jsCallback, std::vector<uint8_t>* data){
        Napi::Buffer<uint8_t> buffer = Napi::Buffer<uint8_t>::Copy(env, data->data(), data->size());
        jsCallback.Call({buffer});
        delete data;
    };

    tsfnJScriptCallback.BlockingCall(frameData, translateFunction); // Blocking call will take the jscript parameters and a lambda function that tells it how to convert, then call the actual jscript function using the lambda function

   m_pCamera->QueueFrame(pFrame);
}


// ============================ VimbaX class to initialise in JScript ===================================
class VimbaXCamera : public Napi::ObjectWrap<VimbaXCamera> {
    private:
        VmbSystem& system;
        CameraPtr camera; // Need to figure out if we want 5 instances of the class, or 1 instance with 5 cameras in it
        std::shared_ptr<FrameObserver> myFrameObserver; // smart pointer. is nullptr currently

    public:
        VimbaXCamera(const Napi::CallbackInfo& info);

        Napi::Value StartCapture(const Napi::CallbackInfo& info);
};

// ================== Initialisation ================
VimbaXCamera::VimbaXCamera(const Napi::CallbackInfo& info) 
: Napi::ObjectWrap<VimbaXCamera>(info), system(VmbSystem::GetInstance()) {
    
    VmbError_t err = system.Startup();

    err = system.OpenCameraByID("169.254.24.139", VmbAccessModeFull, camera); // OpenCameraById returns an error. If not error, will edit camera object to read only from ID
    if (VmbErrorSuccess != err) {
        std::cerr << "Failed to open camera: " << err << std::endl;
        system.Shutdown();
    }
}

// =============================== Javascript calls to begin capture ======================
// This is a setup function, to make the FrameObserver act as a bridge between the two. VimbaX will call FrameRecieved, and FrameRecieved will call the javascript callback.
Napi::Value VimbaXCamera::StartCapture(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env(); // Info on the runtime enviroment

    Napi::Function jscriptCallback = info[0].As<Napi::Function>(); // Take the first input, turn it into a function

    Napi::ThreadSafeFunction tsfnJScriptCallback = Napi::ThreadSafeFunction::New( // Turn the callback function into threadsafe so that we can access the javascript function from the C++ thread
        env, // The runtime code, taken from the info
        jscriptCallback, // The javascript function that will be called in javascript file                 
        "FrameCallback",   // Debugging name that will appear in jscript logs       
        0,   // max queue size of functions                       
        1   // How many threads will call this function (just main for me)                      
    );
    
    // Create frame observer with the thread-safe function. Has to be shared because we need to be able to stop the camera later
    myFrameObserver = std::make_shared<FrameObserver>(camera, tsfnJScriptCallback);
    
    // Start capture. VimbaX gets this function, and calls FrameRecieved whenever a frame comes from it. 
    VmbError_t err = camera->StartContinuousImageAcquisition(
        5,  // Number of frames to queue
        IFrameObserverPtr(myFrameObserver)
    );

    return env.Undefined();
}

