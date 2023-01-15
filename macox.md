# macOS
Chip:	Apple M1 Pro

Model Name:	MacBook Pro

System Version:	macOS 12.6.1 


## Build
```

export VULKAN_SDK=~/VulkanSDK/1.3.231.1/macOS
export PATH=${PATH}:${VULKAN_SDK}/bin:/opt/homebrew/bin
export DYLD_LIBRARY_PATH=$VULKAN_SDK/lib:$DYLD_LIBRARY_PATH
export VK_ICD_FILENAMES=$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH=$VULKAN_SDK/etc/vulkan/explicit_layer.d

cd ~
git clone --recursive https://github.com/glscopeclient/scopehal-apps.git
cd scopehal-apps
mkdir build
cd build
cmake ../ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH="/opt/homebrew;/opt/homebrew/opt/libomp" 
make -j4

```
## Log

```




OMP_WAIT_POLICY not set to PASSIVE
Re-exec'ing with correct environment
Initializing Vulkan
    VK_KHR_get_physical_device_properties2: supported
    VK_EXT_debug_utils: supported
    Loader/API support available for Vulkan 1.3
    Vulkan 1.2 support available, requesting it
    Initializing glfw 3.3.8 Cocoa NSGL EGL OSMesa dynamic
    GLFW required extensions:
        VK_KHR_surface
        VK_EXT_metal_surface
    Physical devices:
        Device 0: Apple M1 Pro
            API version:            0x004020e7 (0.1.2.231)
            Driver version:         0x000027d8 (0.0.2.2008)
            Vendor ID:              106b
            Device ID:              c0603ef
            Device type:            Integrated GPU
            int64:                  yes
            int16:                  yes (allowed in SSBOs)
            int8:                   yes (allowed in SSBOs)
            Max image dim 2D:       16384
            Max storage buf range:  4095 MB
            Max mem alloc:          1024 MB
            Max compute shared mem: 32 KB
            Max compute grp count:  1073741824 x 1073741824 x 1073741824
            Max compute invocs:     1024
            Max compute grp size:   1024 x 1024 x 1024
            Memory types:
                Type 0
                    Heap index: 0
                    Device local
                Type 1
                    Heap index: 0
                    Device local
                    Host visible
                    Host coherent
                    Host cached
                Type 2
                    Heap index: 0
                    Device local
                    Host visible
                    Host cached
                Type 3
                    Heap index: 0
                    Device local
                    Lazily allocated
            Memory heaps:
                Heap 0
                    Size: 16 GB
                    Device local
        Selected device 0
            Queue families (4 total)
                Queue type 0
                    Queue count:          1
                    Timestamp valid bits: 64
                    Graphics
                    Compute
                    Transfer
                Queue type 1
                    Queue count:          1
                    Timestamp valid bits: 64
                    Graphics
                    Compute
                    Transfer
                Queue type 2
                    Queue count:          1
                    Timestamp valid bits: 64
                    Graphics
                    Compute
                    Transfer
                Queue type 3
                    Queue count:          1
                    Timestamp valid bits: 64
                    Graphics
                    Compute
                    Transfer
            Enabling 64-bit integer support
            Enabling 16-bit integer support
            Enabling 16-bit integer support for SSBOs
            Enabling 8-bit integer support
            Enabling 8-bit integer support for SSBOs
            Device has VK_KHR_portability_subset, requesting it
            Using type 1 for pinned host memory
            Using type 0 for card-local memory
            Sorted queues:
                Family=0 Index=0 Flags=00000007
                Family=1 Index=0 Flags=00000007
                Family=2 Index=0 Flags=00000007
                Family=3 Index=0 Flags=00000007
            QueueManager creating family=0 index=0 name=g_vkTransferQueue
    
    vkFFT version: 1.2.29
Detecting CPU features...
QueueManager creating family=1 index=0 name=g_mainWindow.render
Using ImGui version 1.89.1 WIP
QueueManager creating family=2 index=0 name=FilterGraphExecutor[0].queue
QueueManager creating family=3 index=0 name=FilterGraphExecutor[5].queue

```
