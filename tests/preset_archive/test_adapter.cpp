#include "preset_store.h"
#include <assert.h>
namespace fake_nvs {std::map<std::string,std::vector<uint8_t>> data;std::map<unsigned,std::string> handles;unsigned next=0,reads=0,bank_reads=0;bool fail_write=false;}
int main(){
 PresetStore storage;DmxController controller(storage);uint8_t packet[513]={0};packet[1]=24;packet[512]=255;
 assert(storage.save(0,packet)&&storage.save(1,packet));assert(storage.saveClearedMask(2)); // Genuine legacy v0.2 layout and logical tombstone.
 auto legacy=fake_nvs::data;assert(controller.presetExists(0)&&!controller.presetExists(1));assert(controller.captureArchive()==DmxController::Result::ok&&controller.archive().present==1);
 preset_archive::Collection collection;collection.present=0x84;collection.values[2][0]=99;collection.values[7][511]=17;
 fake_nvs::fail_write=true;assert(storage.replaceAll(collection,{})==preset_archive::Commit::failed);fake_nvs::fail_write=false;
 assert(controller.presetExists(0));assert(storage.replaceAll(collection,{})==preset_archive::Commit::ok);
 for(auto& p:legacy)assert(fake_nvs::data[p.first]==p.second); // Initial adoption retains legacy until the next verified bank mutation.
 PresetStore reboot;DmxController after(reboot);assert(!after.presetExists(0)&&after.presetExists(2)&&after.presetExists(7));assert(after.loadPreset(2)==DmxController::Result::ok&&after.channel(1)==99);
 after.setChannel(1,231);assert(after.savePreset(0)==DmxController::Result::ok);assert(after.copyPreset(0,1)==DmxController::Result::ok);assert(after.clearPreset(2)==DmxController::Result::ok);
 for(unsigned i=1;i<=8;++i)assert(!fake_nvs::data.count("dmxpreset/p"+std::to_string(i)));
 assert(fake_nvs::data["dmxmeta/cleared"]==legacy["dmxmeta/cleared"]);
 PresetStore reboot2;DmxController final(reboot2);assert(final.presetExists(0)&&final.presetExists(1)&&!final.presetExists(2)&&final.presetExists(7));assert(final.loadPreset(1)==DmxController::Result::ok&&final.channel(1)==231);
 auto empty=preset_archive::Collection{};assert(reboot2.replaceAll(empty,{})==preset_archive::Commit::ok);PresetStore reboot3;DmxController cleared(reboot3);for(unsigned i=0;i<8;++i)assert(!cleared.presetExists(i));
 fake_nvs::data["dmxpreset/xactive"]={8};assert(reboot3.inspect(0)==DmxPresetStorage::SlotState::corrupt);assert(reboot3.replaceAll(collection,{})==preset_archive::Commit::failed);
 puts("PASS actual PresetStore/native-NVS adapter with simulated NVS: legacy v0.2 raw records/tombstones, failed-space/write retention, atomic adoption, reboot, save/copy/load/delete after import, empty collection and corrupt-selector fail-closed");
}
