#include <VmbCPP/VmbCPP.h>
#include <iostream>
#include <napi.h>
#include <cstdint>
#include <vector>
#include <filesystem>

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
    std::vector<uint8_t> FrameObserver::binRGB8Vertical(VmbUchar_t* rgb, uint32_t width, uint32_t height, uint32_t binningFactor);
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
    uint8_t binninFactor = 4;
    uint32_t binnedHeight = height/binninFactor;
    std::vector<uint8_t> binnedRGBdata = binRGB8Vertical(frameCopy.data(), width, height, binninFactor);
    std::vector<uint8_t> yuv420data = convertRGB8toYUV420(binnedRGBdata.data(), width, binnedHeight);

    struct FrameInfo {
        std::vector<uint8_t> data;
        uint32_t width;
        uint32_t binnedHeight;
    };
    
    FrameInfo* info = new FrameInfo{std::move(yuv420data), width, height};

    auto translateFunction = [](Napi::Env env, Napi::Function jsCallback, FrameInfo* info){
        Napi::Object obj = Napi::Object::New(env);
        obj.Set("buffer", Napi::Buffer<uint8_t>::Copy(env, info->data.data(), info->data.size()));
        obj.Set("width", info->width);
        obj.Set("height", info->binnedHeight);
        
        jsCallback.Call({obj});
        
        delete info;
    };

    tsfnJScriptCallback.NonBlockingCall(info, translateFunction);
}


