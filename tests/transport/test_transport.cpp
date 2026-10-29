// Compiles the installed patched ISR and the exact repository task-side APIs.
#include "esp_dmx.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "impl/intr_handlers.h"
#pragma GCC diagnostic pop
#include "dmx_tx_contract.h"
#include "../../patches/esp_dmx_v202/tx_api.inc"
#include "../../patches/esp_dmx_v202/tx_mode.inc"
#include "dmx_transport_esp_dmx.h"
#include "command_dispatcher.h"
#include "maintenance_transport.h"
#include "../support/fake_preset_storage.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)
FakeUart hw;
bool fake_isr_context = false;
int fake_task = 1;
dmx_context_t dmx_context[DMX_NUM_MAX];
dmx_obj_t* p_dmx_obj[DMX_NUM_MAX]{};

struct Fixture {
    uint8_t staging[513]{}, active[513]{};
    dmx_obj_t object;
    Fixture() {
        hw = FakeUart{};
        dmx_context[1].hal.dev = &hw;
        object.buffer[0] = staging;
        object.buffer[1] = active;
        p_dmx_obj[1] = &object;
    }
    void irq() { dmx_intr_handler(&object); }
    void drain(unsigned n, bool service = true) {
        while (n-- && !hw.fifo.empty()) {
            hw.wire.push_back(hw.fifo.front());
            hw.fifo.pop_front();
            hw.now += 44;
            if (hw.fifo.empty()) {
                hw.int_raw.val |= UART_INTR_TX_DONE;
                hw.idle = true;
            }
            if (service) irq();
        }
    }
    void finish() {
        for (int i = 0; i < 1024 && object.tx.phase != DMX_TX_READY; ++i) {
            drain(1);
            dmx_tx_status_t status;
            CHECK(dmx_tx_poll(1, &status) == ESP_OK);
        }
        CHECK(object.tx.phase == DMX_TX_READY);
    }
    void start(uint8_t value) {
        CHECK(dmx_set_mode(1, DMX_MODE_WRITE) == ESP_OK);
        memset(staging + 1, value, 512);
        uint32_t attempt = 0;
        CHECK(dmx_tx_start(1, 513, &attempt) == ESP_OK && attempt != 0);
    }
};

static void normalAndSnapshot() {
    Fixture f;
    f.start(17);
    CHECK(f.object.tx.queued == 128);
    CHECK(f.object.tx.first_fifo_us == 188);
    CHECK(hw.now == 188); // Preserved first-frame busy-wait exception.
    const auto attempt = f.object.tx.attempt;
    CHECK(dmx_tx_start(1, 513, nullptr) != ESP_OK);
    CHECK(dmx_set_mode(1, DMX_MODE_READ) != ESP_OK);
    uint8_t new_packet[513]{};
    memset(new_packet + 1, 219, 512);
    CHECK(dmx_write_packet(1, new_packet, 513) == ESP_OK);
    for (int i = 1; i < 513; ++i) CHECK(f.active[i] == 17);
    f.finish();
    CHECK(f.object.tx.result == DMX_TX_COMPLETE && f.object.tx.attempt == attempt);
    CHECK(hw.wire.size() == 513 && hw.wire[0] == 0);
    for (int i = 1; i < 513; ++i) CHECK(hw.wire[i] == 17);
    hw.wire.clear();
    CHECK(dmx_tx_start(1, 513, nullptr) == ESP_OK);
    f.finish();
    CHECK(hw.wire[512] == 219);
    fake_task = 2;
    CHECK(dmx_tx_start(1, 513, nullptr) != ESP_OK);
    CHECK(dmx_write_packet(1, new_packet, 513) != ESP_OK);
    fake_task = 1;
    fake_isr_context = true;
    CHECK(dmx_tx_cancel(1) != ESP_OK);
    CHECK(dmx_write_packet(1, new_packet, 513) != ESP_OK);
    fake_isr_context = false;
    puts("PASS normal 513 slots, immutable snapshot, busy admission, next staged generation");
}

