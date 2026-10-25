"""Strict, version-specific source transformation; importable by host tests."""
from hashlib import sha256
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "patches/esp_dmx_v202"
BEFORE = {
    "esp_dmx.c": "9ddc50e211e768ff1c1612fc803e973071b22e6b18c60ebae9800cd288e497b4",
    "impl/driver.h": "5aa8ba4c1d361d2fff6ac5e58aff9fd5db3562f7d260f0f4549612e8288be626",
    "impl/intr_handlers.h": "f841719683ff5c8aaf24c07a650c6746feb5a5ef52a2dc7d78df187117adbd43",
}
READ_ONLY = {
    "impl/dmx_hal.h": "bb5d12b60c1aa121448f54868215570d9bd20c6f8db61e36d897f18ceb3fcf8b",
}
# Filled from the reviewed transformation; assets and results are validated together.
AFTER = {
    "esp_dmx.c": "18aa8e6ad3653a5c944cdeb5aaf1f5aba76c03531debfeba353d071f9d40d2c8",
    "impl/driver.h": "f6b70348340143a66bd8d91a23297191eba01cef7e5eef4987d0962b39e5ddcc",
    "impl/intr_handlers.h": "bf70e7f284012c5b346386402ecbf92b646f7a9eb93be9b36f1157b5de814441",
}
COPIES = {"dmx_tx_contract.h": "dmx_tx_contract.h",
          "impl/tx_core.h": "tx_core.h", "impl/tx_service.h": "tx_service.h"}
ASSET_SHA256 = {
    "dmx_tx_contract.h": "ec9bf545132c5be4bad63a0983476dfe06df688e6e813da023a8989c48630d61",
    "tx_api.inc": "23d8c9c404b4ddef24af970503545ed487965136715f076c10d2de5c25b903e4",
    "tx_core.h": "0172fb43efbf1020defd97b7adefd78c8931d9339c837b332752a598d7842c01",
    "tx_mode.inc": "717b060d84b514d2d065c0273345e794e6f9bbe277ba9b3ffff190d3cfe55caa",
    "tx_service.h": "9ab0ca6c877c0684c7398a6ef9a5687aa53094a67b2303d04d9558cb740d3030",
}


def digest(text):
    return sha256(text.encode("utf-8")).hexdigest()


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError("missing/duplicate esp_dmx patch anchor: " + old[:80])
    return text.replace(old, new, 1)


def asset(name):
    return (ASSETS / name).read_text(encoding="utf-8")


