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
        std::cerr << "Incomplete frame, status: " << status << std::endl;
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


std::vector<uint8_t> FrameObserver::convertRGB8toYUV420(uint8_t* rgb, uint32_t width, uint32_t height) {
    
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
    
    err = system.OpenCameraByID(cameraIP.c_str(), VmbAccessModeFull, camera);
    if (VmbErrorSuccess != err) {
        std::cerr << "Failed to open camera: " << err << std::endl;
        return err;
    }

    std::cerr << "\n========== CAMERA SETTINGS VERIFICATION ==========" << std::endl;

    // === TRIGGER SETTINGS ===
    std::cerr << "\n--- Trigger Configuration ---" << std::endl;
    FeaturePtr pTriggerSelector;
    if (camera->GetFeatureByName("TriggerSelector", pTriggerSelector) == VmbErrorSuccess) {
        std::string before, after;
        pTriggerSelector->GetValue(before);
        std::cerr << "TriggerSelector BEFORE: " << before << std::endl;
        
        pTriggerSelector->SetValue("FrameStart");
        
        pTriggerSelector->GetValue(after);
        std::cerr << "TriggerSelector AFTER:  " << after << std::endl;
        if (before != after) std::cerr << "  ✓ Changed successfully" << std::endl;
    }
    
    FeaturePtr pTriggerMode;
    if (camera->GetFeatureByName("TriggerMode", pTriggerMode) == VmbErrorSuccess) {
        std::string before, after;
        pTriggerMode->GetValue(before);
        std::cerr << "TriggerMode BEFORE: " << before << std::endl;
        
        err = pTriggerMode->SetValue("Off");
        std::cerr << "  SetValue('Off') returned: " << err << " (0=success)" << std::endl;
        
        pTriggerMode->GetValue(after);
        std::cerr << "TriggerMode AFTER:  " << after << std::endl;
        if (after != "Off") {
            std::cerr << "  ✗ ERROR: Failed to set to Off!" << std::endl;
        } else {
            std::cerr << "  ✓ Set to Off successfully" << std::endl;
        }
    }

    // === ACQUISITION MODE ===
    std::cerr << "\n--- Acquisition Mode ---" << std::endl;
    FeaturePtr pAcqMode;
    if (camera->GetFeatureByName("AcquisitionMode", pAcqMode) == VmbErrorSuccess) {
        std::string before, after;
        pAcqMode->GetValue(before);
        std::cerr << "AcquisitionMode BEFORE: " << before << std::endl;
        
        pAcqMode->SetValue("Continuous");
        
        pAcqMode->GetValue(after);
        std::cerr << "AcquisitionMode AFTER:  " << after << std::endl;
    }

    // === BINNING ===
    std::cerr << "\n--- Binning ---" << std::endl;
    FeaturePtr pBinningH;
    if (camera->GetFeatureByName("BinningHorizontal", pBinningH) == VmbErrorSuccess) {
        VmbInt64_t min, max, inc, before, after;
        pBinningH->GetRange(min, max);
        pBinningH->GetIncrement(inc);
        std::cerr << "BinningHorizontal range: " << min << "-" << max << " (increment: " << inc << ")" << std::endl;
        
        pBinningH->GetValue(before);
        std::cerr << "BinningHorizontal BEFORE: " << before << std::endl;
        
        err = pBinningH->SetValue(4);
        std::cerr << "  SetValue(4) returned: " << err << std::endl;
        
        pBinningH->GetValue(after);
        std::cerr << "BinningHorizontal AFTER:  " << after << std::endl;
    }

    // === RESOLUTION ===
    std::cerr << "\n--- Resolution ---" << std::endl;
    FeaturePtr pWidth, pHeight;
    camera->GetFeatureByName("Width", pWidth);
    camera->GetFeatureByName("Height", pHeight);
    
    VmbInt64_t width, height;
    pWidth->GetValue(width);
    pHeight->GetValue(height);
    std::cerr << "Resolution: " << width << " x " << height << std::endl;

    // === PIXEL FORMAT ===
    std::cerr << "\n--- Pixel Format ---" << std::endl;
    FeaturePtr pFormat;
    if (camera->GetFeatureByName("PixelFormat", pFormat) == VmbErrorSuccess) {
        std::string before, after;
        pFormat->GetValue(before);
        std::cerr << "PixelFormat BEFORE: " << before << std::endl;
        
        err = pFormat->SetValue("RGB8Packed");
        std::cerr << "  SetValue('RGB8Packed') returned: " << err << std::endl;
        
        pFormat->GetValue(after);
        std::cerr << "PixelFormat AFTER:  " << after << std::endl;
    }

    // === GIGE NETWORK ===
    std::cerr << "\n--- GigE Network ---" << std::endl;
    FeaturePtr pPacketSize;
    if (camera->GetFeatureByName("GevSCPSPacketSize", pPacketSize) == VmbErrorSuccess) {
        VmbInt64_t before, after;
        pPacketSize->GetValue(before);
        std::cerr << "PacketSize BEFORE: " << before << std::endl;
        
        pPacketSize->SetValue(1500);
        
        pPacketSize->GetValue(after);
        std::cerr << "PacketSize AFTER:  " << after << std::endl;
    }
    
    FeaturePtr pDelay;
    if (camera->GetFeatureByName("GevSCPD", pDelay) == VmbErrorSuccess) {
        VmbInt64_t delay;
        pDelay->GetValue(delay);
        std::cerr << "Inter-packet delay: " << delay << " ns" << std::endl;
    }

    // === EXPOSURE ===
    std::cerr << "\n--- Exposure ---" << std::endl;
    FeaturePtr pExposureAuto;
    if (camera->GetFeatureByName("ExposureAuto", pExposureAuto) == VmbErrorSuccess) {
        std::string before, after;
        pExposureAuto->GetValue(before);
        std::cerr << "ExposureAuto BEFORE: " << before << std::endl;
        
        pExposureAuto->SetValue("Off");
        
        pExposureAuto->GetValue(after);
        std::cerr << "ExposureAuto AFTER:  " << after << std::endl;
    }

    FeaturePtr pExposure;
    if (camera->GetFeatureByName("ExposureTime", pExposure) == VmbErrorSuccess) {
        double min, max, before, after;
        pExposure->GetRange(min, max);
        std::cerr << "ExposureTime range: " << min << "-" << max << " µs" << std::endl;
        
        pExposure->GetValue(before);
        std::cerr << "ExposureTime BEFORE: " << before << " µs" << std::endl;
        
        pExposure->SetValue(9000);
        
        pExposure->GetValue(after);
        std::cerr << "ExposureTime AFTER:  " << after << " µs" << std::endl;
        std::cerr << "  (= " << (after/1000.0) << " ms)" << std::endl;
    }

    // === GAIN ===
    std::cerr << "\n--- Gain ---" << std::endl;
    FeaturePtr pGainAuto;
    if (camera->GetFeatureByName("GainAuto", pGainAuto) == VmbErrorSuccess) {
        std::string before, after;
        pGainAuto->GetValue(before);
        std::cerr << "GainAuto BEFORE: " << before << std::endl;
        
        pGainAuto->SetValue("Continuous");
        
        pGainAuto->GetValue(after);
        std::cerr << "GainAuto AFTER:  " << after << std::endl;
    }
    
    FeaturePtr pGain;
    if (camera->GetFeatureByName("Gain", pGain) == VmbErrorSuccess) {
        double gain;
        pGain->GetValue(gain);
        std::cerr << "Current Gain: " << gain << " dB" << std::endl;
    }

    // === WHITE BALANCE ===
    std::cerr << "\n--- White Balance ---" << std::endl;
    FeaturePtr pBalanceWhiteAuto;
    if (camera->GetFeatureByName("BalanceWhiteAuto", pBalanceWhiteAuto) == VmbErrorSuccess) {
        std::string mode;
        pBalanceWhiteAuto->GetValue(mode);
        std::cerr << "BalanceWhiteAuto: " << mode << std::endl;
        
        // Try enabling it
        pBalanceWhiteAuto->SetValue("Continuous");
        pBalanceWhiteAuto->GetValue(mode);
        std::cerr << "BalanceWhiteAuto AFTER: " << mode << std::endl;
    }

    // === GAMMA ===
    std::cerr << "\n--- Gamma ---" << std::endl;
    FeaturePtr pGamma;
    if (camera->GetFeatureByName("Gamma", pGamma) == VmbErrorSuccess) {
        double gamma;
        pGamma->GetValue(gamma);
        std::cerr << "Gamma: " << gamma << std::endl;
    }

    // === BLACK LEVEL ===
    std::cerr << "\n--- Black Level ---" << std::endl;
    FeaturePtr pBlackLevel;
    if (camera->GetFeatureByName("BlackLevel", pBlackLevel) == VmbErrorSuccess) {
        double blackLevel;
        pBlackLevel->GetValue(blackLevel);
        std::cerr << "BlackLevel: " << blackLevel << std::endl;
    }

    std::cerr << "\n================================================\n" << std::endl;

    // Start acquisition
    observer = std::make_shared<FrameObserver>(camera, tsfn);
    err = camera->StartContinuousImageAcquisition(40, IFrameObserverPtr(observer));
    if (VmbErrorSuccess != err) {
        std::cerr << "StartContinuousImageAcquisition FAILED: " << err << std::endl;
        return err;
    }
    std::cerr << "Started acquisition with 40 buffers" << std::endl;

    // AcquisitionStart
    FeaturePtr pAcqStart;
    err = camera->GetFeatureByName("AcquisitionStart", pAcqStart);
    if (VmbErrorSuccess == err) {
        err = pAcqStart->RunCommand();
        std::cerr << "AcquisitionStart command: " << (err == VmbErrorSuccess ? "SUCCESS" : "FAILED") << std::endl;
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
            err = InitializeCamera("169.254.234.45", cameraBack, frameObserverBack, tsfnJScriptCallback);
            break;
        case LEFTCAMERA:
            err = InitializeCamera("169.254.145.157", cameraLeft, frameObserverLeft, tsfnJScriptCallback);
            break;
        case RIGHTCAMERA:
            err = InitializeCamera("169.254.56.227", cameraRight, frameObserverRight, tsfnJScriptCallback);
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

