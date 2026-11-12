# Optional automatic recording stop (113)

In the recording setup, open **More options → Auto-stop** and choose Off,
1 minute, 5 minutes or 10 minutes. Off is the default. The collapsed options
control shows an enabled limit so it is not hidden before starting a recording.
The normal Pause/Resume and Finish controls remain available.

The limit is copied into this GUI recording request. Selection, start delay and
encoder preparation do not consume it. Only an acknowledged Recording state
starts the allowance; time in an acknowledged Paused state is excluded. Changing
preferences later cannot alter an active recording's captured limit. Command-line
and D-Bus callers retain their existing behavior and do not silently inherit it.

The recording card retains its existing elapsed-time display and primary controls.
Its secondary description shows the remaining allowance, with a Paused indication
when appropriate. Finalization returns to the existing preparing-video message,
not a premature saved claim.

## Timing and safety boundary

This uses the platform's existing monotonic active-time clock, updated roughly
once per second. It is not a frame-accurate encoded-file duration or a real-time
security boundary. Event-loop delays, platform acknowledgement and encoder drain
can affect the final file duration. Review and trim the saved video as needed.
A pending pause takes effect for timing only when the backend acknowledges it.

The controller is independent of native capture and media-file ownership. A
successful start binds its generation and preparation token. At the limit it
queues a single stop; the actual Core checks that the same generation/token is
still recording before invoking the normal finish path. Cancel, failure, manual
stop, rendering and a newer recording intent invalidate pending work. Independent
screenshot requests and screenshot cancellation do not remove an active
recording limit. A callback
from an older recording cannot stop a new one.

No capture permissions, output-file ownership, pause implementation or Spectacle
capture modes are replaced. Native KDE/KWin/PipeWire, physical devices, A/V sync
and precise final media timing still require desktop acceptance.

## Verification and UI review

All 37 CTest targets passed (168.29 seconds), plus 38 identity/static checks.
Focused policy/setup tests cover disabled/invalid choices, acknowledgement,
pause/resume, once-only expiry, generation/token rejection, cancellation and
synchronous observer destruction/replacement. Actual software-window runs passed
in light, dark and compact layouts, with a controlled platform and virtual long
elapsed samples. They exercise real Core start/finish routes and actual settings,
Pause/Resume controls, retained preferences and finalizing feedback.

Recording and Paused each execute real Core delayed-screenshot request/cancel
routes before a grab can occur: the screenshot epoch changes, while the independent
recording epoch and remaining allowance stay intact. Old queued expiries cannot
stop a replacement recording. Default external calls stay Off despite enabled GUI
preferences. Existing controlled recording-pause/SNI and preparation UI workflows
also passed. These checks do not create or validate a native recording file.

Actual captures were reviewed for hierarchy and reachability: advanced selection
remains folded away, its enabled value stays visible, and the status card adds no
second large counter or extra action. The published status pictures are captured
before virtual elapsed-time injection, so elapsed and allowance are consistent.
They show a controlled software UI, not evidence of real device capture.

- [Options](screenshots/113-auto-stop-options.png)
- [Collapsed summary](screenshots/113-auto-stop-collapsed.png)
- [Light status](screenshots/113-auto-stop-light.png)
- [Dark status](screenshots/113-auto-stop-dark.png)
- [Compact status](screenshots/113-auto-stop-compact.png)