std::vector<uint8_t> FrameObserver::binRGB8Vertical(VmbUchar_t* rgb, uint32_t width, uint32_t height, uint32_t binningFactor) {

    uint32_t binned_height = height / binningFactor;
    std::vector<uint8_t> binned_rgb(width * binned_height * 3);

    for (uint32_t row = 0; row < binned_height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            uint32_t sum_r = 0, sum_g = 0, sum_b = 0;

            for (uint32_t k = 0; k < binningFactor; k++) {
                uint32_t src_row = row * binningFactor + k;
                uint32_t rgb_idx = (src_row * width + col) * 3;
                sum_r += rgb[rgb_idx + 0];
                sum_g += rgb[rgb_idx + 1];
                sum_b += rgb[rgb_idx + 2];
            }

            uint32_t dst_idx = (row * width + col) * 3;
            binned_rgb[dst_idx + 0] = (uint8_t)(sum_r / binningFactor);
            binned_rgb[dst_idx + 1] = (uint8_t)(sum_g / binningFactor);
            binned_rgb[dst_idx + 2] = (uint8_t)(sum_b / binningFactor);
        }
    }

    return binned_rgb;
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
            if (cameraFront) {
                VmbError_t closeErr = cameraFront->Close();
                if (closeErr != VmbErrorSuccess) {
                    std::cerr << "Warning: Failed to close existing camera connection: " << closeErr << std::endl;
                }
                cameraFront.reset(); // Reset the shared_ptr if using smart pointers
            }
            std::cerr << "=========== starting FRONT-CAMERA aquisition ===========" << std::endl;
            err = InitializeCamera("DEV_000F315DFF44", cameraFront, frameObserverFront, tsfnJScriptCallback);
            if (err != VmbErrorSuccess) {
                std::cerr << "Failed to initialise camera: " << FRONTCAMERA << std::endl;
                return Napi::Number::New(env, err);
            }
            break;
        case BACKCAMERA:
            if (cameraBack) {
                VmbError_t closeErr = cameraBack->Close();
                if (closeErr != VmbErrorSuccess) {
                    std::cerr << "Warning: Failed to close existing camera connection: " << closeErr << std::endl;
                }
                cameraBack.reset(); // Reset the shared_ptr if using smart pointers
            }
            std::cerr << "=========== starting BACK-CAMERA aquisition ===========" << std::endl;
            err = InitializeCamera("EMAEFO", cameraBack, frameObserverBack, tsfnJScriptCallback);
            if (err != VmbErrorSuccess) {
                std::cerr << "Failed to initialise camera: " << BACKCAMERA << std::endl;
                return Napi::Number::New(env, err);
            }
            break;
        case LEFTCAMERA:
            if (cameraLeft) {
                VmbError_t closeErr = cameraLeft->Close();
                if (closeErr != VmbErrorSuccess) {
                    std::cerr << "Warning: Failed to close existing camera connection: " << closeErr << std::endl;
                }
                cameraLeft.reset(); // Reset the shared_ptr if using smart pointers
            }
            std::cerr << "=========== starting LEFT-CAMERA aquisition ===========" << std::endl;
            err = InitializeCamera("DEV_000F315DFF47", cameraLeft, frameObserverLeft, tsfnJScriptCallback);
            if (err != VmbErrorSuccess) {
                std::cerr << "Failed to initialise camera: " << LEFTCAMERA << std::endl;
                return Napi::Number::New(env, err);
            }
            break;
        case RIGHTCAMERA:
            if (cameraRight) {
                VmbError_t closeErr = cameraRight->Close();
                if (closeErr != VmbErrorSuccess) {
                    std::cerr << "Warning: Failed to close existing camera connection: " << closeErr << std::endl;
                }
                cameraRight.reset(); // Reset the shared_ptr if using smart pointers
            }
            std::cerr << "=========== starting RIGHT-CAMERA aquisition ===========" << std::endl;
            err = InitializeCamera("DEV_000F315DFF45", cameraRight, frameObserverRight, tsfnJScriptCallback);
            if (err != VmbErrorSuccess) {
                std::cerr << "Failed to initialise camera: " << RIGHTCAMERA << std::endl;
                return Napi::Number::New(env, err);
            }
            break;
        case MANIPCAMERA:
            if (cameraManip) {
                VmbError_t closeErr = cameraManip->Close();
                if (closeErr != VmbErrorSuccess) {
                    std::cerr << "Warning: Failed to close existing camera connection: " << closeErr << std::endl;
                }
                cameraManip.reset(); // Reset the shared_ptr if using smart pointers
            }
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
        return err;
    }

    err = camera->Open(VmbAccessModeFull);
    if (err != VmbErrorSuccess)
    {
        std::cerr << ("Could not open camera, err=" + std::to_string(err)) << std::endl;
        return err;
    }

    // Acquisition Mode
    FeaturePtr pAcqMode;
    err = camera->GetFeatureByName("AcquisitionMode", pAcqMode);
    if (err == VmbErrorSuccess) {
        err = pAcqMode->SetValue("Continuous");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: AcquisitionMode failed to set. Error: " << err << std::endl;
        }
    }

    // Acquisition Frame Count
    FeaturePtr pAcqFrameCount;
    err = camera->GetFeatureByName("AcquisitionFrameCount", pAcqFrameCount);
    if (err == VmbErrorSuccess) {
        err = pAcqFrameCount->SetValue(1);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: AcquisitionFrameCount failed to set. Error: " << err << std::endl;
        }
    }

    // Trigger Settings - FrameStart
    FeaturePtr pTriggerSelector;
    err = camera->GetFeatureByName("TriggerSelector", pTriggerSelector);
    if (err == VmbErrorSuccess) {
        err = pTriggerSelector->SetValue("FrameStart");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: TriggerSelector failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pTriggerMode;
    err = camera->GetFeatureByName("TriggerMode", pTriggerMode);
    if (err == VmbErrorSuccess) {
        err = pTriggerMode->SetValue("On");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: TriggerMode (FrameStart) failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pTriggerSource;
    err = camera->GetFeatureByName("TriggerSource", pTriggerSource);
    if (err == VmbErrorSuccess) {
        err = pTriggerSource->SetValue("Freerun");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: TriggerSource (FrameStart) failed to set. Error: " << err << std::endl;
        }
    }

    // Pixel Format
    FeaturePtr pPixelFormat;
    err = camera->GetFeatureByName("PixelFormat", pPixelFormat);
    if (err == VmbErrorSuccess) {
        err = pPixelFormat->SetValue("RGB8Packed");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: PixelFormat failed to set. Error: " << err << std::endl;
        }
    }

    // Width and Height
    FeaturePtr pWidth;
    err = camera->GetFeatureByName("Width", pWidth);
    if (err == VmbErrorSuccess) {
        err = pWidth->SetValue(2064);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: Width failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pHeight;
    err = camera->GetFeatureByName("Height", pHeight);
    if (err == VmbErrorSuccess) {
        err = pHeight->SetValue(1544);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: Height failed to set. Error: " << err << std::endl;
        }
    }

    // Offset
    FeaturePtr pOffsetX;
    err = camera->GetFeatureByName("OffsetX", pOffsetX);
    if (err == VmbErrorSuccess) {
        err = pOffsetX->SetValue(0);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: OffsetX failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pOffsetY;
    err = camera->GetFeatureByName("OffsetY", pOffsetY);
    if (err == VmbErrorSuccess) {
        err = pOffsetY->SetValue(0);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: OffsetY failed to set. Error: " << err << std::endl;
        }
    }

    // Binning
    FeaturePtr pBinningH;
    err = camera->GetFeatureByName("BinningHorizontal", pBinningH);
    if (err == VmbErrorSuccess) {
        err = pBinningH->SetValue(4);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: BinningHorizontal failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pBinningV;
    err = camera->GetFeatureByName("BinningVertical", pBinningV);
    if (err == VmbErrorSuccess) {
        err = pBinningV->SetValue(1);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: BinningVertical failed to set. Error: " << err << std::endl;
        }
    }

    // Decimation
    FeaturePtr pDecimationH;
    err = camera->GetFeatureByName("DecimationHorizontal", pDecimationH);
    if (err == VmbErrorSuccess) {
        err = pDecimationH->SetValue(1);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: DecimationHorizontal failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pDecimationV;
    err = camera->GetFeatureByName("DecimationVertical", pDecimationV);
    if (err == VmbErrorSuccess) {
        err = pDecimationV->SetValue(1);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: DecimationVertical failed to set. Error: " << err << std::endl;
        }
    }

    // Reverse
    FeaturePtr pReverseX;
    err = camera->GetFeatureByName("ReverseX", pReverseX);
    if (err == VmbErrorSuccess) {
        err = pReverseX->SetValue(false);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: ReverseX failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pReverseY;
    err = camera->GetFeatureByName("ReverseY", pReverseY);
    if (err == VmbErrorSuccess) {
        err = pReverseY->SetValue(false);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: ReverseY failed to set. Error: " << err << std::endl;
        }
    }

    // Exposure
    FeaturePtr pExposureAuto;
    err = camera->GetFeatureByName("ExposureAuto", pExposureAuto);
    if (err == VmbErrorSuccess) {
        err = pExposureAuto->SetValue("Off");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: ExposureAuto failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pExposureMode;
    err = camera->GetFeatureByName("ExposureMode", pExposureMode);
    if (err == VmbErrorSuccess) {
        err = pExposureMode->SetValue("Timed");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: ExposureMode failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pExposureTime;
    err = camera->GetFeatureByName("ExposureTimeAbs", pExposureTime);
    if (err == VmbErrorSuccess) {
        err = pExposureTime->SetValue(5000.0);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: ExposureTimeAbs failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pExposureAutoMin;
    err = camera->GetFeatureByName("ExposureAutoMin", pExposureAutoMin);
    if (err == VmbErrorSuccess) {
        err = pExposureAutoMin->SetValue(15);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: ExposureAutoMin failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pExposureAutoMax;
    err = camera->GetFeatureByName("ExposureAutoMax", pExposureAutoMax);
    if (err == VmbErrorSuccess) {
        err = pExposureAutoMax->SetValue(499999);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: ExposureAutoMax failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pExposureAutoTarget;
    err = camera->GetFeatureByName("ExposureAutoTarget", pExposureAutoTarget);
    if (err == VmbErrorSuccess) {
        err = pExposureAutoTarget->SetValue(50);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: ExposureAutoTarget failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pExposureAutoRate;
    err = camera->GetFeatureByName("ExposureAutoRate", pExposureAutoRate);
    if (err == VmbErrorSuccess) {
        err = pExposureAutoRate->SetValue(100);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: ExposureAutoRate failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pExposureAutoAlg;
    err = camera->GetFeatureByName("ExposureAutoAlg", pExposureAutoAlg);
    if (err == VmbErrorSuccess) {
        err = pExposureAutoAlg->SetValue("Mean");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: ExposureAutoAlg failed to set. Error: " << err << std::endl;
        }
    }

    // Gain
    FeaturePtr pGainSelector;
    err = camera->GetFeatureByName("GainSelector", pGainSelector);
    if (err == VmbErrorSuccess) {
        err = pGainSelector->SetValue("All");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: GainSelector failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pGainAuto;
    err = camera->GetFeatureByName("GainAuto", pGainAuto);
    if (err == VmbErrorSuccess) {
        err = pGainAuto->SetValue("Continuous");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: GainAuto failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pGainAutoMin;
    err = camera->GetFeatureByName("GainAutoMin", pGainAutoMin);
    if (err == VmbErrorSuccess) {
        err = pGainAutoMin->SetValue(0.0);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: GainAutoMin failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pGainAutoMax;
    err = camera->GetFeatureByName("GainAutoMax", pGainAutoMax);
    if (err == VmbErrorSuccess) {
        err = pGainAutoMax->SetValue(40.0);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: GainAutoMax failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pGainAutoTarget;
    err = camera->GetFeatureByName("GainAutoTarget", pGainAutoTarget);
    if (err == VmbErrorSuccess) {
        err = pGainAutoTarget->SetValue(50);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: GainAutoTarget failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pGainAutoRate;
    err = camera->GetFeatureByName("GainAutoRate", pGainAutoRate);
    if (err == VmbErrorSuccess) {
        err = pGainAutoRate->SetValue(100);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: GainAutoRate failed to set. Error: " << err << std::endl;
        }
    }

    // White Balance
    FeaturePtr pBalanceWhiteAuto;
    err = camera->GetFeatureByName("BalanceWhiteAuto", pBalanceWhiteAuto);
    if (err == VmbErrorSuccess) {
        err = pBalanceWhiteAuto->SetValue("Continuous");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: BalanceWhiteAuto failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pBalanceWhiteAutoAdjustTol;
    err = camera->GetFeatureByName("BalanceWhiteAutoAdjustTol", pBalanceWhiteAutoAdjustTol);
    if (err == VmbErrorSuccess) {
        err = pBalanceWhiteAutoAdjustTol->SetValue(5);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: BalanceWhiteAutoAdjustTol failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pBalanceWhiteAutoRate;
    err = camera->GetFeatureByName("BalanceWhiteAutoRate", pBalanceWhiteAutoRate);
    if (err == VmbErrorSuccess) {
        err = pBalanceWhiteAutoRate->SetValue(100);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: BalanceWhiteAutoRate failed to set. Error: " << err << std::endl;
        }
    }

    // Black Level
    FeaturePtr pBlackLevelSelector;
    err = camera->GetFeatureByName("BlackLevelSelector", pBlackLevelSelector);
    if (err == VmbErrorSuccess) {
        err = pBlackLevelSelector->SetValue("All");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: BlackLevelSelector failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pBlackLevel;
    err = camera->GetFeatureByName("BlackLevel", pBlackLevel);
    if (err == VmbErrorSuccess) {
        err = pBlackLevel->SetValue(4.0);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: BlackLevel failed to set. Error: " << err << std::endl;
        }
    }

    // Gamma, Hue, Saturation
    FeaturePtr pGamma;
    err = camera->GetFeatureByName("Gamma", pGamma);
    if (err == VmbErrorSuccess) {
        err = pGamma->SetValue(1.0);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: Gamma failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pHue;
    err = camera->GetFeatureByName("Hue", pHue);
    if (err == VmbErrorSuccess) {
        err = pHue->SetValue(0.0);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: Hue failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pSaturation;
    err = camera->GetFeatureByName("Saturation", pSaturation);
    if (err == VmbErrorSuccess) {
        err = pSaturation->SetValue(1.0);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: Saturation failed to set. Error: " << err << std::endl;
        }
    }

    // Bandwidth / Throughput
    FeaturePtr pBandwidthControlMode;
    err = camera->GetFeatureByName("BandwidthControlMode", pBandwidthControlMode);
    if (err == VmbErrorSuccess) {
        err = pBandwidthControlMode->SetValue("StreamBytesPerSecond");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: BandwidthControlMode failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pStreamBytesPerSecond;
    err = camera->GetFeatureByName("StreamBytesPerSecond", pStreamBytesPerSecond);
    if (err == VmbErrorSuccess) {
        err = pStreamBytesPerSecond->SetValue(23000000);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: StreamBytesPerSecond failed to set. Error: " << err << std::endl;
        }
    }

    // GigE Packet Size
    FeaturePtr pPacketSize;
    err = camera->GetFeatureByName("GevSCPSPacketSize", pPacketSize);
    if (err == VmbErrorSuccess) {
        err = pPacketSize->SetValue(8228);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: GevSCPSPacketSize failed to set. Error: " << err << std::endl;
        }
    }

    // Stream Frame Rate Constrain
    FeaturePtr pStreamFrameRateConstrain;
    err = camera->GetFeatureByName("StreamFrameRateConstrain", pStreamFrameRateConstrain);
    if (err == VmbErrorSuccess) {
        err = pStreamFrameRateConstrain->SetValue(true);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: StreamFrameRateConstrain failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pFrameRate;
    err = camera->GetFeatureByName("AcquisitionFrameRateAbs", pFrameRate);
    if (err == VmbErrorSuccess) {
        err = pFrameRate->SetValue(16.0);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: AcquisitionFrameRateAbs failed to set. Error: " << err << std::endl;
        }
    }

    // DSP Subregion
    FeaturePtr pDSPBottom;
    err = camera->GetFeatureByName("DSPSubregionBottom", pDSPBottom);
    if (err == VmbErrorSuccess) {
        err = pDSPBottom->SetValue(1544);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: DSPSubregionBottom failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pDSPLeft;
    err = camera->GetFeatureByName("DSPSubregionLeft", pDSPLeft);
    if (err == VmbErrorSuccess) {
        err = pDSPLeft->SetValue(0);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: DSPSubregionLeft failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pDSPRight;
    err = camera->GetFeatureByName("DSPSubregionRight", pDSPRight);
    if (err == VmbErrorSuccess) {
        err = pDSPRight->SetValue(2064);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: DSPSubregionRight failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pDSPTop;
    err = camera->GetFeatureByName("DSPSubregionTop", pDSPTop);
    if (err == VmbErrorSuccess) {
        err = pDSPTop->SetValue(0);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: DSPSubregionTop failed to set. Error: " << err << std::endl;
        }
    }

    // Color Transformation
    FeaturePtr pColorTransformationMode;
    err = camera->GetFeatureByName("ColorTransformationMode", pColorTransformationMode);
    if (err == VmbErrorSuccess) {
        err = pColorTransformationMode->SetValue("Off");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: ColorTransformationMode failed to set. Error: " << err << std::endl;
        }
    }

    // PTP
    FeaturePtr pPtpMode;
    err = camera->GetFeatureByName("PtpMode", pPtpMode);
    if (err == VmbErrorSuccess) {
        err = pPtpMode->SetValue("Off");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: PtpMode failed to set. Error: " << err << std::endl;
        }
    }

    // Chunk Mode
    FeaturePtr pChunkModeActive;
    err = camera->GetFeatureByName("ChunkModeActive", pChunkModeActive);
    if (err == VmbErrorSuccess) {
        err = pChunkModeActive->SetValue(false);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: ChunkModeActive failed to set. Error: " << err << std::endl;
        }
    }

    // Stream Hold
    FeaturePtr pStreamHoldEnable;
    err = camera->GetFeatureByName("StreamHoldEnable", pStreamHoldEnable);
    if (err == VmbErrorSuccess) {
        err = pStreamHoldEnable->SetValue("Off");
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: StreamHoldEnable failed to set. Error: " << err << std::endl;
        }
    }

    FeaturePtr pStreamHoldCapacity;
    err = camera->GetFeatureByName("StreamHoldCapacity", pStreamHoldCapacity);
    if (err == VmbErrorSuccess) {
        err = pStreamHoldCapacity->SetValue(20);
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: StreamHoldCapacity failed to set. Error: " << err << std::endl;
        }
    }

    // Start acquisition
    observer = std::make_shared<FrameObserver>(camera, tsfn);
    err = camera->StartContinuousImageAcquisition(40, IFrameObserverPtr(observer));
    if (VmbErrorSuccess != err) {
        std::cerr << "ERROR: Failed to start image aquisition. Code: " << err << std::endl;
        return err;
    }
    
    // AcquisitionStart
    FeaturePtr pAcqStart;
    err = camera->GetFeatureByName("AcquisitionStart", pAcqStart);
    if (VmbErrorSuccess == err) {
        err = pAcqStart->RunCommand();
        if (err != VmbErrorSuccess) {
            std::cerr << "ERROR: AcquisitionStart failed to run. Error: " << err << std::endl;
        }
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

