#include <VmbCPP/VmbCPP.h>
#include <iostream>
#include <napi.h>
#include <cstdint>
#include <vector>

#define FRONTCAMERA 1
#define BACKCAMERA 2
#define LEFTCAMERA 3
#define RIGHTCAMERA 4
#define MANIPCAMERA 5

using namespace VmbCPP;

class FrameObserver;
class VimbaXSystem;



// =================== Bridging class the VimbaX will call, and will then call JScript ==============================
class FrameObserver : public IFrameObserver
{
private:
    Napi::ThreadSafeFunction tsfnJScriptCallback;
    std::vector<uint8_t> convertYUV422toYUV420(VmbUchar_t* yuv422, uint32_t width, uint32_t height);
public:
   FrameObserver(CameraPtr pCamera, Napi::ThreadSafeFunction threadsafefunction);
   void FrameReceived(const FramePtr pFrame);
};

// the : is a constructor initialisor list. easier way than manually assigning all the pointers.
FrameObserver::FrameObserver(CameraPtr pCamera, Napi::ThreadSafeFunction tsfnJScriptCallback) : IFrameObserver(pCamera), tsfnJScriptCallback(tsfnJScriptCallback) {}

// Frame callback for processing what i want to do with each frame. Will 
void FrameObserver::FrameReceived(const FramePtr pFrame){
    VmbUchar_t* pBuffer;
    VmbUint32_t bufferSize;
    VmbUint32_t width, height;

    pFrame->GetBuffer(pBuffer);
    pFrame->GetBufferSize(bufferSize);
    pFrame->GetWidth(width);
    pFrame->GetHeight(height);

    // Convert that data
    std::vector<uint8_t> yuv420data = convertYUV422toYUV420(pBuffer, width, height);

    struct FrameInfo {
        std::vector<uint8_t> data;
        uint32_t width;
        uint32_t height;
    };
    
    FrameInfo* info = new FrameInfo{std::move(yuv420data), width, height};

    // Take the jscript callback given in the ts file, and call it passing our Napi object in
    auto translateFunction = [](Napi::Env env, Napi::Function jsCallback, FrameInfo* info){
        Napi::Object obj = Napi::Object::New(env);
        obj.Set("buffer", Napi::Buffer<uint8_t>::Copy(env, info->data.data(), info->data.size()));
        obj.Set("width", info->width);
        obj.Set("height", info->height);
        
        jsCallback.Call({obj});
        
        delete info;
    };

    tsfnJScriptCallback.NonBlockingCall(info, translateFunction);
    m_pCamera->QueueFrame(pFrame);
}


std::vector<uint8_t> FrameObserver::convertYUV422toYUV420(VmbUchar_t* yuv422, uint32_t width, uint32_t height) {
    
    
    uint32_t y_size = width * height;
    uint32_t uv_size = (width / 2) * (height / 2);
    uint32_t total_size = y_size + 2 * uv_size;
    
    std::vector<uint8_t> yuv420(total_size);

    uint8_t* y_plane = yuv420.data();
    uint8_t* u_plane = yuv420.data() + y_size;
    uint8_t* v_plane = yuv420.data() + y_size + uv_size;
    
    // Try UYVY format (U Y V Y)
    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col += 2) {
            uint32_t yuv422_idx = (row * width + col) * 2;
            uint32_t y_idx = row * width + col;
            
            // UYVY: U0 Y0 V0 Y1
            y_plane[y_idx] = yuv422[yuv422_idx + 1];      // Y0
            y_plane[y_idx + 1] = yuv422[yuv422_idx + 3];  // Y1
        }
    }
    
    // Fill U and V with 128 (neutral gray)
    std::fill(u_plane, u_plane + uv_size, 128);
    std::fill(v_plane, v_plane + uv_size, 128);
    
    return yuv420;
}

// ------------------------------------------------------------------------------------------------------
// ============================ VimbaX class to initialise in JScript ===================================
class VimbaXSystem : public Napi::ObjectWrap<VimbaXSystem> {
    private:
        VmbSystem& system;
        CameraPtr camera1;
        std::shared_ptr<FrameObserver> myFrameObserver; // smart pointer. is nullptr currently

    public:

        static Napi::Function GetClass(Napi::Env env) {
            return DefineClass(env, "VimbaXSystem", {
                VimbaXSystem::InstanceMethod("startCapture", &VimbaXSystem::StartCapture)
            });
        }

