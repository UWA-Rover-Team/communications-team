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

// -----------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------
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
// ------------------------------------------------------------------------------------------------------
// ============================ VimbaX class to initialise in JScript ===================================
class VimbaXSystem : public Napi::ObjectWrap<VimbaXSystem> {
    private:
        VmbSystem& system;
        CameraPtr cameraFront;
        CameraPtr cameraBack;
        CameraPtr cameraLeft;
        CameraPtr cameraRight;
        CameraPtr cameraManip;
        
        std::shared_ptr<FrameObserver> frameObserverFront;
        std::shared_ptr<FrameObserver> frameObserverBack;
        std::shared_ptr<FrameObserver> frameObserverLeft;
        std::shared_ptr<FrameObserver> frameObserverRight;
        std::shared_ptr<FrameObserver> frameObserverManip;

        VmbError_t InitializeCamera(const std::string& cameraIP, CameraPtr& camera, std::shared_ptr<FrameObserver>& observer, Napi::ThreadSafeFunction& tsfn);

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
    std::cerr << "Successfully initialised system" << std::endl;
}

// ================== General camera capture Function ================
VmbError_t VimbaXSystem::InitializeCamera(const std::string& cameraIP, CameraPtr& camera, std::shared_ptr<FrameObserver>& observer, Napi::ThreadSafeFunction& tsfn) {
    VmbError_t err;
    
    // Open camera
    err = system.OpenCameraByID(cameraIP.c_str(), VmbAccessModeFull, camera);
    if (VmbErrorSuccess != err) {
        return err;
    }

    // Set TriggerSelector to FrameStart, then disable trigger
    FeaturePtr pTriggerSelector;
    if (camera->GetFeatureByName("TriggerSelector", pTriggerSelector) == VmbErrorSuccess) {
        pTriggerSelector->SetValue("FrameStart");
    }
    
    FeaturePtr pTriggerMode;
    if (camera->GetFeatureByName("TriggerMode", pTriggerMode) == VmbErrorSuccess) {
        pTriggerMode->SetValue("Off");
    }

    // Set AcquisitionMode to Continuous
    FeaturePtr pAcqMode;
    if (camera->GetFeatureByName("AcquisitionMode", pAcqMode) == VmbErrorSuccess) {
        pAcqMode->SetValue("Continuous");
    }

    // Set resolution
    FeaturePtr pWidth, pHeight;
    camera->GetFeatureByName("Width", pWidth);
    camera->GetFeatureByName("Height", pHeight);
    pWidth->SetValue(240);
    pHeight->SetValue(240);

    // Set pixel format
    FeaturePtr pFormat;
    if (camera->GetFeatureByName("PixelFormat", pFormat) == VmbErrorSuccess) {
        err = pFormat->SetValue("YUV422Packed");
        if (err != VmbErrorSuccess) {
            pFormat->SetValue("RGB8");
        }
        if (err != VmbErrorSuccess) {
            pFormat->SetValue("Mono8");
        }
    }

    // GigE packet settings
    FeaturePtr pPacketSize;
    if (camera->GetFeatureByName("GevSCPSPacketSize", pPacketSize) == VmbErrorSuccess) {
        pPacketSize->SetValue(1500);
    }

    // Start acquisition
    observer = std::make_shared<FrameObserver>(camera, tsfn);
    err = camera->StartContinuousImageAcquisition(20, IFrameObserverPtr(observer));
    if (VmbErrorSuccess != err) {
        return err;
    }

    // Run AcquisitionStart command
    FeaturePtr pAcqStart;
    err = camera->GetFeatureByName("AcquisitionStart", pAcqStart);
    if (VmbErrorSuccess == err) {
        pAcqStart->RunCommand();
    }
    
    return VmbErrorSuccess;
}


// =============================== Specific camera capture function for jscript ======================
Napi::Value VimbaXSystem::StartCapture(const Napi::CallbackInfo& info) {
    VmbError_t err;
    Napi::Env env = info.Env();

    Napi::Number cameraIDparameter = info[0].As<Napi::Number>();
    uint32_t cameraID = cameraIDparameter.Uint32Value();

    Napi::Function jscriptCallback = info[1].As<Napi::Function>();
    
    Napi::ThreadSafeFunction tsfnJScriptCallback = Napi::ThreadSafeFunction::New(
        env,
        jscriptCallback,
        "FrameCallback",
        0,
        1
    );

    switch(cameraID) {
        case FRONTCAMERA:
            err = InitializeCamera("169.254.24.139", cameraFront, frameObserverFront, tsfnJScriptCallback);
            break;
        case BACKCAMERA:
            err = InitializeCamera("169.254.24.XXX", cameraBack, frameObserverBack, tsfnJScriptCallback);
            break;
        case LEFTCAMERA:
            err = InitializeCamera("169.254.24.XXX", cameraLeft, frameObserverLeft, tsfnJScriptCallback);
            break;
        case RIGHTCAMERA:
            err = InitializeCamera("169.254.24.XXX", cameraRight, frameObserverRight, tsfnJScriptCallback);
            break;
        case MANIPCAMERA:
            err = InitializeCamera("169.254.24.XXX", cameraManip, frameObserverManip, tsfnJScriptCallback);
            break;
        default:
            std::cerr << "Invalid cameraID: " << cameraID << std::endl;
            return env.Undefined();
    }

    return env.Undefined();
}

// ----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
// ====================== N-API linux init (for some reason) =========================================
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = VimbaXSystem::GetClass(env);
    exports.Set("VimbaXSystem", func);
    return exports;
}

NODE_API_MODULE(addon, Init);