static void underrunAndPrematureDone() {
    // Exercise every software-tail length, including the last remaining slot.
    for (unsigned queued = 128; queued < 513; ++queued) {
        Fixture f;
        f.start(11);
        f.object.tx.queued = queued;
        hw.int_raw.val |= UART_INTR_RS485_CLASH;
        f.drain(128, false);
        const unsigned reads = hw.snapshot_reads;
        f.irq();
        CHECK(f.object.tx.result == DMX_TX_UNDERRUN);
        CHECK(f.object.tx.phase == DMX_TX_READY);
        CHECK(hw.snapshot_reads == reads); // Raw done checked BEFORE refill.
        CHECK(f.object.tx.queued == queued);
    }
    Fixture f;
    f.start(33);
    // Make the next refill the final slot and inject done during that FIFO write.
    f.object.tx.queued = 512;
    f.drain(121, false);
    hw.inject_done_in_write = true;
    hw.inject_clash_in_write = true;
    f.irq();
    CHECK(f.object.tx.queued == 513 && f.object.tx.result == DMX_TX_UNDERRUN);
    CHECK(f.object.tx.phase == DMX_TX_RETIRING);
    CHECK(dmx_tx_start(1, 513, nullptr) != ESP_OK);
    f.drain(128, false);
    dmx_tx_status_t s;
    CHECK(dmx_tx_poll(1, &s) == ESP_OK && s.can_start);
    CHECK(s.tx.result == DMX_TX_UNDERRUN);
    const int64_t before = hw.now;
    CHECK(dmx_tx_start(1, 513, nullptr) == ESP_OK);
    CHECK(hw.now - before == 188); // Fresh recovery framing.
    f.finish();
    CHECK(f.object.tx.result == DMX_TX_COMPLETE);
    puts("PASS 385 deliberate underrun tails, premature/final-refill done, quiescent recovery");
}

static void timeoutsAndFailedRetirement() {
    Fixture f;
    f.start(29);
    hw.int_raw.val |= UART_INTR_RS485_CLASH;
    hw.now += 4 * f.object.tx.frame_us;
    dmx_tx_status_t s;
    CHECK(dmx_tx_poll(1, &s) == ESP_OK);
    CHECK(s.tx.phase == DMX_TX_RETIRING && s.tx.result == DMX_TX_TIMEOUT);
    const auto old_attempt = s.tx.attempt;
    CHECK(dmx_tx_start(1, 513, nullptr) != ESP_OK);
    CHECK(dmx_set_mode(1, DMX_MODE_READ) != ESP_OK);
    f.drain(128, false);
    CHECK(dmx_tx_poll(1, &s) == ESP_OK && s.can_start && s.tx.result == DMX_TX_TIMEOUT);
    CHECK(s.tx.attempt == old_attempt);
    CHECK(dmx_tx_start(1, 513, nullptr) == ESP_OK);
    f.finish();
    CHECK(f.object.tx.result == DMX_TX_COMPLETE);

    Fixture stuck;
    stuck.start(71);
    CHECK(dmx_tx_cancel(1) == ESP_OK);
    hw.now += 2 * stuck.object.tx.frame_us;
    CHECK(dmx_tx_poll(1, &s) == ESP_OK && s.tx.phase == DMX_TX_FAULT_STOPPED);
    for (int i = 1; i < 513; ++i) CHECK(stuck.active[i] == 71);
    CHECK(dmx_tx_start(1, 513, nullptr) != ESP_OK);
    CHECK(dmx_set_mode(1, DMX_MODE_READ) != ESP_OK);
    stuck.drain(128, false);
    CHECK(dmx_tx_poll(1, &s) == ESP_OK && !s.can_start); // Late idle is not auto-reset.
    puts("PASS active timeout, safe retry boundary, stuck retirement, no ambiguous buffer reuse");
}

