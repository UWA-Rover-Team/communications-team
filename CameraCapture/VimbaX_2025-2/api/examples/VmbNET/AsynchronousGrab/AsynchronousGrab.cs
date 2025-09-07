/*=============================================================================
  Copyright (C) 2024-2025 Allied Vision Technologies. All Rights Reserved.
  Subject to the BSD 3-Clause License.
=============================================================================*/

using System;
using System.Threading;
using VmbNET;

namespace AsynchronousGrab
{
    /// <summary>
    /// Example program to acquire images asynchronously.
    /// </summary>
    class AsynchronousGrab
    {
        static void Main(string[] args)
        {
            Console.WriteLine("////////////////////////////////////////");
            Console.WriteLine("/// VmbNET Asynchronous Grab Example ///");
            Console.WriteLine("////////////////////////////////////////\n");

            // startup Vimba X
            using var vmbSystem = IVmbSystem.Startup();

            // get camera
            ICamera? camera = null;
            try
            {
                // get camera with ID if supplied as argument, otherwise the first detected camera
                camera = args.Length > 0 ? vmbSystem.GetCameraByID(args[0]) : vmbSystem.GetCameras()[0];
            }
            catch { }

            if (camera != null)
            {
                using (var openCamera = camera.Open())
                {
                    Console.WriteLine($"Opened camera with ID {openCamera.Id}");

                    // register an event handler for the "FrameReceived" event
                    openCamera.FrameReceived += (s, e) =>
                    {
                        using var frame = e.Frame;
                        Console.WriteLine($"Frame received! ID={frame.Id}");
                    }; // IDisposable: leaving the scope will automatically requeue the frame

                    using var acquisition = openCamera.StartFrameAcquisition();
                    Thread.Sleep(5000);
                } // IDisposable: leaving the scope will automatically stop the acquisition and close the camera

                Console.WriteLine("Acquisition stopped and camera closed.");
            }
            else
            {
                Console.WriteLine(args.Length == 0 ? "No cameras found." : $"No camera with ID: {args[0]} found.");
            }
        } // IDisposable: leaving the scope will automatically shutdown VmbSystem
    }
}
