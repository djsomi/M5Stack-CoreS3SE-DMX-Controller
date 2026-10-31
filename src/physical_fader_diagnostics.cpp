#include "physical_fader_diagnostics.h"
#include "touch_acquisition_diagnostics.h"
#if DMX_FADER_DIAGNOSTICS
#include <Arduino.h>
#include <M5Unified.hpp>
#include <esp_timer.h>
#include <stdio.h>

namespace physical_fader {
namespace {
Recorder recorder;
char line[512];size_t length=0,sent=0,row=0;
uint32_t last_tx=0;
bool input_active=false;
const char* names[]={"loop_gap","M5_update","owner","local_command","main_bar","grid","precision"};
size_t transitionCount(const Report& r){return r.transition_count<Report::transition_capacity?r.transition_count:Report::transition_capacity;}
size_t tailCount(const Report& r){return r.event_count<Report::capacity?r.event_count:Report::capacity;}
size_t endRow(const Report& r){return 28+r.onset_count+2*transitionCount(r)+tailCount(r);}
int formatEvent(const Report& r,const Event& e,const char* stream,size_t index,int before=-1) {
    return snprintf(line,sizeof(line),"[FADER] id=%lu stream=%s index=%u event=%c us=%lu poll_us=%lu m5_ms=%lu raw=%d,%d unfiltered=%d,%d detail=%d,%d previous=%d state=%u point=%u count=%u cache_match=%u mapped=%d map_us=%lu accepted=%d accept_us=%lu rendered=%d render_us=%lu\n",
        (unsigned long)r.id,stream,(unsigned)index,e.kind,(unsigned long)e.at,(unsigned long)e.touch.us,(unsigned long)e.touch.m5_ms,
        e.touch.raw_x,e.touch.raw_y,e.touch.unfiltered_x,e.touch.unfiltered_y,e.touch.x,e.touch.y,before,e.touch.state,e.touch.id,e.touch.count,e.touch.cache_match?1U:0U,
        e.mapped,(unsigned long)e.map_us,e.accepted,(unsigned long)e.accept_us,e.rendered,(unsigned long)e.render_us);
}
void prepare(const Report& r) {
    int n=0;
    if(row==0) {
        n=snprintf(line,sizeof(line),"[FADER] id=%lu page=%s ch=%u start_us=%lu end_us=%lu polls=%lu raw_moves=%lu processed_moves=%lu held_raw=%lu id_changes=%lu releases=%lu dropped_gestures=%lu bytes=%u\n",
            (unsigned long)r.id,r.page==Page::main?"main":"precision",r.channel,(unsigned long)r.start,(unsigned long)r.end,
            (unsigned long)r.polls_count,(unsigned long)r.raw_moves,(unsigned long)r.processed_moves,(unsigned long)r.held_raw_moves,
            (unsigned long)r.id_changes,(unsigned long)r.release_edges,(unsigned long)recorder.dropped,(unsigned)sizeof(recorder));
    } else if(row==1) {
        n=snprintf(line,sizeof(line),"[FADER] id=%lu mapped_changes=%lu initial_delta=%lu max_map_delta=%lu skipped_integers=%lu jumps_ge10=%lu accepted=%lu attempts=%lu unchanged_attempts=%lu max_raw_dy=%lu max_processed_dy=%lu tail_overwritten=%lu\n",
            (unsigned long)r.id,(unsigned long)r.map_changes,(unsigned long)r.initial_delta,(unsigned long)r.max_map_delta,
            (unsigned long)r.skipped_values,(unsigned long)r.jumps10,(unsigned long)r.accepted_changes,(unsigned long)r.attempts,
            (unsigned long)r.unchanged_attempts,(unsigned long)r.max_raw_dy,(unsigned long)r.max_processed_dy,(unsigned long)r.overwritten);
    } else if(row<14) {
        const size_t i=row-2;
        const Stats* s=i<7?&r.stages[i]:i==7?&r.polls:i==8?&r.raw_changes:i==9?&r.accepted_gaps:i==10?&r.visual_lag:&r.touch_to_accept;
        const char* label=i<7?names[i]:i==7?"cached_poll_gap":i==8?"raw_change_gap":i==9?"accepted_gap":i==10?"accept_to_render_return":"cached_poll_to_accept";
        n=snprintf(line,sizeof(line),"[FADER] id=%lu metric=%s n=%lu min_us=%lu mean_us=%lu max_us=%lu bins=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
            (unsigned long)r.id,label,(unsigned long)s->count,(unsigned long)(s->count?s->minimum:0),
            (unsigned long)(s->count?s->sum/s->count:0),(unsigned long)s->maximum,
            (unsigned long)s->histogram[0],(unsigned long)s->histogram[1],(unsigned long)s->histogram[2],(unsigned long)s->histogram[3],
            (unsigned long)s->histogram[4],(unsigned long)s->histogram[5],(unsigned long)s->histogram[6],(unsigned long)s->histogram[7]);
    } else if(row==14) {
        n=snprintf(line,sizeof(line),"[FADER] id=%lu onset_ms=1500 interval_ms=25 onset_samples=%lu transitions=%lu transition_overflow=%lu y_moves=%lu last_pressed_state=%u last_pressed_us=%lu observation=cache_not_sensor_sample\n",
            (unsigned long)r.id,(unsigned long)r.onset_count,(unsigned long)r.transition_count,(unsigned long)r.transition_overflow,
            (unsigned long)r.y_moves,r.last_pressed_state,(unsigned long)r.last_pressed_us);
    } else if(row<21) {
        const size_t i=row-15;const auto& p=r.periods[i];
        const char* labels[]={"0-250","250-500","500-750","750-1000","1000-1500","1500+"};
        n=snprintf(line,sizeof(line),"[FADER] id=%lu age_ms=%s polls=%lu xy_moves=%lu y_moves=%lu accepted=%lu rendered=%lu max_poll_us=%lu max_loop_us=%lu max_M5_us=%lu max_owner_us=%lu max_xy_change_gap_us=%lu max_y_change_gap_us=%lu max_xy_unchanged_us=%lu max_y_unchanged_us=%lu\n",
            (unsigned long)r.id,labels[i],(unsigned long)p.polls,(unsigned long)p.xy_moves,(unsigned long)p.y_moves,(unsigned long)p.accepted,(unsigned long)p.rendered,
            (unsigned long)p.max_poll_us,(unsigned long)p.max_loop_us,(unsigned long)p.max_m5_us,(unsigned long)p.max_owner_us,
            (unsigned long)p.max_xy_gap_us,(unsigned long)p.max_y_gap_us,(unsigned long)p.max_xy_unchanged_us,(unsigned long)p.max_y_unchanged_us);
    } else if(row<25) {
        const size_t i=row-21;const auto& s=r.speeds[i];
        const char* labels[]={"0-10","10-40","40-100","100+"};
        n=snprintf(line,sizeof(line),"[FADER] id=%lu observed_y_px_per_s=%s moves=%lu pixels=%lu mean_gap_us=%lu max_gap_us=%lu max_step_px=%lu\n",
            (unsigned long)r.id,labels[i],(unsigned long)s.moves,(unsigned long)s.pixels,
            (unsigned long)(s.moves?s.gap_sum/s.moves:0),(unsigned long)s.max_gap_us,(unsigned long)s.max_step);
    } else if(row<27) {
        const auto& s=row==25?r.unchanged_xy:r.unchanged_y;
        n=snprintf(line,sizeof(line),"[FADER] id=%lu unchanged=%s runs=%lu total_us=%llu max_us=%lu ge20ms=%lu ge50ms=%lu ge100ms=%lu physical_stationary=unknown\n",
            (unsigned long)r.id,row==25?"XY":"Y",(unsigned long)s.runs,(unsigned long long)s.total_us,(unsigned long)s.max_us,
            (unsigned long)s.ge20ms,(unsigned long)s.ge50ms,(unsigned long)s.ge100ms);
    } else if(row==27) n=formatEvent(r,r.first,"begin",0);
    else if(row==endRow(r)) n=snprintf(line,sizeof(line),"[FADER_END] id=%lu\n",(unsigned long)r.id);
    else {
        size_t i=row-28;
        if(i<r.onset_count) {
            size_t slot=0,ordinal=0;
            for(;slot<Report::onset_capacity;++slot)if(r.onset_used[slot] && ordinal++==i)break;
            n=formatEvent(r,r.onset[slot],"onset",slot);
        } else if((i-=r.onset_count)<2*transitionCount(r)) {
            const auto& t=r.transitions[i/2];
            if(i%2 && !t.has_after)n=snprintf(line,sizeof(line),"[FADER] id=%lu stream=state_after index=%u missing=1\n",(unsigned long)r.id,(unsigned)(i/2));
            else n=formatEvent(r,i%2?t.after:t.edge,i%2?"state_after":"state_edge",i/2,t.from);
        } else {
            i-=2*transitionCount(r);
            const auto& e=r.events[(r.event_count-tailCount(r)+i)%Report::capacity];
            n=formatEvent(r,e,"tail",i);
        }
    }
    length=n>0?size_t(n):0;if(length>=sizeof(line))length=sizeof(line)-1;sent=0;
}
}
uint32_t nowUs() {return static_cast<uint32_t>(esp_timer_get_time());}
void loopStart(uint32_t now) {recorder.loopStart(now);}
void duration(Stage stage,uint32_t us) {recorder.duration(stage,us);}
void finish(Stage stage,uint32_t start) {const uint32_t end=nowUs();recorder.duration(stage,end-start,end);}
void captureTouch() {
    const uint32_t gesture=recorder.activeGestureId();
    const int tracked=recorder.trackedId();size_t index=0;
    bool found=tracked<0;input_active=false;
    const auto count=M5.Touch.getCount();
    for(size_t i=0;i<count && i<M5.Touch.TOUCH_MAX_POINTS;++i) {
        const auto& point=M5.Touch.getTouchPointRaw(i);const auto& detail=M5.Touch.getDetail(i);
        if(point.id==detail.id && detail.isPressed())input_active=true;
        if(point.id==tracked && detail.id==tracked){found=true;index=i;}
    }
    if(!found) {
        Touch lost=recorder.lastTouch();lost.us=nowUs();lost.m5_ms=M5.getUpdateMsec();lost.state=0;lost.count=count;lost.cache_match=false;
        recorder.observe(lost);if(gesture && !recorder.activeGestureId())touch_acquisition::endGesture(gesture);return;
    }
    const auto& d=M5.Touch.getDetail(index);auto p=M5.Touch.getTouchPointRaw(index);
    Touch t;t.us=nowUs();t.raw_x=p.x;t.raw_y=p.y;t.state=static_cast<uint8_t>(d.state);
    t.m5_ms=M5.getUpdateMsec();t.x=d.x;t.y=d.y;t.id=d.id;t.count=count;
    // Transform the cached raw point only. No second sensor read or touch update.
    M5.Display.convertRawXY(&p,1);t.unfiltered_x=p.x;t.unfiltered_y=p.y;recorder.observe(t);
    if(gesture && !recorder.activeGestureId())touch_acquisition::endGesture(gesture);
}
void mapped(Page p,uint16_t ch,int value,int before) {
    const uint32_t old=recorder.activeGestureId();recorder.mapped(p,ch,value,before,nowUs());
    if(!old && recorder.activeGestureId())touch_acquisition::beginGesture(recorder.activeGestureId(),recorder.trackedId());
}
void accepted(uint16_t ch,int before,int after) {recorder.accepted(ch,before,after,nowUs());}
void visual(Page p,int value) {recorder.visual(p,value,nowUs());}
bool touchActive() {return input_active || recorder.touchActive();}
bool service() {
    const auto* r=recorder.pending();if(!r)return false;
    if(touchActive() || !Serial)return true;
    if(sent==length && row==endRow(*r) && touch_acquisition::service(r->id))return true;
    const uint32_t now=millis();if(uint32_t(now-last_tx)<4)return true;
    last_tx=now;int available=Serial.availableForWrite();if(available<=0)return true;
    if(sent==length)prepare(*r);
    size_t n=length-sent;if(n>40)n=40;if(n>size_t(available))n=size_t(available);
    sent+=Serial.write(reinterpret_cast<const uint8_t*>(line+sent),n);
    if(sent==length) {
        if(row==endRow(*r)){recorder.releasePending();row=0;}else ++row;
    }
    return true;
}
}
#endif