static void drainQualificationAndEpochs() {
    Fixture f;
    f.start(23);
    while (f.object.tx.phase == DMX_TX_FEEDING) f.drain(1);
    CHECK(f.object.tx.queued == 513);
    // A completion hint with shifter/FIFO still busy is not COMPLETE.
    hw.int_raw.val |= UART_INTR_TX_DONE;
    f.irq();
    CHECK(f.object.tx.phase == DMX_TX_DRAINING && f.object.tx.done_seen);
    CHECK(dmx_tx_start(1, 513, nullptr) != ESP_OK);
    f.drain(128, false);
    hw.idle = false; // FIFO empty but shifter/FSM not retired.
    dmx_tx_status_t s;
    CHECK(dmx_tx_poll(1, &s) == ESP_OK && !s.can_start);
    hw.idle = true;
    CHECK(dmx_tx_poll(1, &s) == ESP_OK && s.tx.result == DMX_TX_COMPLETE);
    // Old raw flags are cleared while quiescent, before the next epoch.
    hw.int_raw.val |= UART_INTR_TX_DONE;
    CHECK(dmx_tx_start(1, 513, nullptr) == ESP_OK);
    CHECK(!f.object.tx.done_seen && f.object.tx.result == DMX_TX_PENDING);
    f.finish();
    const auto reads = hw.snapshot_reads;
    CHECK(dmx_set_mode(1, DMX_MODE_READ) == ESP_OK);
    hw.int_raw.val |= UART_INTR_TXFIFO_EMPTY | UART_INTR_TX_DONE;
    f.irq();
    CHECK(hw.snapshot_reads == reads);
    CHECK(dmx_tx_start(1, 513, nullptr) != ESP_OK);
    {
        Fixture arming;
        arming.start(82);
        hw.inject_done_on_enable = true;
        while (arming.object.tx.phase == DMX_TX_FEEDING) arming.drain(1);
        CHECK(arming.object.tx.phase == DMX_TX_DRAINING && arming.object.tx.done_seen);
        arming.finish();
        CHECK(arming.object.tx.result == DMX_TX_COMPLETE && hw.wire.size() == 513);
    }
    {
        Fixture zero_write;
        CHECK(dmx_set_mode(1, DMX_MODE_WRITE) == ESP_OK);
        hw.fail_fifo_write = true;
        CHECK(dmx_tx_start(1, 513, nullptr) == ESP_OK);
        dmx_tx_status_t status;
        CHECK(dmx_tx_poll(1, &status) == ESP_OK);
        CHECK(status.tx.result == DMX_TX_START_FAILED && status.can_start);
        CHECK(hw.wire.empty());
    }
    puts("PASS FIFO/shifter drain gate, stale event clearing, Receiver isolation");
}

using Command = ControllerCommand;
using Type = CommandType;
static Command setValue(int value) { return Command::channelCommand(Type::set_channel, 21, value); }
struct Integration {
    Fixture f;
    EspDmxTransportDriver driver{1};
    DmxTransport transport{driver};
    FakeStorage storage;
    DmxController controller{storage};
    CommandDispatcher dispatcher{controller};
    Integration() {
        transport.requestSender(true);
        transport.poll(hw.now);
        dispatcher.setSenderActive(true);
        dispatcher.setPublisher(publish, this);
        dispatcher.setAdmission(admit, this);
        CHECK(transport.stage(controller.outputPacket(), 513));
    }
    static int publish(void* context, const Command& c) {
        auto& h = *static_cast<Integration*>(context);
        return h.transport.stage(h.controller.outputPacket(), 513, c.type == Type::set_blackout) ? 0 : -1;
    }
    static bool admit(void* context, const Command& c) {
        auto& h = *static_cast<Integration*>(context);
        const bool edge = c.type == Type::set_blackout &&
            (c.payload.blackout != 0) != h.controller.blackoutActive();
        return h.transport.canStage(edge);
    }
    void command(const Command& c) {
        const auto result = dispatcher.executeLocal(c);
        CHECK(result.status == CommandStatus::applied || result.status == CommandStatus::no_change);
    }
    void frame() {
        CHECK(transport.serviceTx(hw.now));
        f.finish();
        transport.poll(hw.now);
    }
};