        VimbaXSystem(const Napi::CallbackInfo& info);
        Napi::Value StartCapture(const Napi::CallbackInfo& info);
};

// ================== Initialisation ================
VimbaXSystem::VimbaXSystem(const Napi::CallbackInfo& info) 
: Napi::ObjectWrap<VimbaXSystem>(info), system(VmbSystem::GetInstance()) {
    VmbError_t err = system.Startup();
    std::cerr << "Succesfully initisalised system" << std::endl;
}

// =============================== Javascript calls to begin capture ======================
// This is a setup function, to make the FrameObserver act as a bridge between the two. VimbaX will call FrameRecieved, and FrameRecieved will call the javascript callback.
Napi::Value VimbaXSystem::StartCapture(const Napi::CallbackInfo& info) {
    VmbError_t err;
    Napi::ThreadSafeFunction tsfnJScriptCallback;
    FeaturePtr pFormatFeature;
    FeaturePtr pPayloadSizeFeature;
    VmbInt64_t payloadSize;
    FeaturePtr pAcqStartFeature;

    Napi::Env env = info.Env(); // Info on the runtime enviroment

    Napi::Number cameraIDparameter = info[0].As<Napi::Number>(); // Take second parameter, turn it into int
    uint32_t cameraID = cameraIDparameter.Uint32Value();

    Napi::Function jscriptCallback = info[1].As<Napi::Function>(); // Take the first parameter, turn it into a function


    // Open the requested camera
    switch(cameraID) {
        case 1:
            err = system.OpenCameraByID("169.254.24.139", VmbAccessModeFull, camera1); // OpenCameraById returns an error. If not error, will edit camera object to read only from ID
            if (VmbErrorSuccess != err) {
                std::cerr << "Failed to open camera: " << err << std::endl;
                system.Shutdown();
            } else std::cerr << "Opened FRONT camera" << std::endl;

            // Configure pixel format
            err = camera1->GetFeatureByName("PixelFormat", pFormatFeature);
            if (VmbErrorSuccess == err) {
                err = pFormatFeature->SetValue(VmbPixelFormatYuv422);
                std::cerr << "Set PixelFormat result: " << err << std::endl;
            }
            
            // Set payload size (CRITICAL for frame completion)
            err = camera1->GetFeatureByName("PayloadSize", pPayloadSizeFeature);
            if (VmbErrorSuccess == err) {

                pPayloadSizeFeature->GetValue(payloadSize);
                std::cerr << "PayloadSize: " << payloadSize << std::endl;
            }
            
            // Prepare camera for capture - IMPORTANT!
            err = camera1->AnnounceFrame(FramePtr(new Frame(payloadSize)));
            // or simpler:
            
            // Start acquisition ON THE CAMERA first
            err = camera1->GetFeatureByName("AcquisitionStart", pAcqStartFeature);
            if (VmbErrorSuccess == err) {
                err = pAcqStartFeature->RunCommand();
                std::cerr << "AcquisitionStart command result: " << err << std::endl;
            }

            tsfnJScriptCallback = Napi::ThreadSafeFunction::New( // Turn the callback function into threadsafe so that we can access the javascript function from the C++ thread
                env, // The runtime code, taken from the info
                jscriptCallback, // The javascript function that will be called in javascript file                 
                "FrameCallback",   // Debugging name that will appear in jscript logs       
                0,   // max queue size of functions                       
                1   // How many threads will call this function (just main for me)                      
            );
            
            // Create frame observer with the thread-safe function. Has to be shared because we need to be able to stop the camera later
            myFrameObserver = std::make_shared<FrameObserver>(camera1, tsfnJScriptCallback);
            
            // Start capture. VimbaX gets this function, and calls FrameRecieved whenever a frame comes from it. 
            err = camera1->StartContinuousImageAcquisition(
                5,  // Number of frames to queue
                IFrameObserverPtr(myFrameObserver)
            );
            if (VmbErrorSuccess != err) {
                std::cerr << "Failed to start acquisition: " << err << std::endl;
                // cleanup camera
            }
            break;
        default:
            std::cerr << "Invalid cameraID: " << cameraID << std::endl;
    }


    return env.Undefined();
}


Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = VimbaXSystem::GetClass(env);
    exports.Set("VimbaXSystem", func);
    return exports;
}

NODE_API_MODULE(addon, Init);

