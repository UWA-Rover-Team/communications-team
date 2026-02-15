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
    std::vector<uint8_t> convertRGB8toYUV420(VmbUchar_t* rgb, uint32_t width, uint32_t height);
public:
   FrameObserver(CameraPtr pCamera, Napi::ThreadSafeFunction threadsafefunction);
   void FrameReceived(const FramePtr pFrame);
};

// the : is a constructor initialisor list. easier way than manually assigning all the pointers.
FrameObserver::FrameObserver(CameraPtr pCamera, Napi::ThreadSafeFunction tsfnJScriptCallback) : IFrameObserver(pCamera), tsfnJScriptCallback(tsfnJScriptCallback) {}

// Frame callback for processing what i want to do with each frame. Will 
void FrameObserver::FrameReceived(const FramePtr pFrame){
    VmbFrameStatusType status;
    pFrame->GetReceiveStatus(status);

    if (status != VmbFrameStatusComplete) {
        VmbUint64_t frameID;
        pFrame->GetFrameID(frameID);
        
        std::string cameraID;
        m_pCamera->GetID(cameraID);
        
        std::cerr << "Camera: " << cameraID 
                  << " Frame: " << frameID 
                  << " Status: " << status 
                  <<  std::endl;
        
        m_pCamera->QueueFrame(pFrame);
        return;
    }
    
    VmbUchar_t* pBuffer;
    VmbUint32_t bufferSize;
    VmbUint32_t width, height;

    pFrame->GetBuffer(pBuffer);
    pFrame->GetBufferSize(bufferSize);
    pFrame->GetWidth(width);
    pFrame->GetHeight(height);

    std::vector<uint8_t> frameCopy(pBuffer, pBuffer + bufferSize);
    m_pCamera->QueueFrame(pFrame);
    
    // NOW do the slow conversion with the copy
    std::vector<uint8_t> yuv420data = convertRGB8toYUV420(frameCopy.data(), width, height);

    struct FrameInfo {
        std::vector<uint8_t> data;
        uint32_t width;
        uint32_t height;
    };
    
    FrameInfo* info = new FrameInfo{std::move(yuv420data), width, height};

    auto translateFunction = [](Napi::Env env, Napi::Function jsCallback, FrameInfo* info){
        Napi::Object obj = Napi::Object::New(env);
        obj.Set("buffer", Napi::Buffer<uint8_t>::Copy(env, info->data.data(), info->data.size()));
        obj.Set("width", info->width);
        obj.Set("height", info->height);
        
        jsCallback.Call({obj});
        
        delete info;
    };

    tsfnJScriptCallback.NonBlockingCall(info, translateFunction);
}