static void latestAndPresets() {
    Integration h;
    h.command(setValue(10));
    CHECK(h.transport.serviceTx(hw.now));
    for (int value = 11; value <= 255; ++value) h.command(setValue(value));
    h.f.finish();
    h.transport.poll(hw.now);
    CHECK(hw.wire[21] == 10);
    h.frame();
    CHECK(hw.wire[513 + 21] == 255);
    CHECK(h.transport.health().completed == h.transport.health().requested);
    // Continue retransmitting without a new command.
    h.frame();
    CHECK(hw.wire[1026 + 21] == 255);
    h.command(Command::presetCommand(Type::save_preset, 0));
    h.command(setValue(77));
    CHECK(h.transport.serviceTx(hw.now));
    h.command(Command::presetCommand(Type::load_preset, 0));
    h.f.finish();
    h.transport.poll(hw.now);
    h.frame();
    CHECK(hw.wire[4 * 513 + 21] == 255);
    CHECK(h.controller.channel(21) == 255);
    puts("PASS live updates, latest/final value, continuous output, preset during active TX");
}

static void blackoutOrderingAndAdmission() {
    Integration h;
    h.command(setValue(123));
    CHECK(h.transport.serviceTx(hw.now));
    h.command(Command::blackoutCommand(true));
    h.command(setValue(211)); // Discarded by existing Controller release semantics.
    h.command(Command::blackoutCommand(false));
    h.command(setValue(124));
    CHECK(h.transport.pendingBarriers() == 2);
    h.f.finish();
    h.transport.poll(hw.now);
    h.frame(); // Required zero barrier, never sampled away.
    for (int i = 513; i < 1026; ++i) CHECK(hw.wire[i] == 0);
    h.frame();
    CHECK(hw.wire[1026 + 21] == 123); // OFF snapshot immutable, not future latest=124.
    h.frame();
    CHECK(hw.wire[1539 + 21] == 124 && h.controller.channel(21) == 124);

    for (unsigned i = 0; i < DmxTransport::barrier_capacity; ++i)
        h.command(Command::blackoutCommand((i & 1) == 0));
    const auto revision = h.controller.revision();
    CHECK(h.dispatcher.executeLocal(Command::blackoutCommand(true)).status == CommandStatus::busy);
    CHECK(h.controller.revision() == revision && !h.controller.blackoutActive());
    CHECK(h.dispatcher.submit(Command::blackoutCommand(true)).status == CommandStatus::accepted);
    CHECK(h.dispatcher.submit(setValue(99)).status == CommandStatus::accepted);
    CHECK(h.dispatcher.process() == 0 && h.dispatcher.pending() == 2);
    h.frame();
    CHECK(h.dispatcher.process() == 2 && h.controller.blackoutActive());
    CHECK(h.transport.pendingBarriers() == DmxTransport::barrier_capacity);
    puts("PASS rapid ordered blackout ON/OFF, exact restore, pre-mutation BUSY/FIFO backpressure");
}

