#pragma once
#include "hardware_diagnostics.h"
#ifndef DMX_FADER_DIAGNOSTICS
#define DMX_FADER_DIAGNOSTICS 0
#endif
#if DMX_FADER_DIAGNOSTICS && !DMX_HW_DIAGNOSTICS
#error "Fader diagnostics use the existing diagnostics USB output; enable DMX_HW_DIAGNOSTICS"
#endif

#if DMX_FADER_DIAGNOSTICS
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <new>

namespace physical_fader {
enum class Page : uint8_t { main, precision };
enum class Stage : uint8_t { loop, m5, owner, command, bar, grid, precision, count };
struct Stats {
    uint64_t sum = 0;
    uint32_t count = 0, minimum = UINT32_MAX, maximum = 0;
    uint32_t histogram[8] = {};
    void add(uint32_t us) {
        ++count; sum += us;
        if (us < minimum) minimum = us;
        if (us > maximum) maximum = us;
        const uint32_t limits[7] = {1000,5000,10000,20000,50000,100000,250000};
        size_t i=0; while(i<7 && us>=limits[i]) ++i;
        ++histogram[i];
    }
};
struct Touch {
    uint32_t us=0, m5_ms=0;
    int16_t raw_x=0, raw_y=0, unfiltered_x=0, unfiltered_y=0, x=0, y=0;
    uint8_t state=0, id=0, count=0;bool cache_match=true;
};
struct Event {
    Touch touch;
    uint32_t at=0;
    int16_t mapped=-1, accepted=-1;
    char kind='?';
    uint32_t map_us=0, accept_us=0, render_us=0;
    int16_t rendered=-1;
};
struct Period {
    uint32_t polls=0, xy_moves=0, y_moves=0, accepted=0, rendered=0;
    uint32_t max_poll_us=0, max_loop_us=0, max_m5_us=0, max_owner_us=0;
    uint32_t max_xy_gap_us=0, max_y_gap_us=0, max_xy_unchanged_us=0, max_y_unchanged_us=0;
};
struct Speed {
    uint64_t gap_sum=0;
    uint32_t moves=0,pixels=0,max_gap_us=0,max_step=0;
};
struct Unchanged {
    uint64_t total_us=0;
    uint32_t runs=0,max_us=0,ge20ms=0,ge50ms=0,ge100ms=0;
    void add(uint32_t us) {
        if(!us)return;
        ++runs;total_us+=us;if(us>max_us)max_us=us;
        ge20ms+=(us>=20000);ge50ms+=(us>=50000);ge100ms+=(us>=100000);
    }
};
struct Transition {
    Event edge,after;
    uint8_t from=0;bool has_after=false;
};
struct Report {
    static constexpr size_t capacity=32, onset_capacity=61, transition_capacity=16;
    Stats stages[static_cast<size_t>(Stage::count)], polls, raw_changes, accepted_gaps, visual_lag, touch_to_accept;
    Event first, events[capacity];
    // First observation in each 25ms window, never overwritten by later windows.
    Event onset[onset_capacity];bool onset_used[onset_capacity]={};
    Transition transitions[transition_capacity];
    Period periods[6];Speed speeds[4];Unchanged unchanged_xy,unchanged_y;
    uint32_t transition_count=0,transition_overflow=0,onset_count=0,y_moves=0;
    uint32_t last_pressed_us=0;uint8_t last_pressed_state=0;
    uint32_t id=0, start=0, end=0, polls_count=0, raw_moves=0, processed_moves=0;
    uint32_t held_raw_moves=0, id_changes=0, release_edges=0, map_changes=0;
    uint32_t skipped_values=0, jumps10=0, accepted_changes=0, attempts=0, unchanged_attempts=0;
    uint32_t max_raw_dy=0, max_processed_dy=0, max_map_delta=0, initial_delta=0;
    uint32_t event_count=0, overwritten=0;
    uint16_t channel=0;
    Page page=Page::main;
};

// Single application-task owner. Fixed storage, no allocation/formatting/I/O.
class Recorder {
 public:
    static size_t periodIndex(uint32_t age) {
        return age<250000?0:age<500000?1:age<750000?2:age<1000000?3:age<1500000?4:5;
    }
    void loopStart(uint32_t now) {
        if(active_>=0 && have_loop_) {
            report().stages[0].add(now-last_loop_);
            maximum(report().periods[periodIndex(now-report().start)].max_loop_us,now-last_loop_);
        }
        last_loop_=now; have_loop_=true;
    }
    void duration(Stage stage,uint32_t us) {
        duration(stage,us,last_.us);
    }
    void duration(Stage stage,uint32_t us,uint32_t end) {
        if(active_>=0 && stage!=Stage::loop) {
            report().stages[static_cast<size_t>(stage)].add(us);
            auto& p=report().periods[periodIndex(end-report().start)];
            if(stage==Stage::m5)maximum(p.max_m5_us,us);
            if(stage==Stage::owner)maximum(p.max_owner_us,us);
        }
    }
    void observe(const Touch& sample) {
        if(active_>=0) {
            auto& r=report(); ++r.polls_count; r.polls.add(sample.us-last_.us);
            const bool raw=sample.raw_x!=last_.raw_x || sample.raw_y!=last_.raw_y;
            const bool processed=sample.x!=last_.x || sample.y!=last_.y;
            const bool continuing=(sample.state&1) && (last_.state&1) && sample.id==last_.id;
            const bool y_changed=sample.unfiltered_y!=last_.unfiltered_y;
            auto& p=r.periods[periodIndex(sample.us-r.start)];++p.polls;
            maximum(p.max_poll_us,sample.us-last_.us);
            if(last_.state&1) {
                maximum(p.max_xy_unchanged_us,sample.us-last_raw_);
                maximum(p.max_y_unchanged_us,sample.us-last_y_);
            }
            if(raw && continuing) {
                const uint32_t gap=sample.us-last_raw_;
                ++r.raw_moves;++p.xy_moves;r.raw_changes.add(gap);
                maximum(p.max_xy_gap_us,gap);r.unchanged_xy.add(gap);last_raw_=sample.us;
                const auto dy=distance(sample.unfiltered_y,last_.unfiltered_y);
                if(dy>r.max_raw_dy) r.max_raw_dy=dy;
                if(!processed) ++r.held_raw_moves;
            }
            if(y_changed && continuing) {
                const uint32_t gap=sample.us-last_y_;
                const uint32_t dy=distance(sample.unfiltered_y,last_.unfiltered_y);
                ++r.y_moves;++p.y_moves;maximum(p.max_y_gap_us,gap);r.unchanged_y.add(gap);
                size_t s=uint64_t(dy)*1000000<=uint64_t(gap)*10?0:
                    uint64_t(dy)*1000000<=uint64_t(gap)*40?1:
                    uint64_t(dy)*1000000<=uint64_t(gap)*100?2:3;
                auto& speed=r.speeds[s];++speed.moves;speed.pixels+=dy;speed.gap_sum+=gap;
                maximum(speed.max_gap_us,gap);maximum(speed.max_step,dy);last_y_=sample.us;
            }
            if(processed && continuing) {
                ++r.processed_moves;
                const auto dy=distance(sample.y,last_.y);
                if(dy>r.max_processed_dy) r.max_processed_dy=dy;
            }
            if(sample.id!=last_.id && (sample.state&1)) ++r.id_changes;
            if(!(sample.state&1) && (last_.state&1)) ++r.release_edges;
            const uint8_t before=last_.state;
            last_=sample;
            if(sample.state&1){r.last_pressed_us=sample.us;r.last_pressed_state=sample.state;}
            // A transition's next observation is retained even if it changes state again.
            const size_t transitions=r.transition_count<Report::transition_capacity?r.transition_count:Report::transition_capacity;
            if(transitions && !r.transitions[transitions-1].has_after) {
                auto& t=r.transitions[transitions-1];t.after=makeEvent('P',sample.us);t.has_after=true;
            }
            if(sample.state!=before) stateChange(before,sample.us);
            onset();
            if(raw || processed || sample.state!=previous_state_) event('T',sample.us);
            if(!(sample.state&1) && !unchanged_closed_) {
                r.unchanged_xy.add(sample.us-last_raw_);r.unchanged_y.add(sample.us-last_y_);unchanged_closed_=true;
            }
            if(!sample.state) {r.end=sample.us; ready_[active_]=true; active_=-1;}
        } else last_=sample;
        previous_state_=sample.state;
        if(!sample.state) suppressed_=false;
    }
    void mapped(Page page,uint16_t channel,int value,int before,uint32_t now) {
        if(suppressed_) return;
        if(active_<0) {
            int slot=!ready_[0]?0:!ready_[1]?1:-1;
            if(slot<0) {++dropped; suppressed_=true; return;}
            active_=slot; new (&reports_[slot]) Report();
            auto& r=report();r.id=++sequence_;r.start=last_.us;r.channel=channel;r.page=page;
            mapped_=before;accepted_=before;last_raw_=last_y_=last_.us;last_accepted_=last_mapped_=last_visual_=0;
            rendered_=-1;have_accepted_=false;have_map_=false;awaiting_visual_=false;unchanged_closed_=false;
            r.first=makeEvent('B',now);
            r.last_pressed_state=last_.state;r.last_pressed_us=last_.us;
            stateChange(0,last_.us);onset();
        }
        auto& r=report();if(r.channel!=channel || r.page!=page) return;
        if(value!=mapped_) {
            const auto delta=distance(value,mapped_);++r.map_changes;
            if(have_map_) {
                if(delta>r.max_map_delta) r.max_map_delta=delta;
                if(delta>1) r.skipped_values+=delta-1;
                if(delta>=10) ++r.jumps10;
            } else r.initial_delta=delta;
            mapped_=value;last_mapped_=now;event('M',now);refreshSaved();
        }
        have_map_=true;
    }
    void accepted(uint16_t channel,int before,int after,uint32_t now) {
        if(active_<0 || report().channel!=channel) return;
        auto& r=report();++r.attempts;
        if(before==after) {++r.unchanged_attempts;return;}
        if(have_accepted_) r.accepted_gaps.add(now-last_accepted_);
        ++r.accepted_changes;accepted_=after;last_accepted_=now;have_accepted_=true;awaiting_visual_=true;
        ++r.periods[periodIndex(now-r.start)].accepted;
        r.touch_to_accept.add(now-last_.us);
        event('A',now);refreshSaved();
    }
    void visual(Page page,int value,uint32_t now) {
        if(active_<0 || report().page!=page || value!=accepted_ || !awaiting_visual_) return;
        report().visual_lag.add(now-last_accepted_);last_visual_=now;rendered_=value;
        ++report().periods[periodIndex(now-report().start)].rendered;event('V',now);refreshSaved();
        awaiting_visual_=false;
    }
    bool touchActive() const {return last_.state!=0;}
    int trackedId() const {return active_>=0?reports_[active_].first.touch.id:-1;}
    uint32_t activeGestureId() const {return active_>=0?reports_[active_].id:0;}
    Touch lastTouch() const {return last_;}
    const Report* pending() const {
        if(ready_[0] && ready_[1]) return reports_[0].id<reports_[1].id?&reports_[0]:&reports_[1];
        return ready_[0]?&reports_[0]:ready_[1]?&reports_[1]:nullptr;
    }
    void releasePending() {const auto* p=pending();if(p)ready_[p==&reports_[0]?0:1]=false;}
    uint32_t dropped=0;
 private:
    static void maximum(uint32_t& old,uint32_t value){if(value>old)old=value;}
    void stateChange(uint8_t before,uint32_t now) {
        auto& r=report();const auto i=r.transition_count++;
        if(i>=Report::transition_capacity){++r.transition_overflow;return;}
        r.transitions[i].edge=makeEvent('S',now);r.transitions[i].from=before;
    }
    void onset() {
        auto& r=report();const uint32_t age=last_.us-r.start;
        if(age>1500000)return;
        const auto i=age/25000;
        if(!r.onset_used[i]){r.onset_used[i]=true;++r.onset_count;}
        else if(r.onset[i].touch.us!=last_.us)return;
        r.onset[i]=makeEvent('W',last_.us);
    }
    void refreshSaved() {
        onset();auto& r=report();
        size_t n=r.transition_count<Report::transition_capacity?r.transition_count:Report::transition_capacity;
        // At most the current edge and previous transition's follow-up can refer
        // to this observation. Keep original edge timestamps; attach response times.
        const size_t first=n>1?n-2:0;
        for(size_t i=first;i<n;++i) {
            auto& t=r.transitions[i];
            if(t.edge.touch.us==last_.us)t.edge=makeEvent('S',t.edge.at);
            if(t.has_after && t.after.touch.us==last_.us)t.after=makeEvent('P',t.after.at);
        }
    }
    static uint32_t distance(int a,int b) {return a>b?uint32_t(a-b):uint32_t(b-a);}
    Report& report() {return reports_[active_];}
    Event makeEvent(char kind,uint32_t now) const {
        Event e;e.touch=last_;e.at=now;e.kind=kind;e.mapped=mapped_;e.accepted=accepted_;
        e.map_us=last_mapped_;e.accept_us=last_accepted_;e.render_us=last_visual_;e.rendered=rendered_;return e;
    }
    void event(char kind,uint32_t now) {
        auto& r=report();r.events[r.event_count%Report::capacity]=makeEvent(kind,now);
        ++r.event_count;if(r.event_count>Report::capacity)++r.overwritten;
    }
    Report reports_[2];Touch last_;
    bool ready_[2]={false,false},have_loop_=false,have_accepted_=false,have_map_=false,suppressed_=false,awaiting_visual_=false,unchanged_closed_=false;
    uint8_t previous_state_=0;
    int active_=-1,mapped_=-1,accepted_=-1,rendered_=-1;
    uint32_t sequence_=0,last_loop_=0,last_raw_=0,last_y_=0,last_accepted_=0,last_mapped_=0,last_visual_=0;
};
static_assert(sizeof(Recorder)<=18432,"Fader recorder must remain bounded below 18KiB");

uint32_t nowUs();
void loopStart(uint32_t now);
void duration(Stage stage,uint32_t us);
void finish(Stage stage,uint32_t start);
void captureTouch();
void mapped(Page page,uint16_t channel,int value,int before);
void accepted(uint16_t channel,int before,int after);
void visual(Page page,int value);
bool touchActive();
bool service();
class Scope {
 public:
    explicit Scope(Stage stage):stage_(stage),at_(nowUs()) {if(stage==Stage::loop)loopStart(at_);}
    ~Scope() {finish(stage_,at_);}
 private: Stage stage_;uint32_t at_;
};
}
#define FADER_SCOPE(name,stage) physical_fader::Scope name(physical_fader::Stage::stage)
#define FADER_START(name) const uint32_t name=physical_fader::nowUs()
#define FADER_VALUE(name,value) const int name=value
#define FADER_STOP(name,stage) physical_fader::finish(physical_fader::Stage::stage,name)
#define FADER_TOUCH() physical_fader::captureTouch()
#define FADER_MAP(page,ch,value,before) physical_fader::mapped(physical_fader::Page::page,ch,value,before)
#define FADER_ACCEPT(ch,before,after) physical_fader::accepted(ch,before,after)
#define FADER_VISIBLE(page,value) physical_fader::visual(physical_fader::Page::page,value)
#else
#define FADER_SCOPE(name,stage)
#define FADER_START(name)
#define FADER_VALUE(name,value)
#define FADER_STOP(name,stage)
#define FADER_TOUCH()
#define FADER_MAP(page,ch,value,before)
#define FADER_ACCEPT(ch,before,after)
#define FADER_VISIBLE(page,value)
#endif