std::vector<uint8_t> FrameObserver::convertRGB8toYUV420(VmbUchar_t* rgb, uint32_t width, uint32_t height) {
    
    uint32_t y_size = width * height;
    uint32_t uv_size = (width / 2) * (height / 2);
    uint32_t total_size = y_size + 2 * uv_size;
    
    std::vector<uint8_t> yuv420(total_size);

    uint8_t* y_plane = yuv420.data();
    uint8_t* u_plane = yuv420.data() + y_size;
    uint8_t* v_plane = yuv420.data() + y_size + uv_size;
    
    // Convert RGB to YUV420
    // RGB8 format is: R G B R G B R G B... (3 bytes per pixel)
    
    // Step 1: Convert all pixels to Y (luminance)
    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            uint32_t rgb_idx = (row * width + col) * 3;
            uint32_t y_idx = row * width + col;
            
            uint8_t r = rgb[rgb_idx + 0];
            uint8_t g = rgb[rgb_idx + 1];
            uint8_t b = rgb[rgb_idx + 2];
            
            // Y = 0.299*R + 0.587*G + 0.114*B
            y_plane[y_idx] = (uint8_t)((77 * r + 150 * g + 29 * b) >> 8);
        }
    }
    
    // Step 2: Downsample and convert to U and V (chrominance)
    // Sample every 2x2 block
    for (uint32_t row = 0; row < height; row += 2) {
        for (uint32_t col = 0; col < width; col += 2) {
            // Sample the top-left pixel of each 2x2 block
            uint32_t rgb_idx = (row * width + col) * 3;
            uint32_t uv_idx = (row / 2) * (width / 2) + (col / 2);
            
            uint8_t r = rgb[rgb_idx + 0];
            uint8_t g = rgb[rgb_idx + 1];
            uint8_t b = rgb[rgb_idx + 2];
            
            // U = -0.169*R - 0.331*G + 0.500*B + 128
            // V =  0.500*R - 0.419*G - 0.081*B + 128
            int u_val = ((-43 * r - 85 * g + 128 * b) >> 8) + 128;
            int v_val = ((128 * r - 107 * g - 21 * b) >> 8) + 128;
            
            // Clamp to 0-255
            u_plane[uv_idx] = (uint8_t)(u_val < 0 ? 0 : (u_val > 255 ? 255 : u_val));
            v_plane[uv_idx] = (uint8_t)(v_val < 0 ? 0 : (v_val > 255 ? 255 : v_val));
        }
    }
    
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

        VmbError_t InitializeCamera(const char* cameraID, CameraPtr& camera, std::shared_ptr<FrameObserver>& observer, Napi::ThreadSafeFunction& tsfn);

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
    VmbErrorType err = system.Startup();
    if (err != VmbErrorSuccess)
    {
        throw std::runtime_error("Could not start API, err=" + std::to_string(err));
    }
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
            std::cerr << "=========== starting FRONT-CAMERA aquisition ===========" << std::endl;
            err = InitializeCamera("DEV_000F315DFF44", cameraFront, frameObserverFront, tsfnJScriptCallback);
            if (err != VmbErrorSuccess) {
                std::cerr << "Failed to initialise camera: " << FRONTCAMERA << std::endl;
                return Napi::Number::New(env, err);
            }
            break;
        case BACKCAMERA:
            std::cerr << "=========== starting BACK-CAMERA aquisition ===========" << std::endl;
            err = InitializeCamera("EMAEFO", cameraBack, frameObserverBack, tsfnJScriptCallback);
            if (err != VmbErrorSuccess) {
                std::cerr << "Failed to initialise camera: " << BACKCAMERA << std::endl;
                return Napi::Number::New(env, err);
            }
            break;
        case LEFTCAMERA:
            std::cerr << "=========== starting LEFT-CAMERA aquisition ===========" << std::endl;
            err = InitializeCamera("MWRMGIR", cameraLeft, frameObserverLeft, tsfnJScriptCallback);
            if (err != VmbErrorSuccess) {
                std::cerr << "Failed to initialise camera: " << LEFTCAMERA << std::endl;
                return Napi::Number::New(env, err);
            }
            break;
        case RIGHTCAMERA:
            std::cerr << "=========== starting RIGHT-CAMERA aquisition ===========" << std::endl;
            err = InitializeCamera("DEV_000F315DFF45", cameraRight, frameObserverRight, tsfnJScriptCallback);
            if (err != VmbErrorSuccess) {
                std::cerr << "Failed to initialise camera: " << RIGHTCAMERA << std::endl;
                return Napi::Number::New(env, err);
            }
            break;
        case MANIPCAMERA:
            std::cerr << "=========== starting MANIP-CAMERA aquisition ===========" << std::endl;
            err = InitializeCamera("MWRGORW", cameraManip, frameObserverManip, tsfnJScriptCallback);
            if (err != VmbErrorSuccess) {
                std::cerr << "Failed to initialise camera: " << MANIPCAMERA << std::endl;
                return Napi::Number::New(env, err);
            }
            break;
        default:
            std::cerr << "Invalid cameraID: " << cameraID << std::endl;
            return Napi::Number::New(env, 0);
    }

    return Napi::Number::New(env, 0);
}