static void retryModesAndShutdown() {
    {
        Integration recovery;
        recovery.command(setValue(91));
        recovery.command(Command::blackoutCommand(true));
        recovery.command(Command::blackoutCommand(false));
        CHECK(recovery.transport.serviceTx(hw.now));
        recovery.f.drain(128, false);
        recovery.f.irq();
        recovery.transport.poll(hw.now);
        CHECK(recovery.transport.pendingBarriers() == 2);
        CHECK(recovery.transport.health().completed == 0);
        hw.now += DmxTransport::nominal_frame_us;
        recovery.frame();
        CHECK(recovery.transport.pendingBarriers() == 1);
        for (size_t i = 128; i < 128 + 513; ++i) CHECK(hw.wire[i] == 0);
        recovery.frame();
        CHECK(hw.wire[128 + 513 + 21] == 91);
        CHECK(!recovery.transport.health().fault_stopped);
    }
    Integration h;
    h.command(setValue(81));
    h.command(Command::blackoutCommand(true));
    h.command(Command::blackoutCommand(false));
    for (unsigned fail = 1; fail <= DmxTransport::max_failed_attempts; ++fail) {
        CHECK(h.transport.serviceTx(hw.now));
        h.f.drain(128, false);
        h.f.irq();
        h.transport.poll(hw.now);
        CHECK(h.transport.health().errors == fail && h.transport.pendingBarriers() == 2);
        CHECK(!h.transport.serviceTx(hw.now));
        hw.now += DmxTransport::nominal_frame_us;
    }
    CHECK(h.transport.health().fault_stopped);
    const auto reads = hw.snapshot_reads;
    for (int i = 0; i < 100; ++i) { hw.now += 1000000; CHECK(!h.transport.serviceTx(hw.now)); }
    CHECK(hw.snapshot_reads == reads); // No infinite automatic retries.

    Integration modes;
    modes.command(setValue(54));
    CHECK(modes.transport.serviceTx(hw.now));
    modes.transport.requestSender(false);
    modes.transport.poll(hw.now);
    CHECK(!modes.transport.receiverReady());
    modes.f.drain(128, false);
    modes.transport.poll(hw.now);
    CHECK(modes.transport.receiverReady());
    CHECK(!modes.transport.serviceTx(hw.now));

    Integration shutdown;
    shutdown.command(setValue(70));
    CHECK(shutdown.transport.serviceTx(hw.now));
    uint8_t zeros[513]{};
    CHECK(shutdown.transport.stage(zeros, 513));
    CHECK(dmx_wait_send_done(1, 0) == ESP_ERR_TIMEOUT);
    shutdown.f.finish();
    shutdown.transport.poll(hw.now);
    CHECK(hw.wire.size() == 513 && hw.wire[21] == 70);
    CHECK(shutdown.transport.health().completed != shutdown.transport.health().requested);
    puts("PASS bounded retry exhaustion, barrier retention, mode retirement, legacy shutdown limitation");
}


