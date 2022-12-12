## Debugging under Xcode

Build the plugin

```
cd <OpenCPN build directory>
cmake -GXcode ..
mkdir -p Debug/OpenCPN.app/Contents/PlugIns
mkdir Debug/OpenCPN.app/Contents/PlugIns/NSK
ln -s <NSK_pi build directory>/Debug/libNSK_pi.dylib Debug/OpenCPN.app/Contents/PlugIns/libNSK_pi.dylib
ln -s <NSK_pi source directory>/data/* Debug/OpenCPN.app/Contents/PlugIns/NSK
```

Build OpenCPN

Run OpenCPN from inside Xcode
