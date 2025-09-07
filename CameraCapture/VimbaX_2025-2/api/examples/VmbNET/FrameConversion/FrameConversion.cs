/*=============================================================================
  Copyright (C) 2024-2025 Allied Vision Technologies. All Rights Reserved.
  Subject to the BSD 3-Clause License.
=============================================================================*/

using Logging;
using OpenCvSharp;
using VmbNET;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.IO;
using System.Threading.Tasks;

namespace FrameConversion
{
    /// <summary>
    /// Example program using the frame converter of VmbNET.
    /// </summary>
    class FrameConversion
    {
        // code returned by OpenCV's WaitKey() when <enter> is pressed
        const int EnterKeyCode = 13;
        // code returned by OpenCV's WaitKey() when <space> is pressed
        const int SpaceKeyCode = 32;

        static async Task Main(string[] args)
        {
            Console.WriteLine("///////////////////////////////");
            Console.WriteLine("/// VmbNET Frame Conversion ///");
            Console.WriteLine("///////////////////////////////\n");

            string cameraId = "";
            var allocationMode = ICapturingModule.AllocationModeValue.AnnounceFrame;
            if (args.Length == 0)
            {
                // no arguments are OK.
            }
            else if ((args.Length == 1 || args.Length == 2) && args[0] != "-h")
            {
                if (args[0] == "-x")
                {
                    allocationMode = ICapturingModule.AllocationModeValue.AllocAndAnnounceFrame;
                    if (args.Length == 2)
                    {
                        cameraId = args[1];
                    }
                }
                else
                {
                    cameraId = args[0];
                }
            }
            else
            {
                Console.WriteLine("Usage:");
                Console.WriteLine("  FrameConversion.exe [ -x ] [cameraId]");
                Console.WriteLine("  FrameConversion.exe [ -h ]\n");
                Console.WriteLine("Parameters:");
                Console.WriteLine("  [ -x ]      If present, frame buffers are allocated by the transport layer)");
                Console.WriteLine("  cameraId    ID or extended ID of the camera to use (using the first camera found if not specified)");
                Console.WriteLine("  [ -h ]      Display this usage information)\n");
                return;
            }

            var logger = LoggerCreator.CreateLogger();

            IVmbSystem.Logger = logger;

            IFrameConverter.Logger = logger;

            // startup Vimba X
            using var vmbSystem = IVmbSystem.Startup();

            ICamera camera;
            if (cameraId.Length > 0)
            {
                try
                {
                    // get camera with user-supplied ID
                    camera = vmbSystem.GetCameraByID(cameraId);
                }
                catch (VmbNETException e)
                {
                    Console.WriteLine($"Received VmbNET error: \"{e.Message}\" when trying to get camera.");
                    return;
                }
            }
            else
            {
                // get first detected camera
                var cameras = vmbSystem.GetCameras();
                if (cameras.Count > 0)
                {
                    camera = cameras[0];
                }
                else
                {
                    Console.WriteLine("No cameras found.");
                    return;
                }
            }

            // get image transformation parameters from JSON file

            float[,]? colorCorrectionMatrix3x3 = null;
            IFrameConverter.DebayerModeValue? debayerMode = null;
            float? gammaCorrection = null;

            var settings = "transformationSettings.json";
            if (File.Exists(settings))
            {
                var text = File.ReadAllText(settings);
                JObject jObject = JObject.Parse(text);

                if (jObject.TryGetValue("ColorCorrectionMatrix3x3", out JToken? colorCorrectionMatrix3x3JToken))
                {
                    colorCorrectionMatrix3x3 = new float[3, 3];
                    JArray colorCorrectionMatrix3x3JArray = (JArray)colorCorrectionMatrix3x3JToken;
                    for (int i = 0; i < 3; ++i)
                    {
                        for (int j = 0; j < 3; ++j)
                        {
                            colorCorrectionMatrix3x3[i, j] = (float)colorCorrectionMatrix3x3JArray[3 * j + i];
                        }
                    }
                }

                if (jObject.TryGetValue("DebayerMode", out JToken? debayerModeJToken))
                {
                    if (Enum.TryParse(debayerModeJToken.ToObject<string>(), out IFrameConverter.DebayerModeValue debayerModeEnum))
                    {
                        debayerMode = debayerModeEnum;
                    }
                }

                if (jObject.TryGetValue("GammaCorrection", out JToken? gammaCorrectionJToken))
                {
                    gammaCorrection = (float)gammaCorrectionJToken;
                }
            }

            // open the camera
            using var openCamera = camera.Open();

            // for GigE cameras only, adjust the packet size if possible
            if (openCamera.TransportLayer.Type == TransportLayerType.GEV && openCamera.Stream.Features.GVSPAdjustPacketSize.Exists)
            {
                await (Task)openCamera.Stream.Features.GVSPAdjustPacketSize(TimeSpan.FromSeconds(1));
            }

            // Create the format list.
            // Each format has a conversion format list containing the formats that frames can be converted to,
            // which is obtained from the frame converter.
            Dictionary<string, IList<IFrame.PixelFormatValue>> frameFormatToConversionFormats = new();

            foreach (string pixelFormat in openCamera.Features.PixelFormat.EnumEntriesByName.Keys)
            {
                if (openCamera.Features.PixelFormat.EnumEntriesByName[pixelFormat].IsAvailable)
                {
                    frameFormatToConversionFormats[pixelFormat] = IFrameConverter.GetSupportedPixelFormatConversions(pixelFormat);
                }
            }

            if(frameFormatToConversionFormats.Count == 0)
            {
                frameFormatToConversionFormats[openCamera.Features.PixelFormat] = IFrameConverter.GetSupportedPixelFormatConversions(openCamera.Features.PixelFormat);
            }

            // enumerator of the format list
            IEnumerator<KeyValuePair<string, IList<IFrame.PixelFormatValue>>> frameFormatToConversionFormatsEnumerator = frameFormatToConversionFormats.GetEnumerator();
            frameFormatToConversionFormatsEnumerator.MoveNext();

            // enumerator of the conversion format list
            IEnumerator<IFrame.PixelFormatValue> conversionFormatEnumerator = frameFormatToConversionFormatsEnumerator.Current.Value.GetEnumerator();

            // stream capturing
            IStreamCapture? streamCapture = null;

            // frame conversion
            IFrameConverter? frameConverter = null;

            // create disposable concurrent queue for processing the arguments of each frame received event
            using var frameQueue = new BlockingCollection<IFrame>(5);

            IFrame? previousFrame = null;

            // event handler for received frames
            Action<FrameReceivedEventArgs> frameReceivedAction = (FrameReceivedEventArgs frameReceivedEventArgs) =>
            {
                // get the frame without the "using" keyword because it will be disposed on the main thread

                IFrame frame = frameReceivedEventArgs.Frame;

                // Frame must not be disposed on the main thread while the frame event handler is running,
                // since the API cannot know whether it will be disposed on this thread yet.
                // The API does however have this information for the previous frame.
                if (previousFrame != null)
                {
                    // add the event arguments to the queues
                    frameQueue.Add(previousFrame);
                }

                previousFrame = frame;
            };

            // OpenCV window name
            string windowName = string.Empty;

            Action start = () =>
            {
                // move to the next conversion format and the next format if we are at the end of the conversion format list
                while (!conversionFormatEnumerator.MoveNext())
                {
                    // we are at the end of the conversion format list for this format, move to the next format in the format list
                    while (!frameFormatToConversionFormatsEnumerator.MoveNext())
                    {
                        // we are at the end of the format list, go to the beginning
                        frameFormatToConversionFormatsEnumerator.Dispose();
                        frameFormatToConversionFormatsEnumerator = frameFormatToConversionFormats.GetEnumerator();
                    }
                    // we have a format, we can enumerate through its conversion format list if that is not empty
                    conversionFormatEnumerator.Dispose();
                    conversionFormatEnumerator = frameFormatToConversionFormatsEnumerator.Current.Value.GetEnumerator();
                }

                // set the camera format to the format at the current position of the format list
                openCamera.Features.PixelFormat = frameFormatToConversionFormatsEnumerator.Current.Key;

                // set the conversion format to the format at the current position of the conversion format list, and apply transformation settings
                frameConverter = IFrameConverter.Create(conversionFormatEnumerator.Current, 5);
                if(colorCorrectionMatrix3x3 != null && !frameFormatToConversionFormatsEnumerator.Current.Key.StartsWith("Mono"))
                {
                    frameConverter.SetColorCorrectionMatrix3x3(colorCorrectionMatrix3x3);
                }
                if(debayerMode != null && !frameFormatToConversionFormatsEnumerator.Current.Key.StartsWith("Mono"))
                {
                    frameConverter.SetDebayerMode(debayerMode.Value);
                }
                if(gammaCorrection != null)
                {
                    frameConverter.SetGammaCorrection(gammaCorrection.Value);
                }

                // start capturing and acquisition
                streamCapture = openCamera.PrepareCapture(allocationMode, 5);

                openCamera.FrameReceived += (_, frameReceivedEventArgs) => frameReceivedAction(frameReceivedEventArgs);

                ((Task)openCamera.Features.AcquisitionStart(TimeSpan.FromSeconds(1))).Wait();
                if (openCamera.Features.AcquisitionStatus.Exists)
                {
                    while (!(bool)openCamera.Features.AcquisitionStatus) { }
                }

                // set window name
                windowName = $"Stream from {camera.Serial} with pixel format {frameFormatToConversionFormatsEnumerator.Current.Key}, converting to pixel format {conversionFormatEnumerator.Current}.\n" +
                " Press <space> to select the next pixel format and <enter> to stop streaming.";

                Console.WriteLine($"Camera pixel format: {frameFormatToConversionFormatsEnumerator.Current.Key}, converted pixel format: {conversionFormatEnumerator.Current}.");
            };

            Action stop = () =>
            {
                // reset OpenCV parameters and close the wondow
                _displayHeight = null;
                _convertToMatType = null;
                _colorConversionCodes = null;
                _shift = null;
                Cv2.DestroyWindow(windowName);

                // stop acquisition
                ((Task)openCamera.Features.AcquisitionStop(TimeSpan.FromSeconds(1))).Wait();
                if (openCamera.Features.AcquisitionStatus.Exists)
                {
                    while ((bool)openCamera.Features.AcquisitionStatus) { }
                }

                // remove lambda event handler
                openCamera.RemoveAllFrameEventHandlers();

                // clear the frame queue
                while (frameQueue.TryTake(out var frame))
                {
                    frame?.Dispose();
                }
                previousFrame?.Dispose();
                previousFrame = null;

                // end capturing
                streamCapture?.Dispose();
            };

            start();

            // pop each event argument from the queue and process the frame, until the user presses <enter>
            foreach (var frame in frameQueue.GetConsumingEnumerable())
            {
                // display frame if completed
                if (frame.FrameStatus == IFrame.FrameStatusValue.Completed)
                {
                    // convert the received frame to an OpenCV matrix and resize it

                    using var imageMat = CreateOpenCVImage(frameConverter?.Convert(frame) ?? frame);

                    // display the image in the window
                    Cv2.ImShow(windowName, imageMat);
                }

                // manual dispose since frame was obtained on another thread
                frame.Dispose();

                // get pressed key, if any
                var key = Cv2.WaitKey(1);
                if (key == EnterKeyCode)
                {
                    // stop because user pressed <enter>
                    stop();

                    // exit this loop
                    break;
                }
                else if(key == SpaceKeyCode)
                {
                    // restart with new conversion format and possibly new format
                    stop();
                    start();
                }
            }
        }