static void finishWithClashes(Fixture& f) {
    uint8_t frozen[513];memcpy(frozen,f.active,513);
    for(int i=0;i<1024 && f.object.tx.phase!=DMX_TX_READY;++i) {
        // Model repeated compare indications, including forced delivery from
        // stale/unexpected enables and simultaneous refill/final-DONE delivery.
        hw.int_raw.val|=UART_INTR_RS485_CLASH;
        if(i%17==0) hw.enabled|=UART_INTR_RS485_CLASH;
        f.drain(1);
        dmx_tx_status_t status;
        CHECK(dmx_tx_poll(1,&status)==ESP_OK);
        CHECK(!(hw.enabled & UART_INTR_RS485_CLASH));
        CHECK(status.tx.queued<=513 && memcmp(frozen,f.active,513)==0);
        CHECK(status.tx.result==DMX_TX_PENDING || status.tx.result==DMX_TX_COMPLETE);
    }
    CHECK(f.object.tx.phase==DMX_TX_READY && f.object.tx.result==DMX_TX_COMPLETE);
    CHECK(f.object.tx.queued==513 && f.object.tx.done_seen);
}
static void clashInitialFillAndNoFalseCompletion() {
    Fixture f;
    hw.inject_clash_in_write=true;
    f.start(83);
    CHECK(f.object.tx.phase==DMX_TX_FEEDING && f.object.tx.queued==128);
    CHECK(f.object.tx.result==DMX_TX_PENDING && (hw.int_raw.val & UART_INTR_RS485_CLASH));
    CHECK(!(hw.enabled & UART_INTR_RS485_CLASH));
    const auto reads=hw.snapshot_reads;
    for(int i=0;i<100;++i) {
        hw.enabled|=UART_INTR_RS485_CLASH; // Exercise the defensive ACK branch.
        hw.int_raw.val|=UART_INTR_RS485_CLASH;
        f.irq();
        CHECK(f.object.tx.phase==DMX_TX_FEEDING && f.object.tx.result==DMX_TX_PENDING);
        CHECK(!(hw.int_raw.val & UART_INTR_RS485_CLASH));
        CHECK(!(hw.enabled & UART_INTR_RS485_CLASH));
    }
    CHECK(hw.snapshot_reads==reads && hw.wire.empty());
    CHECK(dmx_wait_send_done(1,0)==ESP_ERR_TIMEOUT);
    CHECK(dmx_tx_start(1,513,nullptr)!=ESP_OK);
    finishWithClashes(f);
    CHECK(hw.wire.size()==513 && hw.wire[0]==0);
    for(size_t i=1;i<513;++i) CHECK(hw.wire[i]==83);
    puts("PASS CLASH during first FIFO fill/repeated delivery: no false fault, refill, completion or snapshot mutation");
}
static void clashNoReceiverAndBlackout() {
    Integration h;
    h.command(setValue(37));
    for(int frame=0;frame<100;++frame) {
        hw.inject_clash_in_write=true;
        CHECK(h.transport.serviceTx(hw.now));
        if(frame==0) for(int value=38;value<=49;++value) h.command(setValue(value));
        finishWithClashes(h.f);
        h.transport.poll(hw.now);
        const size_t offset=frame*513;
        CHECK(hw.wire[offset]==0);
        for(size_t ch=1;ch<513;++ch)
            CHECK(hw.wire[offset+ch]==(ch==21 ? (frame==0 ? 37 : 49) : 0));
    }
    CHECK(!h.transport.health().errors && !h.transport.health().fault_stopped);
    CHECK(h.transport.health().completed==h.transport.health().requested);
    h.command(Command::blackoutCommand(true));
    h.command(Command::blackoutCommand(false));
    CHECK(h.transport.pendingBarriers()==2);
    for(int edge=0;edge<2;++edge) {
        CHECK(h.transport.serviceTx(hw.now));
        finishWithClashes(h.f);h.transport.poll(hw.now);
        const size_t offset=(100+edge)*513;
        CHECK(hw.wire[offset]==0);
        for(size_t ch=1;ch<513;++ch)
            CHECK(hw.wire[offset+ch]==(edge==1 && ch==21 ? 49 : 0));
        CHECK(h.transport.pendingBarriers()==size_t(1-edge));
    }
    CHECK(!h.transport.health().errors && !h.transport.health().fault_stopped);
    puts("PASS no-receiver startup/reconnect model: 102 coherent full frames under repeated CLASH, final value and ordered blackout ON/OFF");
}
static void clashReceiverIsolation() {
    Fixture f;
    f.start(58);f.finish();
    CHECK(dmx_set_mode(1,DMX_MODE_READ)==ESP_OK);
    const auto before=f.object.tx;
    const auto reads=hw.snapshot_reads;
    uint8_t frozen[513];memcpy(frozen,f.active,513);
    hw.enabled|=UART_INTR_RS485_CLASH|UART_INTR_TX_DONE;
    hw.int_raw.val|=UART_INTR_RS485_CLASH|UART_INTR_TX_DONE|UART_INTR_BRK_DET;
    f.irq();
    CHECK(f.object.mode==DMX_MODE_READ && memcmp(&before,&f.object.tx,sizeof(before))==0);
    CHECK(hw.snapshot_reads==reads && memcmp(frozen,f.active,513)==0);
    CHECK((hw.enabled & DMX_INTR_RX_ALL)==DMX_INTR_RX_ALL);
    CHECK(!(hw.enabled & DMX_INTR_TX_ALL));
    CHECK(!(hw.int_raw.val & (UART_INTR_RS485_CLASH|UART_INTR_TX_DONE|UART_INTR_BRK_DET)));
    CHECK(f.object.rx_last_brk_ts==hw.now);
    CHECK(dmx_tx_start(1,513,nullptr)!=ESP_OK);
    puts("PASS Receiver mode: stale TX/CLASH discarded, RX break still handled, no TX/snapshot ownership mutation");
}



