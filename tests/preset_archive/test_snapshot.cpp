// Actual native adapter/Controller, simulated NVS only. Optional private example
// is read from argv, never embedded in tracked fixtures or printed as values.
#include "preset_store.h"
#include <assert.h>
#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
namespace fake_nvs {std::map<std::string,std::vector<uint8_t>> data;std::map<unsigned,std::string> handles;unsigned next=0,reads=0,bank_reads=0;bool fail_write=false;}
using Clock=std::chrono::steady_clock;
uint64_t us(Clock::time_point start){return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count());}
int main(int argc,char** argv){
 preset_archive::Collection example;example.present=15;
 if(argc==2){std::ifstream file(argv[1],std::ios::binary);std::string data((std::istreambuf_iterator<char>(file)),{});assert(preset_archive::parse(data.data(),data.size(),example));}
 for(bool bank:{false,true})for(unsigned slots:{4,8,0}){
  fake_nvs::data.clear();preset_archive::Collection input;
  if(slots==4)input=example;else if(slots==8){input.present=255;memset(input.values,255,sizeof(input.values));}
  PresetStore storage;
  if(bank)assert(storage.replaceAll(input,{})==preset_archive::Commit::ok);
  else for(unsigned i=0;i<8;++i)if(input.present&(1U<<i)){uint8_t packet[513]{};memcpy(packet+1,input.values[i],512);assert(storage.save(uint8_t(i),packet));}
  DmxController controller(storage);controller.setChannel(23,112);controller.setBlackout(true);controller.setChannel(23,9);
  auto revision=controller.revision();fake_nvs::reads=fake_nvs::bank_reads=0;auto start=Clock::now();assert(controller.captureArchive()==DmxController::Result::ok);auto capture=us(start);
  assert(controller.revision()==revision&&controller.channel(23)==9&&controller.blackoutActive());controller.setBlackout(false);assert(controller.channel(23)==112);
  const unsigned reads=fake_nvs::reads,bank_reads=fake_nvs::bank_reads;assert(controller.archive().present==input.present);
  for(unsigned i=0;i<8;++i)if(input.present&(1U<<i))assert(!memcmp(controller.archive().values[i],input.values[i],512));
#ifdef EXPECT_BULK_SNAPSHOT
  if(bank)assert(bank_reads==1&&reads==2); // One selector and one verified generation, regardless of occupancy.
#endif
  char output[preset_archive::max_file];start=Clock::now();auto bytes=preset_archive::serialize(controller.archive(),output,sizeof(output));auto json=us(start);assert(bytes);
  printf("SNAPSHOT bank=%u slots=%u nvs_reads=%u bank_reads=%u capture_host_us=%llu serialize_host_us=%llu bytes=%u\n",bank,slots,reads,bank_reads,(unsigned long long)capture,(unsigned long long)json,unsigned(bytes));
 }
}