// ================== General camera capture Function ================= ==================
VmbError_t VimbaXSystem::InitializeCamera(const char* cameraID, CameraPtr& camera, std::shared_ptr<FrameObserver>& observer, Napi::ThreadSafeFunction& tsfn) {

    VmbErrorType err;

    err = system.GetCameraByID(cameraID, camera);
    if (err != VmbErrorSuccess)
    {
        std::cerr << ("No camera found with ID=" + std::string(cameraID) + ", err = " + std::to_string(err)) << std::endl;
        return -3;
    }

    err = camera->Open(VmbAccessModeFull);
    if (err != VmbErrorSuccess)
    {
        std::cerr << ("Could not open camera, err=" + std::to_string(err)) << std::endl;
    }

    // Trigger Settings
    FeaturePtr pTriggerSelector;
    if (camera->GetFeatureByName("TriggerSelector", pTriggerSelector) == VmbErrorSuccess) {
        err = pTriggerSelector->SetValue("FrameStart");
        std::cerr << "TriggerSelector set. Error: " << err << std::endl;
    }
    
    FeaturePtr pTriggerMode;
    if (camera->GetFeatureByName("TriggerMode", pTriggerMode) == VmbErrorSuccess) {
        err = pTriggerMode->SetValue("Off");
        std::cerr << "TriggerMode set. Error: " << err << std::endl;
    }

    // Enable packet resend
    FeaturePtr pResendEnabled;
    if (camera->GetFeatureByName("GevSCPSPacketResend", pResendEnabled) == VmbErrorSuccess) {
        err = pResendEnabled->SetValue(true);
        std::cerr << "PacketResend enabled. Error: " << err << std::endl;
    }

    // Increase resend timeout
    FeaturePtr pResendTimeout;
    if (camera->GetFeatureByName("GevSCPSResendRequestTimeout", pResendTimeout) == VmbErrorSuccess) {
        err = pResendTimeout->SetValue(10000); // 10ms timeout
        std::cerr << "ResendTimeout set. Error: " << err << std::endl;
    }

    // Maximum resend requests
    FeaturePtr pMaxResends;
    if (camera->GetFeatureByName("GevSCPSMaxResendRequests", pMaxResends) == VmbErrorSuccess) {
        err = pMaxResends->SetValue(100);
        std::cerr << "MaxResendRequests set. Error: " << err << std::endl;
    }

    // Acquisition Mode
    FeaturePtr pAcqMode;
    if (camera->GetFeatureByName("AcquisitionMode", pAcqMode) == VmbErrorSuccess) {
        err = pAcqMode->SetValue("Continuous");
        std::cerr << "AcquisitionMode set. Error: " << err << std::endl;
    }

    // Binning
    FeaturePtr pBinningH;
    if (camera->GetFeatureByName("BinningHorizontal", pBinningH) == VmbErrorSuccess) {
        err = pBinningH->SetValue(1);
        std::cerr << "BinningHorizontal set. Error: " << err << std::endl;
    }

    // Pixel Format
    FeaturePtr pFormat;
    if (camera->GetFeatureByName("PixelFormat", pFormat) == VmbErrorSuccess) {
        err = pFormat->SetValue("RGB8Packed");
        std::cerr << "PixelFormat set. Error: " << err << std::endl;
    }

    // GigE Network
    FeaturePtr pPacketSize;
    if (camera->GetFeatureByName("GevSCPSPacketSize", pPacketSize) == VmbErrorSuccess) {
        err = pPacketSize->SetValue(1500);
        std::cerr << "GevSCPSPacketSize set. Error: " << err << std::endl;
    }

    // Exposure
    FeaturePtr pExposureAuto;
    if (camera->GetFeatureByName("ExposureAuto", pExposureAuto) == VmbErrorSuccess) {
        err = pExposureAuto->SetValue("Off");
        std::cerr << "ExposureAuto set. Error: " << err << std::endl;
    }

    FeaturePtr pExposure;
    if (camera->GetFeatureByName("ExposureTime", pExposure) == VmbErrorSuccess) {
        err = pExposure->SetValue(9000);
        std::cerr << "ExposureTime set. Error: " << err << std::endl;
    }

    // Gain
    FeaturePtr pGainAuto;
    if (camera->GetFeatureByName("GainAuto", pGainAuto) == VmbErrorSuccess) {
        err = pGainAuto->SetValue("Continuous");
        std::cerr << "GainAuto set. Error: " << err << std::endl;
    }

    // White Balance
    FeaturePtr pBalanceWhiteAuto;
    if (camera->GetFeatureByName("BalanceWhiteAuto", pBalanceWhiteAuto) == VmbErrorSuccess) {
        err = pBalanceWhiteAuto->SetValue("Continuous");
        std::cerr << "BalanceWhiteAuto set. Error: " << err << std::endl;
    }

    FeaturePtr pInterPacketDelay;
    if (camera->GetFeatureByName("GevSCPD", pInterPacketDelay) == VmbErrorSuccess) {
        err = pInterPacketDelay->SetValue(1000);  // 1000ns delay between packets
        std::cerr << "GevSCPD set. Error: " << err << std::endl;
    }

    // Start acquisition
    observer = std::make_shared<FrameObserver>(camera, tsfn);
    err = camera->StartContinuousImageAcquisition(40, IFrameObserverPtr(observer));
    if (VmbErrorSuccess != err) {
        return err;
    }

    // AcquisitionStart
    FeaturePtr pAcqStart;
    err = camera->GetFeatureByName("AcquisitionStart", pAcqStart);
    if (VmbErrorSuccess == err) {
        err = pAcqStart->RunCommand();
        std::cerr << "AcquisitionStart set. Error: " << err << std::endl;
    }
    
    return VmbErrorSuccess;
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

