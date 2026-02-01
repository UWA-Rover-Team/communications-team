{
  "targets": [
    {
      "target_name": "addon",
      "sources": [ "src/main.cpp" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "C:/Program Files/Allied Vision/Vimba X/api/include"
      ],
      "conditions": [
        ["OS=='win'", {
          "libraries": [
            "C:/Program Files/Allied Vision/Vimba X/api/lib/VmbCPP.lib"
          ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1
            },
            "VCLinkerTool": {
              "AdditionalLibraryDirectories": [
                "C:/Program Files/Allied Vision/Vimba X/api/lib"
              ]
            }
          },
          "copies": [
            {
              "destination": "<(module_root_dir)/build/Release/",
              "files": [
                "C:/Program Files/Allied Vision/Vimba X/api/bin/VmbCPP.dll"
              ]
            }
          ]
        }]
      ]
    }
  ]
}