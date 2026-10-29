#!/usr/bin/env python3
"""Regression check: the first GUI frame must precede Wi-Fi startup."""

from pathlib import Path
import re
import sys


def main() -> int:
    source = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
    frame = source.find("lv_refr_now(NULL);")
    init = source.find("IoT_Init(")
    loop_match = re.search(r"while\s*\(\s*1\s*\)", source[frame:])
    loop = frame + loop_match.start() if loop_match else -1
    process = source.find("IoT_Process();", loop)

    assert frame >= 0, "main must explicitly render the first frame"
    assert init >= 0, "IoT_Init must start the asynchronous Wi-Fi manager"
    assert frame < init, "the first LCD frame must be rendered before IoT/Wi-Fi startup"
    assert loop >= 0 and process > loop, "main loop must keep IoT_Process progressing"
    display_guard = re.search(r"if\s*\(\s*!\s*gui_disp_init\s*\(\s*\)\s*\)\s*\{", source)
    assert display_guard is not None, "main must handle LVGL display allocation failure"
    first_screen_use = source.find("lv_screen_active()")
    assert display_guard.start() < first_screen_use, "screen access must follow successful display initialization"
    failure_path = source[display_guard.start():first_screen_use]
    assert "LVGL display initialization failed" in failure_path
    assert "IoT_Init" not in failure_path, "Wi-Fi startup must be skipped when no display exists"
    assert re.search(r"while\s*\(\s*1\s*\)", failure_path), "failure path must remain responsive"
    assert re.search(r"if\s*\(\s*!\s*gui_touch_init\s*\(\s*\)\s*\)", source), "touch init failure must be reported"
    print("startup order: first frame -> IoT_Init -> IoT_Process")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