// No RX input, receiver presence or ACK exists in this HAL model.
// Hardware confirmed CLASH; actual incomplete sends must still fail.
static void auditNoReceiver() {
    Integration h;
    h.command(setValue(37));
    for (int i = 0; i < 100; ++i) h.frame();
    CHECK(hw.wire.size() == 100 * 513);
    CHECK(h.transport.health().errors == 0);
    CHECK(!h.transport.health().fault_stopped);
    printf("AUDIT no receiver input: 100 complete frames, errors=0, no latch\n");
}
static void auditIncompleteLatchAndReconnect() {
    Integration h;
    h.command(setValue(91));
    h.command(Command::blackoutCommand(true));
    h.command(Command::blackoutCommand(false));
    for (unsigned i=1; i<=3; ++i) {
        CHECK(h.transport.serviceTx(hw.now));
        uint8_t frozen[513]; memcpy(frozen,h.f.active,513);
        hw.int_raw.val |= UART_INTR_RS485_CLASH;
        h.f.drain(128,false); // Actual FIFO underrun, in addition to masked CLASH.
        // Hold the shifter busy to prove failed storage cannot be reused yet.
        hw.idle=false;
        h.f.irq();
        CHECK(h.f.object.tx.result == DMX_TX_UNDERRUN);
        CHECK(h.f.object.tx.phase == DMX_TX_RETIRING);
        CHECK(dmx_tx_owned(&h.f.object.tx));
        CHECK(!h.transport.serviceTx(hw.now));
        CHECK(memcmp(frozen,h.f.active,513)==0);
        hw.idle=true;
        h.transport.poll(hw.now);
        CHECK(h.f.object.tx.phase == DMX_TX_READY);
        CHECK(h.transport.health().errors == i);
        CHECK(h.transport.health().consecutive_failures == i);
        CHECK(h.transport.pendingBarriers() == 2);
        CHECK(h.transport.health().completed == 0);
        printf("AUDIT actual incomplete frame: t=%lld attempt=%u queued=%u result=%u driver_phase=%u failures=%u stopped=%u\n",
            (long long)hw.now,h.f.object.tx.attempt,h.f.object.tx.queued,
            unsigned(h.f.object.tx.result),unsigned(h.f.object.tx.phase),
            h.transport.health().consecutive_failures,unsigned(h.transport.health().fault_stopped));
        hw.now += DmxTransport::nominal_frame_us;
    }
    dmx_tx_status_t status;
    CHECK(dmx_tx_poll(1,&status)==ESP_OK && status.can_start);
    CHECK(!dmx_tx_owned(&status.tx));
    const auto reads=hw.snapshot_reads;
    hw.int_raw.val=0; hw.idle=true; // Model disturbance removed / cable reconnected.
    for (int i=0;i<100;++i) {hw.now+=100000; CHECK(!h.transport.serviceTx(hw.now));}
    CHECK(hw.snapshot_reads==reads && h.transport.pendingBarriers()==2);
    puts("AUDIT disturbance removed: driver quiescent/available, application stays latched, barriers retained");
}
static void auditSingleIncompleteRecovery() {
    Integration h;
    h.command(Command::blackoutCommand(true));
    h.command(Command::blackoutCommand(false));
    CHECK(h.transport.serviceTx(hw.now));
    hw.int_raw.val|=UART_INTR_RS485_CLASH;
    h.f.drain(128,false); h.f.irq(); h.transport.poll(hw.now);
    CHECK(!h.transport.health().fault_stopped);
    hw.now+=DmxTransport::nominal_frame_us;
    h.frame();
    CHECK(h.transport.pendingBarriers()==1);
    CHECK(h.transport.health().consecutive_failures==0);
    h.frame(); CHECK(h.transport.pendingBarriers()==0);
    puts("AUDIT one safely retired incomplete frame: retry succeeds, consecutive count resets, ON/OFF order retained");
}
static void auditLateOwnerPoll() {
    Integration h;
    CHECK(h.transport.serviceTx(hw.now));
    while(h.f.object.tx.phase==DMX_TX_FEEDING) h.f.drain(1);
    h.f.drain(128,false);
    CHECK(hw.wire.size()==513 && hw.idle);
    hw.now+=10*DmxTransport::nominal_frame_us;
    h.transport.poll(hw.now);
    CHECK(h.f.object.tx.result==DMX_TX_COMPLETE);
    CHECK(h.transport.health().errors==0);
    puts("AUDIT late owner poll >91.04ms after fully drained frame: COMPLETE wins, no false timeout");
}




