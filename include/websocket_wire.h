#pragma once
#include <stddef.h>
#include <stdint.h>

namespace websocket_bridge {
// Stream guard BEFORE IDF decoding. IDF 4.4.7 narrows extended frame lengths to
// size_t (32-bit on ESP32) and does not expose RSV bits to the URI handler.
// Reject early pipelined data at upgrade so native pending HTTP bytes cannot
// bypass this guard. Standard browsers wait for the 101 response.
class WireGuard {
 public:
    void httpBytes(const char* data,size_t length) {
        const char end[]={ '\r','\n','\r','\n' };
        for(size_t i=0;i<length;++i) {
            if(header_done_){trailing_=true;continue;}
            matched_=data[i]==end[matched_]?uint8_t(matched_+1):uint8_t(data[i]=='\r'?1:0);
            if(matched_==4)header_done_=true;
        }
    }
    bool trailing() const {return trailing_;}
    bool wsBytes(const char* data,size_t length) {
        if(failed_)return false;
        for(size_t i=0;i<length;++i) {
            const auto c=uint8_t(data[i]);
            switch(stage_) {
            case 0:
                opcode_=c&15;
                if((c&0xf0)!=0x80||(opcode_!=1&&opcode_!=8&&opcode_!=9&&opcode_!=10))return fail();
                stage_=1;break;
            case 1:
                if(!(c&0x80)||(c&127)==127||((opcode_&8)&&(c&127)>125))return fail();
                remaining_=c&127;
                if(remaining_==126){remaining_=0;stage_=2;}
                else if(!payloadLength())return fail();
                break;
            case 2:remaining_=uint16_t(c)<<8;stage_=3;break;
            case 3:
                remaining_|=c;
                if(remaining_<126||remaining_>256||!payloadLength())return fail();
                break;
            case 4:if(--mask_==0)stage_=remaining_?5:0;break;
            case 5:if(--remaining_==0)stage_=0;break;
            default:return fail();
            }
        }
        return true;
    }
 private:
    bool payloadLength() {
        if((opcode_==1&&!remaining_)||(opcode_==8&&remaining_==1))return false;
        mask_=4;stage_=4;return true;
    }
    bool fail(){failed_=true;return false;}
    uint16_t remaining_=0;
    uint8_t matched_=0,stage_=0,opcode_=0,mask_=0;
    bool header_done_=false,trailing_=false,failed_=false;
};
}
