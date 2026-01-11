{
  "targets": [{ # you can have multiple targets. generlly only need one
    "target_name": "addon", # THe output file name
    "sources": [ "src/main.cpp" ], # All the cpp files that need to be compiled if they are linked
    "include_dirs": [ # Where to find header files
      "<!@(node -p \"require('node-addon-api').include\")"
    ],
    "cflags!": [ "-fno-exceptions" ], # remove compieer flags
    "cflags_cc!": [ "-fno-exceptions" ], # remove compiler flags
    "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ] #ignore cpp exceptions
  }]
}