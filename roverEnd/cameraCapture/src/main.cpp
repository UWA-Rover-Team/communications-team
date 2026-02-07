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
    VmbError_t err = pFrame->GetReceiveStatus(status);

    if (status != VmbFrameStatusComplete) {
        // Frame is incomplete
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

    // Convert that data
    std::vector<uint8_t> yuv420data = convertRGB8toYUV420(pBuffer, width, height);

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
VmbError_t VimbaXSystem::InitializeCamera(const std::string& cameraIP, CameraPtr& camera, 
                                          std::shared_ptr<FrameObserver>& observer, 
                                          Napi::ThreadSafeFunction& tsfn) {
    VmbError_t err;
    
    std::cerr << "\n========================================" << std::endl;
    std::cerr << "CAMERA INITIALIZATION: " << cameraIP << std::endl;
    std::cerr << "========================================\n" << std::endl;
    
    // STEP 1: Open camera
    err = system.OpenCameraByID(cameraIP.c_str(), VmbAccessModeFull, camera);
    if (VmbErrorSuccess != err) {
        std::cerr << "✗ Failed to open camera: " << err << std::endl;
        return err;
    }
    std::cerr << "✓ Camera opened successfully\n" << std::endl;

    // STEP 2: Get camera info
    std::cerr << "----- Camera Information -----" << std::endl;
    FeaturePtr pFeature;
    std::string strValue;
    
    if (camera->GetFeatureByName("DeviceModelName", pFeature) == VmbErrorSuccess) {
        pFeature->GetValue(strValue);
        std::cerr << "Model: " << strValue << std::endl;
    }
    if (camera->GetFeatureByName("DeviceSerialNumber", pFeature) == VmbErrorSuccess) {
        pFeature->GetValue(strValue);
        std::cerr << "Serial: " << strValue << std::endl;
    }
    if (camera->GetFeatureByName("DeviceFirmwareVersion", pFeature) == VmbErrorSuccess) {
        pFeature->GetValue(strValue);
        std::cerr << "Firmware: " << strValue << std::endl;
    }
    std::cerr << std::endl;

    // STEP 3: Check and set TriggerMode
    std::cerr << "----- Trigger Configuration -----" << std::endl;
    FeaturePtr pTriggerMode, pTriggerSource, pTriggerSelector;
    
    if (camera->GetFeatureByName("TriggerSelector", pTriggerSelector) == VmbErrorSuccess) {
        pTriggerSelector->GetValue(strValue);
        std::cerr << "TriggerSelector: " << strValue << std::endl;
        pTriggerSelector->SetValue("FrameStart");
        std::cerr << "Set TriggerSelector to FrameStart" << std::endl;
    }
    
    if (camera->GetFeatureByName("TriggerMode", pTriggerMode) == VmbErrorSuccess) {
        pTriggerMode->GetValue(strValue);
        std::cerr << "TriggerMode (before): " << strValue << std::endl;
        
        err = pTriggerMode->SetValue("Off");
        std::cerr << "Set TriggerMode to Off: " << (err == VmbErrorSuccess ? "SUCCESS" : "FAILED") << std::endl;
        
        pTriggerMode->GetValue(strValue);
        std::cerr << "TriggerMode (after): " << strValue << std::endl;
    }
    
    if (camera->GetFeatureByName("TriggerSource", pTriggerSource) == VmbErrorSuccess) {
        pTriggerSource->GetValue(strValue);
        std::cerr << "TriggerSource: " << strValue << std::endl;
    }
    std::cerr << std::endl;

    // STEP 4: Check and set AcquisitionMode
    std::cerr << "----- Acquisition Mode -----" << std::endl;
    FeaturePtr pAcqMode;
    if (camera->GetFeatureByName("AcquisitionMode", pAcqMode) == VmbErrorSuccess) {
        StringVector modes;
        pAcqMode->GetValues(modes);
        std::cerr << "Available modes: ";
        for (const auto& m : modes) std::cerr << m << " ";
        std::cerr << std::endl;
        
        pAcqMode->GetValue(strValue);
        std::cerr << "Current mode: " << strValue << std::endl;
        
        err = pAcqMode->SetValue("Continuous");
        std::cerr << "Set to Continuous: " << (err == VmbErrorSuccess ? "SUCCESS" : "FAILED") << std::endl;
        
        pAcqMode->GetValue(strValue);
        std::cerr << "Mode after set: " << strValue << std::endl;
    }
    std::cerr << std::endl;

    // STEP 5: Set resolution
    std::cerr << "----- Resolution -----" << std::endl;
    FeaturePtr pWidth, pHeight;
    camera->GetFeatureByName("Width", pWidth);
    camera->GetFeatureByName("Height", pHeight);
    
    VmbInt64_t minW, maxW, minH, maxH;
    pWidth->GetRange(minW, maxW);
    pHeight->GetRange(minH, maxH);
    std::cerr << "Width range: " << minW << " - " << maxW << std::endl;
    std::cerr << "Height range: " << minH << " - " << maxH << std::endl;
    
    pWidth->SetValue(240);
    pHeight->SetValue(240);
    
    VmbInt64_t actualW, actualH;
    pWidth->GetValue(actualW);
    pHeight->GetValue(actualH);
    std::cerr << "Set to: " << actualW << "x" << actualH << std::endl;
    std::cerr << std::endl;

    // STEP 6: Pixel format
    std::cerr << "----- Pixel Format -----" << std::endl;
    FeaturePtr pFormat;
    if (camera->GetFeatureByName("PixelFormat", pFormat) == VmbErrorSuccess) {
        // List available formats
        StringVector formats;
        pFormat->GetValues(formats);
        std::cerr << "Available formats:" << std::endl;
        for (const auto& f : formats) {
            std::cerr << "  - " << f << std::endl;
        }
        
        // Try RGB8
        err = pFormat->SetValue("RGB8Packed");
        if (err != VmbErrorSuccess) {
            err = pFormat->SetValue("RGB8");
        }
        if (err != VmbErrorSuccess) {
            err = pFormat->SetValue("Mono8");
        }
        
        pFormat->GetValue(strValue);
        std::cerr << "Using format: " << strValue << std::endl;
    }
    std::cerr << std::endl;

    // STEP 7: Payload size
    std::cerr << "----- Payload -----" << std::endl;
    FeaturePtr pPayload;
    VmbInt64_t payloadSize;
    if (camera->GetFeatureByName("PayloadSize", pPayload) == VmbErrorSuccess) {
        pPayload->GetValue(payloadSize);
        std::cerr << "PayloadSize: " << payloadSize << " bytes" << std::endl;
        
        VmbInt64_t expectedSize = actualW * actualH * 3; // RGB = 3 bytes per pixel
        std::cerr << "Expected for RGB: " << expectedSize << " bytes" << std::endl;
    }
    std::cerr << std::endl;

    // STEP 8: GigE settings
    std::cerr << "----- GigE Network Settings -----" << std::endl;
    FeaturePtr pPacketSize;
    if (camera->GetFeatureByName("GevSCPSPacketSize", pPacketSize) == VmbErrorSuccess) {
        VmbInt64_t packetSize;
        pPacketSize->GetValue(packetSize);
        std::cerr << "Current packet size: " << packetSize << std::endl;
        
        pPacketSize->SetValue(1500);
        pPacketSize->GetValue(packetSize);
        std::cerr << "Set packet size to: " << packetSize << std::endl;
    }
    
    FeaturePtr pDelay;
    if (camera->GetFeatureByName("GevSCPD", pDelay) == VmbErrorSuccess) {
        VmbInt64_t delay;
        pDelay->GetValue(delay);
        std::cerr << "Inter-packet delay: " << delay << " ns" << std::endl;
    }
    std::cerr << std::endl;

    // STEP 9: Frame rate
    std::cerr << "----- Frame Rate -----" << std::endl;
    FeaturePtr pFrameRate;
    if (camera->GetFeatureByName("AcquisitionFrameRate", pFrameRate) == VmbErrorSuccess) {
        double fps;
        pFrameRate->GetValue(fps);
        std::cerr << "Current FPS: " << fps << std::endl;
        
        // Set to low FPS for testing
        pFrameRate->SetValue(5.0);
        pFrameRate->GetValue(fps);
        std::cerr << "Set FPS to: " << fps << std::endl;
    }
    std::cerr << std::endl;

    // STEP 10: Start continuous acquisition
    std::cerr << "----- Starting Acquisition -----" << std::endl;
    observer = std::make_shared<FrameObserver>(camera, tsfn);
    
    err = camera->StartContinuousImageAcquisition(20, IFrameObserverPtr(observer));
    std::cerr << "StartContinuousImageAcquisition: " << (err == VmbErrorSuccess ? "SUCCESS" : "FAILED");
    if (err != VmbErrorSuccess) {
        std::cerr << " (Error code: " << err << ")";
    }
    std::cerr << std::endl;
    
    if (VmbErrorSuccess != err) {
        std::cerr << "✗ Cannot proceed - acquisition start failed" << std::endl;
        return err;
    }
    std::cerr << std::endl;

    // STEP 11: Run AcquisitionStart command
    std::cerr << "----- AcquisitionStart Command -----" << std::endl;
    FeaturePtr pAcqStart;
    err = camera->GetFeatureByName("AcquisitionStart", pAcqStart);
    if (VmbErrorSuccess == err) {
        std::cerr << "Found AcquisitionStart feature" << std::endl;
        
        err = pAcqStart->RunCommand();
        std::cerr << "RunCommand result: " << (err == VmbErrorSuccess ? "SUCCESS" : "FAILED");
        if (err != VmbErrorSuccess) {
            std::cerr << " (Error code: " << err << ")";
        }
        std::cerr << std::endl;
        
        // Check if command is done
        bool isDone = false;
        pAcqStart->IsCommandDone(isDone);
        std::cerr << "Command done: " << (isDone ? "YES" : "NO") << std::endl;
        
    } else {
        std::cerr << "✗ Could not get AcquisitionStart feature (Error: " << err << ")" << std::endl;
    }
    std::cerr << std::endl;

    // STEP 12: Check acquisition status
    std::cerr << "----- Acquisition Status Check -----" << std::endl;
    FeaturePtr pAcqStatus;
    if (camera->GetFeatureByName("AcquisitionStatus", pAcqStatus) == VmbErrorSuccess) {
        pAcqStatus->GetValue(strValue);
        std::cerr << "AcquisitionStatus: " << strValue << std::endl;
    }
    
    // STEP 13: Wait for frames
    std::cerr << "\n----- Waiting 5 seconds for frames -----" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    std::cerr << "\n========================================" << std::endl;
    std::cerr << "INITIALIZATION COMPLETE" << std::endl;
    std::cerr << "Check FrameReceived output above ↑" << std::endl;
    std::cerr << "========================================\n" << std::endl;
    
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

