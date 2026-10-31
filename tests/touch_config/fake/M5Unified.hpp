#pragma once
#include <stdint.h>
namespace m5 { enum class board_t { other=0, board_M5StackCoreS3SE=20 }; }
namespace lgfx {
enum error_t { unknown_err, invalid_arg, connection_lost, mode_mismatch, periph_device_err };
namespace cpp {
struct Result {
    bool ok; uint8_t byte; error_t code;
    Result(bool valid=true, uint8_t value=0, error_t error=unknown_err):ok(valid),byte(value),code(error){}
    Result(uint8_t value):ok(true),byte(value),code(unknown_err){}
    bool has_value() const { return ok; }
    bool has_error() const { return !ok; }
    uint8_t value() const { return byte; }
    error_t error() const { return code; }
    explicit operator bool() const { return ok; }
};
template<class T, class E> using result = Result;
inline Result fail(error_t error) { return {false,0,error}; }
}
namespace i2c {
cpp::Result beginTransaction(int,int,uint32_t,bool);
cpp::Result writeBytes(int,const uint8_t*,size_t);
cpp::Result restart(int,int,uint32_t,bool);
cpp::Result readBytes(int,uint8_t*,size_t,bool);
cpp::Result endTransaction(int);
cpp::Result transactionWriteRead(int,int,const uint8_t*,uint8_t,uint8_t*,size_t,uint32_t);
cpp::Result readRegister8(int,int,uint8_t,uint32_t);
}
}
struct FakeTouchDriver {
    struct Config { int i2c_port=0,i2c_addr=0x38,pin_sda=12,pin_scl=11,pin_int=21,pin_cs=-1; uint32_t freq=400000; } cfg;
    Config config() const { return cfg; }
};
struct FakePanel { FakeTouchDriver driver; bool available=true;
    const FakeTouchDriver* getTouch() const { return available?&driver:nullptr; }
};
struct FakeDisplay { FakePanel device; bool available=true;
    const FakePanel* panel() { return available?&device:nullptr; }
};
struct FakeTouch {
    struct Detail {bool pressed=false; bool isPressed() const{return pressed;}};
    static constexpr size_t TOUCH_MAX_POINTS=5;
    bool enabled=true; unsigned count=0; Detail details[5];
    bool isEnabled() const { return enabled; }
    unsigned getCount() const {return count;}
    const Detail& getDetail(size_t i) const {return details[i];}
};
struct FakeBus { int port=0,sda=12,scl=11; int getPort() const{return port;} int getSDA() const{return sda;} int getSCL() const{return scl;} };
struct FakeM5 { FakeDisplay Display; FakeTouch Touch; FakeBus In_I2C;
    m5::board_t board=m5::board_t::board_M5StackCoreS3SE;
    m5::board_t getBoard() const {return board;}
};
extern FakeM5 M5;