        private const int _displayWidth = 1024;
        private static int? _displayHeight = null;
        private static MatType? _convertToMatType = null;
        private static ColorConversionCodes? _colorConversionCodes = null;
        private static double? _shift = null;

        private static Mat CreateOpenCVImage(IFrame frame)
        {
            if(_convertToMatType == null)
            {
                SetOpenCVParametersFromPixelFormat(frame.PixelFormat);
                _displayHeight = (int)((double)(_displayWidth * frame.Height) / frame.Width);
            }

            Mat? imageMat = null;

            if (_convertToMatType != null)
            {
                imageMat = new Mat((int)frame.Height, (int)frame.Width, _convertToMatType.Value, frame.ImageData);

                Cv2.Resize(imageMat, imageMat, new Size(_displayWidth, _displayHeight ?? _displayWidth));

                if (_shift != null)
                {
                    imageMat.ConvertTo(imageMat, _convertToMatType.Value, _shift.Value);
                }

                if (_colorConversionCodes != null)
                {
                    Cv2.CvtColor(imageMat, imageMat, _colorConversionCodes.Value);
                }
            }
            else
            {
                imageMat = Mat.Zeros(_displayWidth, _displayHeight ?? _displayWidth, MatType.CV_8UC1);
            }

            return imageMat;
        }

