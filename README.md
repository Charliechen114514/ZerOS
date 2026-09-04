# ZerOS: A toy C++23 written OS

for fun!

```shell
# configure (cross toolchain, once)
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arch/arm-none-eabi.cmake

# build & run in Renode
cmake --build build --target renode-bluepill
```


