# Avendish TouchDesigner sweep driver.
#
# One-time setup (see README.md): paste this into an Execute DAT's callbacks,
# enable the DAT's "Frame Start" toggle, and Save As a driver .toe (e.g.
# tooling/td/td_sweep.toe). run_td_sweep.py then launches that .toe headlessly.
#
# It waits a couple of frames (so every Custom OP plugin has registered), runs
# the generated sweep runner once, writes the report, and quits. The runner and
# report locations come from environment variables set by run_td_sweep.py:
#   AVND_TD_RUNNER      path to the generated td_run_all.py (instantiates+cooks
#                       every operator, writes td_test_report.json)
#   AVND_TD_REPORT_DIR  where to write td_sweep_error.txt on failure
#                       (defaults to the project folder)

import os
import traceback

_ran = False


def onFrameStart(frame):
    global _ran
    # Frame 0/1 can run before all plugins are registered; wait a beat.
    if _ran or frame < 3:
        return
    _ran = True

    runner = os.environ.get("AVND_TD_RUNNER", "")
    try:
        if not (runner and os.path.exists(runner)):
            raise RuntimeError("AVND_TD_RUNNER not set / not found: " + runner)
        # Run in this DAT's global scope so `op`, `project`, `td` are available.
        exec(open(runner, "r", encoding="utf-8").read())
    except Exception:
        report_dir = os.environ.get("AVND_TD_REPORT_DIR", project.folder)
        try:
            with open(os.path.join(report_dir, "td_sweep_error.txt"), "w") as f:
                f.write(traceback.format_exc())
        except Exception:
            pass
    project.quit(force=True)


# Unused Execute DAT callbacks (kept so TouchDesigner parses the file cleanly).
def onStart():
    return


def onCreate():
    return


def onExit():
    return


def onFrameEnd(frame):
    return


def onPlayStateChange(state):
    return
