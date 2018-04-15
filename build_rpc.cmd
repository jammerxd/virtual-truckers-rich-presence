rd /s /q "build"
mkdir build
cd build
cmake ..  -DCMAKE_GENERATOR_PLATFORM=x64 -DCMAKE_INSTALL_PREFIX=G:\Projects\Dependencies\discord-rpc-3.2.0\build-x64-Debug
cmake --build . --config Debug --target install

cd ..
rd /s /q "build"
mkdir build
cd build
cmake ..  -DCMAKE_GENERATOR_PLATFORM=x64 -DCMAKE_INSTALL_PREFIX=G:\Projects\Dependencies\discord-rpc-3.2.0\build-x64-Release
cmake  --build . --config Release --target install
cd ..
rd /s /q "build"