def transform(name, text):
    if digest(text) != BEFORE[name]:
        raise RuntimeError("unexpected upstream esp_dmx v2.02 source: " + name)
    if name == "impl/driver.h":
        text = replace_once(text, '#include "esp_dmx.h"',
                            '#include "esp_dmx.h"\n#define DMX_TX_INLINE static inline __attribute__((always_inline))\n#include "tx_core.h"')
        text = replace_once(text, '  dmx_port_t dmx_num;',
                            '  dmx_tx_state_t tx;\n  TaskHandle_t tx_owner;\n  dmx_port_t dmx_num;')
    elif name == "esp_dmx.c":
        text = replace_once(text, '#include "impl/intr_handlers.h"',
                            '#include "impl/intr_handlers.h"\n#include "dmx_tx_contract.h"')
        text = replace_once(text, '    p_dmx_obj[dmx_num]->dmx_num = dmx_num;',
                            '    p_dmx_obj[dmx_num]->dmx_num = dmx_num;\n    p_dmx_obj[dmx_num]->tx_owner = xTaskGetCurrentTaskHandle();')
        text = replace_once(text, '  // free isr\n', '''  // Owner-only deletion; never free a referenced active snapshot.
  dmx_obj_t *o = p_dmx_obj[dmx_num];
  if (xPortInIsrContext() || o->tx_owner != xTaskGetCurrentTaskHandle()) return ESP_ERR_INVALID_STATE;
  portENTER_CRITICAL(&dmx_context[dmx_num].spinlock);
  bool owned = dmx_tx_owned(&o->tx) || !dmx_tx_idle(dmx_num);
  portEXIT_CRITICAL(&dmx_context[dmx_num].spinlock);
  if (owned) return ESP_ERR_INVALID_STATE;

  // free isr
''')
        start = text.index('esp_err_t dmx_set_mode(')
        end = text.index('esp_err_t dmx_get_mode(', start)
        text = text[:start] + asset("tx_mode.inc") + "\n" + text[end:]
        start = text.index('esp_err_t dmx_write_packet(')
        text = text[:start] + asset("tx_api.inc")
    else:
        # One common port lock covers TX and RX lifetime; remove nested old locks.
        text = text.replace('        DMX_ENTER_CRITICAL_ISR(&(dmx_context[dmx_num].spinlock));\n', '')
        text = text.replace('        DMX_EXIT_CRITICAL_ISR(&(dmx_context[dmx_num].spinlock));\n', '')
        text = text.replace('      DMX_ENTER_CRITICAL_ISR(&(dmx_context[dmx_num].spinlock));\n', '')
        text = text.replace('      DMX_EXIT_CRITICAL_ISR(&(dmx_context[dmx_num].spinlock));\n', '')
        text = replace_once(text, 'static void IRAM_ATTR dmx_intr_handler',
                            '#include "tx_service.h"\n\nstatic void IRAM_ATTR dmx_intr_handler')
        text = replace_once(text, '    const uint32_t uart_intr_status =',
                            '    DMX_ENTER_CRITICAL_ISR(&(dmx_context[dmx_num].spinlock));\n    const uint32_t uart_intr_status =')
        text = replace_once(text, '    if (uart_intr_status == 0) break;', '''    if (uart_intr_status == 0) {
      DMX_EXIT_CRITICAL_ISR(&(dmx_context[dmx_num].spinlock));
      break;
    }''')
        start = text.index('    if (uart_intr_status & UART_INTR_TXFIFO_EMPTY)')
        end = text.index('    // DMX Receive', start)
        text = text[:start] + '''    if (uart_intr_status & DMX_INTR_TX_ALL) {
      if (p_dmx->mode != DMX_MODE_WRITE) {
        dmx_hal_disable_intr_mask(&dmx_context[dmx_num].hal, DMX_INTR_TX_ALL);
        dmx_hal_clr_intsts_mask(&dmx_context[dmx_num].hal, DMX_INTR_TX_ALL);
      } else {
        /* Plain DMX: a delivered/stale RX/TX compare flag is not TX failure.
           It never qualifies completion; progress/raw DONE/idle still do that. */
        if (uart_intr_status & UART_INTR_RS485_CLASH)
          dmx_hal_clr_intsts_mask(&dmx_context[dmx_num].hal, UART_INTR_RS485_CLASH);
        if (uart_intr_status & UART_INTR_TXFIFO_EMPTY)
          dmx_tx_fill_locked(p_dmx, now);
        dmx_tx_poll_locked(p_dmx, now);
      }
    }

''' + text[end:]
        text = replace_once(text, '    else if (uart_intr_status & DMX_INTR_RX_ALL) {',
                            '    else if (p_dmx->mode == DMX_MODE_READ && (uart_intr_status & DMX_INTR_RX_ALL)) {')
        text = replace_once(text, '  }\n  \n  if (task_awoken',
                            '    DMX_EXIT_CRITICAL_ISR(&(dmx_context[dmx_num].spinlock));\n  }\n  \n  if (task_awoken')
    return text


def apply(src):
    src = Path(src)
    if not src.is_dir():
        raise RuntimeError("esp_dmx v2.02 source missing after dependency installation: " + str(src))
    for name, expected in ASSET_SHA256.items():
        if digest(asset(name)) != expected:
            raise RuntimeError("unreviewed repository contract template: " + name)
    try:
        version = json.loads((src.parent / "library.json").read_text(encoding="utf-8"))["version"]
        if version != "2.0.2":
            raise RuntimeError("expected esp_dmx metadata 2.0.2, found " + str(version))
        for name, expected in READ_ONLY.items():
            if digest((src / name).read_text(encoding="utf-8")) != expected:
                raise RuntimeError("unexpected esp_dmx FIFO/HAL source: " + name)
        texts = {n: (src / n).read_text(encoding="utf-8") for n in BEFORE}
    except (FileNotFoundError, KeyError, ValueError) as error:
        raise RuntimeError("missing/invalid esp_dmx v2.02 dependency source/manifest: " + str(error)) from error
    states = ["before" if digest(t) == BEFORE[n] else
              "after" if digest(t) == AFTER.get(n) else "unknown" for n, t in texts.items()]
    if all(s == "after" for s in states):
        for target, source in COPIES.items():
            if not (src / target).exists() or (src / target).read_text(encoding="utf-8") != asset(source):
                raise RuntimeError("mixed/modified contract asset: " + target)
        print("esp_dmx TX ownership contract: already applied")
        return
    if not all(s == "before" for s in states) or any((src / n).exists() for n in COPIES):
        raise RuntimeError("mixed/unknown esp_dmx TX patch state: " + repr(dict(zip(BEFORE, states))))
    outputs = {n: transform(n, t) for n, t in texts.items()}
    for n, t in outputs.items():
        if digest(t) != AFTER.get(n):
            raise RuntimeError("repository patch/template hash mismatch: " + n)
    outputs.update({n: asset(source) for n, source in COPIES.items()})
    # Validate the entire set before writing. A failed write leaves a detectable mixed set.
    for n, t in outputs.items():
        with (src / n).open("w", encoding="utf-8", newline="\n") as output:
            output.write(t)
    print("esp_dmx TX ownership contract: applied (buffer 1 snapshot + qualified completion)")


if __name__ == "__main__":
    import sys
    for name in BEFORE:
        text = (Path(sys.argv[1]) / name).read_text(encoding="utf-8")
        print(name, digest(transform(name, text)))
