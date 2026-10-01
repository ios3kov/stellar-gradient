// Runs the real sg_metal_render entry with Objective-C metadata test doubles.
// No MTLDevice, GPU commands, shaders or After Effects are executed here.
#include "StellarBridge.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <initializer_list>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

@interface SGTestBuffer : NSObject {
@public
    NSUInteger byteLength;
    unsigned lengthReads;
}
- (NSUInteger)length;
@end
@implementation SGTestBuffer
- (NSUInteger)length { ++lengthReads; return byteLength; }
@end

@interface SGTestQueue : NSObject {
@public
    unsigned devices, commands;
}
- (id<MTLDevice>)device;
- (id<MTLCommandBuffer>)commandBuffer;
@end
@implementation SGTestQueue
- (id<MTLDevice>)device { ++devices; return nil; }
- (id<MTLCommandBuffer>)commandBuffer { ++commands; return nil; }
@end

namespace {
unsigned checks=0, failures=0;
void expect(bool ok, const char* name) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr,"FAIL: %s\n",name); }
}
SGRenderStateC state() {
    SGRenderStateC s{};
    s.input_rect={0,0,2,2}; s.output_rect=s.input_rect;
    s.work_rect=s.input_rect; s.source_max_rect=s.input_rect;
    return s;
}
struct Fixture {
    // Never dereferenced: valid requests hit the deliberately nil command buffer
    // before pipelines. Invalid requests must return even earlier.
    int unusedContext=0;
    SGTestBuffer* input=[[SGTestBuffer alloc] init];
    SGTestBuffer* output=[[SGTestBuffer alloc] init];
    SGTestQueue* queue=[[SGTestQueue alloc] init];
    Fixture() { input->byteLength=4096; output->byteLength=4096; }
    ~Fixture() { [queue release]; [output release]; [input release]; }
    int render(SGRenderStateC& s, int iw=2, int ih=2, int irb=32,
               int ow=2, int oh=2, int orb=32) {
        return sg_metal_render(&unusedContext,(void*)queue,(void*)input,(void*)output,
                               &s,iw,ih,irb,ow,oh,orb);
    }
    void rejected(int rc, const char* name) {
        expect(rc==-1,name);
        expect(queue->devices==0 && queue->commands==0,"invalid request touched queue");
    }
};
}
int main(int argc, char** argv) {
    NSAutoreleasePool* pool=[[NSAutoreleasePool alloc] init];
    if (argc==2 && std::strcmp(argv[1],"--rowbytes-reproducer")==0) {
        Fixture f; auto s=state();
        const int rc=f.render(s,2,2,36);
        std::printf("rowbytes=36 width=2 result=%d commands=%u; expected=-1 commands=0\n",rc,f.queue->commands);
        const bool ok=rc==-1 && f.queue->commands==0;
        [pool drain];
        return ok ? 0 : 1;
    }
    const int invalidRows[]={-16,0,4,16,28,33,36,40,44};
    for (int rb:invalidRows) {
        Fixture f; auto s=state(); f.rejected(f.render(s,2,2,rb),"input rowbytes not rejected");
        expect(f.input->lengthReads==0 && f.output->lengthReads==0,"bad pitch read buffer objects");
    }
    for (int rb:invalidRows) {
        Fixture f; auto s=state(); f.rejected(f.render(s,2,2,32,2,2,rb),"output rowbytes not rejected");
    }
    for (bool inputShort:{true,false}) for (NSUInteger length:{NSUInteger(0),NSUInteger(63)}) {
        Fixture f; auto s=state();
        (inputShort ? f.input : f.output)->byteLength=length;
        f.rejected(f.render(s),"short buffer not rejected");
    }
    // Valid input/output pitches may differ. Final-row padding is optional.
    for (int irb:{32,48,80}) for (int orb:{32,64,96}) {
        Fixture f; auto s=state();
        f.input->byteLength=static_cast<NSUInteger>(irb+32);
        f.output->byteLength=static_cast<NSUInteger>(orb+32);
        expect(f.render(s,2,2,irb,2,2,orb)==-3,"valid padded buffers rejected by validation");
        expect(f.queue->commands==1,"valid layout never reached encoder boundary");
        expect(f.input->lengthReads==1 && f.output->lengthReads==1,"capacity getters not checked once");
    }
    {
        Fixture f; auto s=state(); s.output_rect.left=-1;
        f.rejected(f.render(s),"negative crop");
    }
    {
        Fixture f; auto s=state(); s.output_rect.top=1;
        f.rejected(f.render(s),"crop beyond texture");
    }
    {
        Fixture f; auto s=state(); s.work_rect.left=std::numeric_limits<int32_t>::min();
        f.rejected(f.render(s),"overflowing work extent");
    }
    {
        Fixture f; auto s=state(); s.input_rect.top=std::numeric_limits<int32_t>::min();
        f.rejected(f.render(s),"overflowing source offset");
    }
    {
        Fixture f; auto s=state();
        f.rejected(f.render(s,2,std::numeric_limits<int32_t>::max(),48),"uint32 shader index wrap");
    }
    {
        Fixture f; auto s=state();
        s.work_rect={-4,-6,20,18}; s.output_rect={0,0,12,8};
        s.input_rect={-2,-3,14,12}; s.source_max_rect={0,0,12,8};
        expect(f.render(s,16,15,272,12,8,208)==-3,"valid cropped layout rejected");
        expect(f.queue->commands==1,"valid crop failed to reach encoder boundary");
    }
    for (int missing=0;missing<5;++missing) {
        Fixture f; auto s=state();
        f.rejected(sg_metal_render(missing==0?nullptr:&f.unusedContext,
            missing==1?nullptr:(void*)f.queue, missing==2?nullptr:(void*)f.input,
            missing==3?nullptr:(void*)f.output,missing==4?nullptr:&s,2,2,32,2,2,32),"null input");
    }
    std::printf("Metal entry validation: %u checks, %u failures; Objective-C doubles, GPU/AE NOT RUN\n",checks,failures);
    [pool drain];
    return failures ? 1 : 0;
}