        private static void SetOpenCVParametersFromPixelFormat(IFrame.PixelFormatValue pixelFormat)
        {
            switch (pixelFormat)
            {
                case IFrame.PixelFormatValue.Mono8:
                    _convertToMatType = MatType.CV_8UC1;
                    break;
                case IFrame.PixelFormatValue.Mono10:
                    _convertToMatType = MatType.CV_16UC1;
                    _shift = 64;
                    break;
                case IFrame.PixelFormatValue.Mono12:
                    _convertToMatType = MatType.CV_16UC1;
                    _shift = 16;
                    break;
                case IFrame.PixelFormatValue.Mono14:
                    _convertToMatType = MatType.CV_16UC1;
                    _shift = 4;
                    break;
                case IFrame.PixelFormatValue.Mono16:
                    _convertToMatType = MatType.CV_16UC1;
                    break;
                case IFrame.PixelFormatValue.RGB8:
                    _convertToMatType = MatType.CV_8UC3;
                    _colorConversionCodes = ColorConversionCodes.RGB2BGR;
                    break;
                case IFrame.PixelFormatValue.BGR8:
                    _convertToMatType = MatType.CV_8UC3;
                    break;
                case IFrame.PixelFormatValue.RGBa8:
                    _convertToMatType = MatType.CV_8UC4;
                    _colorConversionCodes = ColorConversionCodes.RGBA2BGRA;
                    break;
                case IFrame.PixelFormatValue.BGRa8:
                    _convertToMatType = MatType.CV_8UC4;
                    break;
                case IFrame.PixelFormatValue.RGB10:
                    _convertToMatType = MatType.CV_16UC3;
                    _colorConversionCodes = ColorConversionCodes.RGB2BGR;
                    _shift = 64;
                    break;
                case IFrame.PixelFormatValue.BGR10:
                    _convertToMatType = MatType.CV_16UC3;
                    _shift = 64;
                    break;
                case IFrame.PixelFormatValue.RGBa10:
                    _convertToMatType = MatType.CV_16UC4;
                    _colorConversionCodes = ColorConversionCodes.RGBA2BGRA;
                    _shift = 64;
                    break;
                case IFrame.PixelFormatValue.BGRa10:
                    _convertToMatType = MatType.CV_16UC4;
                    _shift = 64;
                    break;
                case IFrame.PixelFormatValue.RGB12:
                    _convertToMatType = MatType.CV_16UC3;
                    _colorConversionCodes = ColorConversionCodes.RGB2BGR;
                    _shift = 16;
                    break;
                case IFrame.PixelFormatValue.BGR12:
                    _convertToMatType = MatType.CV_16UC3;
                    _shift = 16;
                    break;
                case IFrame.PixelFormatValue.RGBa12:
                    _convertToMatType = MatType.CV_16UC4;
                    _colorConversionCodes = ColorConversionCodes.RGBA2BGRA;
                    _shift = 16;
                    break;
                case IFrame.PixelFormatValue.BGRa12:
                    _convertToMatType = MatType.CV_16UC4;
                    _shift = 16;
                    break;
                case IFrame.PixelFormatValue.RGB14:
                    _convertToMatType = MatType.CV_16UC3;
                    _colorConversionCodes = ColorConversionCodes.RGB2BGR;
                    _shift = 4;
                    break;
                case IFrame.PixelFormatValue.BGR14:
                    _convertToMatType = MatType.CV_16UC3;
                    _shift = 4;
                    break;
                case IFrame.PixelFormatValue.RGBa14:
                    _convertToMatType = MatType.CV_16UC4;
                    _colorConversionCodes = ColorConversionCodes.RGBA2BGRA;
                    _shift = 4;
                    break;
                case IFrame.PixelFormatValue.BGRa14:
                    _convertToMatType = MatType.CV_16UC4;
                    _shift = 4;
                    break;
                case IFrame.PixelFormatValue.RGB16:
                    _convertToMatType = MatType.CV_16UC3;
                    _colorConversionCodes = ColorConversionCodes.RGB2BGR;
                    break;
                case IFrame.PixelFormatValue.BGR16:
                    _convertToMatType = MatType.CV_16UC3;
                    break;
                case IFrame.PixelFormatValue.RGBa16:
                    _convertToMatType = MatType.CV_16UC4;
                    _colorConversionCodes = ColorConversionCodes.RGBA2BGRA;
                    break;
                case IFrame.PixelFormatValue.BGRa16:
                    _convertToMatType = MatType.CV_16UC4;
                    break;
                case IFrame.PixelFormatValue.BayerBG8:
                    _convertToMatType = MatType.CV_8UC1;
                    _colorConversionCodes = ColorConversionCodes.BayerBG2RGB;
                    break;
                case IFrame.PixelFormatValue.BayerBG10:
                case IFrame.PixelFormatValue.BayerBG10p:
                    _convertToMatType = MatType.CV_16UC1;
                    _colorConversionCodes = ColorConversionCodes.BayerBG2RGB;
                    _shift = 64;
                    break;
                case IFrame.PixelFormatValue.BayerBG12:
                case IFrame.PixelFormatValue.BayerBG12p:
                case IFrame.PixelFormatValue.BayerBG12Packed:
                    _convertToMatType = MatType.CV_16UC1;
                    _colorConversionCodes = ColorConversionCodes.BayerBG2RGB;
                    _shift = 16;
                    break;
                case IFrame.PixelFormatValue.BayerBG16:
                    _convertToMatType = MatType.CV_16UC1;
                    _colorConversionCodes = ColorConversionCodes.BayerBG2RGB;
                    break;
                case IFrame.PixelFormatValue.Yuv422:
                    _convertToMatType = MatType.CV_8UC2;
                    _colorConversionCodes = ColorConversionCodes.YUV2RGB_Y422;
                    break;
                default:
                    break;
            }
        }
    }
}
