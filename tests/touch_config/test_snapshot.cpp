#include "touch_configuration_snapshot.h"
#include "M5Unified.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
uint32_t fake_us=1000000;
FakeM5 M5;
#if DMX_TOUCH_CONFIG_DIAGNOSTICS
using namespace touch_configuration;
namespace {
uint8_t bytes[256]={}; unsigned calls=0;
std::vector<unsigned> seen;
int fail_reg=-1; unsigned fail_op=0; uint8_t selected=0; bool locked=false;
const uint8_t allowed[]={0xA3,0xA6,0xA8,0x80,0x85,0x86,0x87,0x88,0x89,0xA5,0xA1,0xA2};
lgfx::cpp::Result result(unsigned op) {
    fake_us+=10;
    return {!(selected==fail_reg && fail_op==op),0,lgfx::connection_lost};
}
ReadResult read(void*,uint8_t reg) {
    ++calls;seen.push_back(reg);fake_us+=100;
    ReadResult r;r.ok=reg!=fail_reg;r.value=bytes[reg];r.error=r.ok?0:2;return r;
}
uint32_t clockNow(){return fake_us++;}
void seed() {
    memset(bytes,0,sizeof(bytes));bytes[0xA3]=0x64;bytes[0xA6]=0x03;bytes[0xA8]=0x20;
    bytes[0x85]=0x57; // Deliberately not the reference default 0xA0.
    calls=0;seen.clear();fail_reg=-1;fail_op=0;locked=false;
}
std::string output(const Snapshot& s) {
    char line[768];std::string text;
    for(size_t row=0;row<Snapshot::row_count;++row){auto n=s.format(row,line,sizeof(line));assert(n && n<sizeof(line)-1 && line[n-1]=='\n');text.append(line,n);}
    assert(s.format(Snapshot::row_count,line,sizeof(line))==0);return text;
}
}
namespace lgfx { namespace i2c {
cpp::Result beginTransaction(int port,int address,uint32_t hz,bool is_read) {
    assert(port==0 && address==0x38 && hz==400000 && !is_read && !locked);
    locked=true;++calls;return {true,0,unknown_err};
}
cpp::Result writeBytes(int port,const uint8_t* data,size_t length) {
    // The ONLY transmitted data is the single register-address selector.
    assert(locked && port==0 && length==1);selected=data[0];seen.push_back(selected);
    return result(1);
}
cpp::Result restart(int port,int address,uint32_t hz,bool is_read) {
    assert(locked && port==0 && address==0x38 && hz==400000 && is_read);return result(2);
}
cpp::Result readBytes(int port,uint8_t* data,size_t length,bool nack) {
    assert(locked && port==0 && length==1 && nack);auto r=result(3);if(r.ok)*data=bytes[selected];return r;
}
cpp::Result endTransaction(int port) { assert(locked && port==0);locked=false;return result(4); }
// Exact installed helpers, extracted without changes by the test runner.
#include "read_under_test.h"
}}
#endif
int main(int argc,char** argv) {
#if DMX_TOUCH_CONFIG_DIAGNOSTICS
    seed();Context context;context.admission=Admission::ready;context.task="loopTask";
    Snapshot s;assert(s.capture(context,read,clockNow,nullptr));assert(s.identity_ok && calls==12);
    for(size_t i=0;i<12;++i)assert(seen[i]==allowed[i]);
    auto text=output(s);assert(text.find("name=THDIFF status=ok hex=0x57 dec=87")!=std::string::npos);
    assert(text.find("marking_scope=visible_only")!=std::string::npos);
    assert(!s.capture(context,read,clockNow,nullptr) && calls==12);
    char small[8];assert(s.format(0,small,sizeof(small))==7 && !small[7]);
    for(unsigned bad=0;bad<3;++bad) {
        seed();bytes[allowed[bad]]^=1;Snapshot unknown;unknown.capture(context,read,clockNow,nullptr);
        assert(!unknown.identity_ok && calls==3);
        for(size_t i=3;i<12;++i)assert(unknown.entries[i].status==Status::skipped_identity);
    }
    for(unsigned failed=0;failed<12;++failed) {
        seed();fail_reg=allowed[failed];Snapshot failure;failure.capture(context,read,clockNow,nullptr);
        assert(calls==(failed<3?3:12));assert(failure.entries[failed].status==Status::error);
        assert(output(failure).find("status=read_error hex=NA dec=NA rc=2")!=std::string::npos);
    }
    seed();bytes[0x85]=0;Snapshot zero;zero.capture(context,read,clockNow,nullptr);
    assert(output(zero).find("name=THDIFF status=ok hex=0x00 dec=0")!=std::string::npos);
    seed();fake_us=0xFFFFFFF0U;Snapshot wrap;wrap.capture(context,read,clockNow,nullptr);
    assert(wrap.identity_ok && uint32_t(wrap.end_us-wrap.start_us)<2000);
    assert(output(wrap).find("[TOUCH_CFG_END]")!=std::string::npos);
    for(auto why:{Admission::other_board,Admission::touch_disabled,Admission::no_driver,Admission::bus_mismatch}) {
        seed();context.admission=why;Snapshot skipped;skipped.capture(context,read,clockNow,nullptr);assert(!calls);
        for(auto& e:skipped.entries)assert(e.status==Status::skipped_context);
    }
    seed();std::string mode=argc>1?argv[1]:"ready";
    if(mode=="board")M5.board=m5::board_t::other;
    if(mode=="touch")M5.Touch.enabled=false;
    if(mode=="panel")M5.Display.available=false;
    if(mode=="driver")M5.Display.device.available=false;
    if(mode=="port")M5.In_I2C.port=1;
    if(mode=="sda")M5.In_I2C.sda=1;
    if(mode=="scl")M5.In_I2C.scl=1;
    if(mode=="address")M5.Display.device.driver.cfg.i2c_addr=0x39;
    if(mode=="spi")M5.Display.device.driver.cfg.pin_cs=1;
    if(mode=="frequency")M5.Display.device.driver.cfg.freq=400001;
    if(mode=="identity")bytes[0xA6]=0x04;
    if(mode=="failure"){fail_reg=0x85;fail_op=3;}
    if(mode=="stop_failure"){fail_reg=0x85;fail_op=4;}
    captureStartup();captureStartup();unsigned total=calls;
    assert(total==(mode=="ready" || mode=="failure" || mode=="stop_failure"?12:mode=="identity"?3:0));
    std::string serial;char line[768];unsigned rows=0;
    while(pending()){auto n=formatNext(line,sizeof(line));assert(n && n<sizeof(line)-1 && line[n-1]=='\n');serial.append(line,n);++rows;}
    assert(rows==14 && !formatNext(line,sizeof(line)) && calls==total && !locked);
    captureStartup();assert(!pending() && calls==total);
    assert(serial.find("[TOUCH_CFG_END]")!=std::string::npos);
    if(mode=="ready")assert(serial.find("hex=0x57 dec=87")!=std::string::npos);
    if(mode=="failure" || mode=="stop_failure")assert(serial.find("name=THDIFF status=read_error hex=NA dec=NA rc=2")!=std::string::npos);
    printf("PASS snapshot core + actual startup adapter %s: once, exact allowlist, identity/context gates, zero vs errors, no configuration payload, release/stop errors, deferred bounded rows\n",mode.c_str());
#else
    (void)argc;(void)argv;touch_configuration::captureStartup();char line[5];
    assert(!touch_configuration::pending() && !touch_configuration::formatNext(line,sizeof(line)));
    puts("PASS diagnostics disabled: no reads or output");
#endif
}
