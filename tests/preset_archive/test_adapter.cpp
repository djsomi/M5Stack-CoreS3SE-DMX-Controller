#include "preset_store.h"
#include <assert.h>
namespace fake_nvs {std::map<std::string,std::vector<uint8_t>> data;std::map<unsigned,std::string> handles;unsigned next=0,reads=0,bank_reads=0;bool fail_write=false;}
int main(){
 PresetStore storage;DmxController controller(storage);uint8_t packet[513]={0};packet[1]=24;packet[512]=255;
 assert(!controller.presetExists(0)&&controller.captureArchive()==DmxController::Result::ok&&controller.archive().present==0&&fake_nvs::data.empty());
 assert(storage.save(0,packet)&&storage.save(1,packet));assert(controller.clearPreset(1)==DmxController::Result::ok);
 assert(controller.presetExists(0)&&!controller.presetExists(1));assert(controller.captureArchive()==DmxController::Result::ok&&controller.archive().present==1);
 preset_archive::Collection collection;collection.present=0x84;collection.values[2][0]=99;collection.values[7][511]=17;
 fake_nvs::fail_write=true;assert(storage.replaceAll(collection,{})==preset_archive::Commit::failed);fake_nvs::fail_write=false;
 assert(controller.presetExists(0));assert(storage.replaceAll(collection,{})==preset_archive::Commit::ok);
 PresetStore reboot;DmxController after(reboot);assert(!after.presetExists(0)&&after.presetExists(2)&&after.presetExists(7));assert(after.loadPreset(2)==DmxController::Result::ok&&after.channel(1)==99);
 after.setChannel(1,231);assert(after.savePreset(0)==DmxController::Result::ok);assert(after.copyPreset(0,1)==DmxController::Result::ok);assert(after.clearPreset(2)==DmxController::Result::ok);
 for(const auto& item:fake_nvs::data)assert(item.first=="dmxpreset/xactive"||item.first=="dmxpreset/xbank0"||item.first=="dmxpreset/xbank1");
 PresetStore reboot2;DmxController final(reboot2);assert(final.presetExists(0)&&final.presetExists(1)&&!final.presetExists(2)&&final.presetExists(7));assert(final.loadPreset(1)==DmxController::Result::ok&&final.channel(1)==231);
 auto empty=preset_archive::Collection{};assert(reboot2.replaceAll(empty,{})==preset_archive::Commit::ok);PresetStore reboot3;DmxController cleared(reboot3);for(unsigned i=0;i<8;++i)assert(!cleared.presetExists(i));
 fake_nvs::data["dmxpreset/xactive"]={8};assert(reboot3.inspect(0)==DmxPresetStorage::SlotState::corrupt);assert(reboot3.replaceAll(collection,{})==preset_archive::Commit::failed);
 // Obsolete keys are neither interpreted nor erased, even after current-format writes.
 fake_nvs::data.clear();for(unsigned i=1;i<=8;++i)fake_nvs::data["dmxpreset/p"+std::to_string(i)]=std::vector<uint8_t>(513,27);
 fake_nvs::data["dmxmeta/cleared"]={0};auto ignored=fake_nvs::data;
 PresetStore fresh;DmxController current(fresh);assert(current.captureArchive()==DmxController::Result::ok&&!current.archive().present&&fake_nvs::data==ignored);
 assert(current.savePreset(0)==DmxController::Result::ok);for(const auto& item:ignored)assert(fake_nvs::data[item.first]==item.second);
 puts("PASS current-format adapter: fresh empty/read-only, first Save, atomic replacement/failure, reboot, Save/Copy/Load/Delete, empty bank, corrupt selector and obsolete-key isolation");
}
