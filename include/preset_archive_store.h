#pragma once
#include "preset_archive.h"

namespace preset_archive {
enum class Io {ok,missing,error};
struct Kv {
 virtual ~Kv()=default;
 virtual Io selector(uint8_t& value)=0;
 virtual bool select(uint8_t value)=0;
 virtual Io read(unsigned bank,uint8_t* out,size_t& size)=0;
 virtual bool write(unsigned bank,const uint8_t* data,size_t size)=0;
 virtual bool erase(unsigned bank)=0; // Inactive bank only; missing is success.
 virtual bool commit()=0;
};
// One scratch buffer, application-task owner only. Missing selector means an
// empty current-format collection; corrupt selected data always fails closed.
class Store {
 public:
 enum class State : uint8_t {empty,active,corrupt,error};
 private:
 uint8_t raw_[16+slots*channels]{};
 uint8_t selected_=0;
 State state_=State::error;
 public:
 static constexpr size_t bytes=16+slots*channels;
 static uint32_t crc(const uint8_t* p,size_t size){uint32_t c=~0U;for(size_t i=0;i<size;++i){uint8_t b=i>=12&&i<16?0:p[i];c^=b;for(unsigned bit=0;bit<8;++bit)c=(c>>1)^(0xEDB88320U&uint32_t(-int32_t(c&1)));}return ~c;}
 static uint32_t word(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
 static void word(uint8_t* p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=uint8_t(n>>(8*i));}
 void seal(){word(raw_+12,crc(raw_,bytes));}
 bool valid()const{return !memcmp(raw_,"DMP1",4)&&raw_[4]==1&&!raw_[5]&&!raw_[6]&&!raw_[7]&&!raw_[9]&&!raw_[10]&&!raw_[11]&&word(raw_+12)==crc(raw_,bytes);}
 State load(Kv& kv){
  state_=State::error;uint8_t value=0;const auto s=kv.selector(value);
  if(s==Io::missing){
   memset(raw_,0,bytes);memcpy(raw_,"DMP1",4);raw_[4]=1;seal();
   selected_=0;return state_=State::empty;
  }
  if(s!=Io::ok)return state_;if(value>1)return state_=State::corrupt;
  size_t size=bytes;const auto read=kv.read(value,raw_,size);
  if(read==Io::error)return state_;if(read!=Io::ok||size!=bytes||!valid())return state_=State::corrupt;
  selected_=value;return state_=State::active;
 }
 uint8_t mask()const{return raw_[8];}
 const uint8_t* values(unsigned slot)const{return raw_+16+slot*channels;}
 void mask(uint8_t value){raw_[8]=value;}
 void value(unsigned slot,const uint8_t* data){memcpy(raw_+16+slot*channels,data,channels);raw_[8]|=uint8_t(1U<<slot);}
 Commit publish(Kv& kv,const Gate& gate){
  if((state_!=State::empty&&state_!=State::active)||!gate.allowed())return Commit::failed;
  const bool first=state_==State::empty;
  const uint8_t previous=selected_,next=first?0:uint8_t(previous^1);
  seal();const uint32_t expected=word(raw_+12);
  state_=State::error; // Scratch may be overwritten; require reload after failure.
  if(!kv.erase(next)||!kv.commit()||!kv.write(next,raw_,bytes)||!kv.commit())return Commit::failed;
  size_t size=bytes;
  if(kv.read(next,raw_,size)!=Io::ok||size!=bytes||!valid()||word(raw_+12)!=expected||!gate.allowed())return Commit::failed;
  // The only visibility boundary. Resolve APIs that report an error after an
  // atomic selector write actually succeeded. Never replay an uncertain commit.
  const bool set=kv.select(next);const bool committed=set&&kv.commit();
  uint8_t actual=2;const auto observed=kv.selector(actual);
  if(observed==Io::ok&&actual==next){selected_=next;state_=State::active;return committed?Commit::ok:Commit::uncertain;}
  if((first&&observed==Io::missing)||(!first&&observed==Io::ok&&actual==previous))return Commit::failed;
  return Commit::uncertain; // Both generations retained; reboot chooses one.
 }
 Commit replace(Kv& kv,const Collection& input,const Gate& gate){
  const auto state=load(kv);if(state==State::corrupt||state==State::error)return Commit::failed;
  memset(raw_,0,bytes);memcpy(raw_,"DMP1",4);raw_[4]=1;raw_[8]=input.present;memcpy(raw_+16,input.values,slots*channels);
  return publish(kv,gate);
 }
};
}
