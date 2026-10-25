#include "http_policy.h"
#include <assert.h>
#include <string>
#include <stdio.h>
int main() {
    using namespace stage5;
    for(const char* s:{"gzip","GZip","deflate, gzip","gzip;q=1.000","*;q=0.5","gzip;q=0.001","br;q=0, gzip ; q=1"}) assert(acceptsGzip(s));
    for(const char* s:{"","identity","br","gzip;q=0","*;q=0","gzip;q=0,*;q=1","gzip;q=1.1","gzip;q=0.0000","gzip;","gzip;q","gzip;q=","gzip;q=.5","gzip,,br","gzip,","gzip,gzip"}) assert(!acceptsGzip(s));
    assert(!acceptsGzip(nullptr));assert(!acceptsGzip(std::string(257,'a').c_str()));
    assert(allowedHost("192.168.4.1") && allowedHost("192.168.4.1:80"));
    assert(!allowedHost(nullptr) && !allowedHost("evil") && !allowedHost("192.168.4.1.evil") && !allowedHost("192.168.4.1:8080"));
    assert(allowedOrigin(nullptr) && allowedOrigin("") && allowedOrigin("http://192.168.4.1"));
    assert(!allowedOrigin("null") && !allowedOrigin("http://evil") && !allowedOrigin("https://192.168.4.1"));
    for(size_t i=0;i<web_assets::bundle().count;++i) {
        const auto& a=web_assets::bundle().assets[i];
        const auto r=assetResponse(a.path,"gzip");
        assert(r.status==200 && r.asset==&a && r.asset->size==a.size);
        assert(!strcmp(r.asset->content_encoding,"gzip"));
        assert(std::string(r.cache).find(i?"immutable":"no-store")!=std::string::npos);
        assert(assetResponse(a.path,"identity").status==406);
        assert(assetResponse(a.path,nullptr).status==406);
        // Simulate actual bounded fragmented sending for every real compressed asset.
        Budget b;b.opened=100;
        size_t offset=0;
        while(offset<a.size) {size_t n=(a.size-offset)>7?7:a.size-offset;assert(b.allow(100+offset,n,true));offset+=n;}
        assert(b.sent==a.size);
    }
    assert(assetResponse("/unknown","gzip").status==404);
    assert(assetResponse("/../","gzip").status==404 && assetResponse(nullptr,"gzip").status==404);
    Budget slow;slow.opened=10;
    for(int i=0;i<30;++i) assert(slow.allow(10+i*100000,1,false));
    assert(!slow.allow(10+session_us,1,false)); // Drip bytes never renew the total deadline.
    Budget receive;assert(receive.allow(1,max_rx_bytes,false));assert(!receive.allow(2,1,false));
    Budget send;assert(send.allow(1,max_tx_bytes,true));assert(!send.allow(2,1,true));
    Budget overflow;assert(!overflow.allow(1,size_t(-1),false));
    Budget disconnected;assert(!disconnected.allow(session_us,0,true));
    for(int clients=1;clients<=3;++clients) {
        Budget parallel[3];
        for(int c=0;c<clients;++c) assert(parallel[c].allow(0,512,false));
        for(int c=0;c<clients;++c) assert(!parallel[c].allow(session_us,1,false));
    }
    puts("PASS actual HTTP routing/encoding/cache/ACL policy; real flash assets; partial transfers; slow/idle/oversize/disconnected budgets and bounded multi-client simulation");
}