static void maintenanceZeroAndRetirement() {
    Integration h;h.command(setValue(91));
    CHECK(h.transport.serviceTx(hw.now)); // An immutable nonzero packet is still active.
    h.command(Command::blackoutCommand(true));h.command(Command::blackoutCommand(false));
    const auto revision=h.controller.revision();
    MaintenanceTransport shutdown(h.transport);shutdown.begin();
    unsigned completed=0;h.f.finish();h.transport.poll(hw.now);
    const auto before=hw.wire.size();
    for(unsigned turn=0;turn<10000&&completed<3;++turn){
        const auto result=shutdown.zeroFrame(hw.now);CHECK(result!=DeviceMaintenance::Step::failed);
        if(result==DeviceMaintenance::Step::ready)++completed;
        // Deliberate clash while queued=128 remains nonfatal; actual full drain qualifies completion.
        if(h.f.object.tx.phase==DMX_TX_FEEDING){hw.int_raw.val|=UART_INTR_RS485_CLASH;h.f.irq();h.f.finish();}
        h.transport.poll(hw.now);
    }
    CHECK(completed==3&&h.transport.pendingBarriers()==0);
    CHECK(hw.wire.size()==before+5*513); // Ordered blackout ON, restore, three zero frames.
    CHECK(hw.wire[before+513+21]==91);
    for(size_t i=before+2*513;i<hw.wire.size();++i)CHECK(hw.wire[i]==0);
    CHECK(shutdown.quiesce(hw.now)==DeviceMaintenance::Step::ready);
    CHECK(h.transport.receiverReady()&&!dmx_tx_owned(&h.f.object.tx));
    CHECK(h.controller.revision()==revision&&h.controller.channel(21)==91);
    CHECK(h.storage.saves==0&&h.storage.metadata_writes==0);
    puts("PASS maintenance adapter: active immutable packet, ordered blackout barriers, three real ISR-qualified 513-byte zeros, clash isolation and safe retirement; Controller/NVS unchanged");
}

int main() {
    maintenanceZeroAndRetirement();
    clashInitialFillAndNoFalseCompletion();
    clashNoReceiverAndBlackout();
    clashReceiverIsolation();
    auditNoReceiver();
    auditIncompleteLatchAndReconnect();
    auditSingleIncompleteRecovery();
    auditLateOwnerPoll();
    (void)&dmx_timing_intr_handler;
    normalAndSnapshot();
    underrunAndPrematureDone();
    timeoutsAndFailedRetirement();
    drainQualificationAndEpochs();
    latestAndPresets();
    blackoutOrderingAndAdmission();
    retryModesAndShutdown();
    printf("Host sizes: transport=%zu driver-contract=%zu bytes\n", sizeof(DmxTransport), sizeof(dmx_tx_state_t));
    puts("All transport tests passed. Electrical/timing correctness still requires hardware capture.");
}
