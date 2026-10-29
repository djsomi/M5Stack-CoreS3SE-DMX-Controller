#include "device_maintenance.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <string.h>
#define ESP_DMX_VERSION 2
#define DMX_MAX_PACKET_SIZE 513
const int dmxPort=1;
std::vector<int> calls;
struct FakeTransport {
    bool stage(const uint8_t* packet,unsigned size){assert(size==513);for(unsigned i=0;i<size;++i)assert(packet[i]==0);calls.push_back(1);return true;}
} transport;
void dmx_wait_send_done(int port,int ticks){assert(port==1&&ticks==50);calls.push_back(2);}
void delay(int ms){assert(ms==10);calls.push_back(3);}
struct FakeM5 {
    struct Power {
        bool usb=false,battery=false,external=false,charging=true;
        unsigned reads=0;
        bool isVBUS(){++reads;return usb;}
        bool getBatState(){++reads;return battery;}
        void powerOff(){calls.push_back(4);}
    } Power;
} M5;
#include "power_original.h"
int main(){
    for(unsigned usb=0;usb<2;++usb)for(unsigned battery=0;battery<2;++battery)for(unsigned external=0;external<2;++external){
        M5.Power.usb=usb;M5.Power.battery=battery;M5.Power.external=external;calls.clear();
        performOriginalPowerOff();
        const std::vector<int> expected{1,2,3,1,2,3,1,2,3,4};
        assert(calls==expected&&M5.Power.reads==0&&M5.Power.charging);
    }
    puts("PASS original Power Off: exact stage/wait50/delay10 x3 then M5.Power.powerOff; USB/battery/external combinations never gate or change charging configuration (Unit/Base)");
}
