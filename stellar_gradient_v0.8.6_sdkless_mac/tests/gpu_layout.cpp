#include "gpu/SharedParams.h"
#include <cstdio>
int main(){
    if(sizeof(stellar::gpu::ParamsGPU)!=344) return 1;
    std::printf("PASS: gpu_layout size=%zu\n",sizeof(stellar::gpu::ParamsGPU));
    return 0;
}
