# Startup performance baseline procedure

`STARTUP-PERF-101` measures the existing opt-in startup milestones without
changing startup behavior. The application emits them only when
`BIBLIA_ELIM_UI_LOAD_DEBUG=1` is exactly `1`; normal runs remain silent.

Build the current tree, then collect seven executions in each scenario:

```sh
cmake -S . -B build
cmake --build build --target biblia-elim -j2
python3 scripts/startup_performance_baseline.py collect \
  --app build/src/gtk/biblia-elim \
  --fixture tests/fixtures/sqlite/test-bible.sql \
  --sqlite3 /usr/bin/sqlite3 \
  --xvfb-run /usr/bin/xvfb-run \
  --runs 7 \
  --output build/startup-baseline-YYYYMMDD
```

Omit `--xvfb-run` on a real X11 display. The output directory must not already
exist. It retains every combined stdout/stderr trace plus `summary.json` and
`summary.md`. The JSON contains raw timestamps, derived phase durations,
median, median absolute deviation (MAD), minimum, maximum, and the phase with
the largest median. Existing traces can be reanalyzed with:

```sh
python3 scripts/startup_performance_baseline.py analyze \
  build/startup-baseline-YYYYMMDD
```

For the focused `GTK_INITIALIZED` to `WINDOW_CREATED` breakdown, collect with
the same command and analyze the retained logs with:

```sh
python3 scripts/startup_performance_baseline.py profile-window \
  build/startup-baseline-YYYYMMDD
```

This report partitions the whole interval into consecutive monotonic
boundaries for splash preparation, global HTML renderer initialization, theme
setup, window chrome/navigation, each startup content pane, showing and
realizing the widget tree, and draining pending GTK events. Component durations
therefore sum to the measured phase total (apart from displayed 0.1 ms
rounding). The events remain opt-in under `BIBLIA_ELIM_UI_LOAD_DEBUG=1` and
perform no startup work themselves.

To split the two final window hotspots further, analyze a collection made by
the current instrumented binary with:

```sh
python3 scripts/startup_performance_baseline.py profile-window-hotspots \
  build/startup-window-hotspots-YYYYMMDD
```

This adds boundaries around statusbar construction, saved layout restoration,
the synchronous `gtk_widget_show_all()` traversal, post-show visibility sync,
and final signal connection. During `sync_windows()` it records each actual
`gtk_main_iteration()` as a nested begin/end pair, plus window realize, map,
style, selected pane allocation, and named renderer surface lifecycle markers.
The report gives median/MAD/min/max for every consecutive operation and for
event-drain iteration count, summed iteration time, longest iteration, and
residual time outside iteration dispatch. These diagnostics use the same
monotonic clock and remain disabled unless `BIBLIA_ELIM_UI_LOAD_DEBUG=1`.

For event-drain attribution, that opt-in switch also attaches diagnostic-only
observers to the already constructed main-window tree before it is shown. Each
`GTK_EVENT_ITERATION_PROFILE` belongs to the numbered iteration immediately
before it and aggregates actual `realize`, `map`, `style-updated`, and
`size-allocate` signal deliveries. Allocation is split between renderer,
`GtkPaned`, notebook, and other widgets. Top-level frame draw time and nested
renderer draw time use the monotonic clock and are reported separately; they
overlap by design and must not be added together. The observers are not
connected and emit no logging unless diagnostics are enabled.

`profile-window-hotspots` validates complete iteration/profile pairing,
summarizes callback totals across samples, and records the three longest
iterations in every log with their nonzero lifecycle categories. These
correlations identify where narrower probes are needed; a signal count alone
does not establish that GTK's internal default handler is avoidable.

The extended attribution opens the former `style_other` and `allocate_other`
buckets without a closed widget-type list. For every iteration it records the
runtime GObject type counts, unique widget counts, same-instance repeats, and
whether repeated allocations retained or changed `x/y/width/height`. Allocation
instance IDs are process-local pointers used only to correlate callbacks within
one run. Detailed records are emitted after `GTK_EVENT_DRAIN_END`, so diagnostic
output I/O is excluded from the measured drain. The scenario report gives
median/MAD/min/max by widget type and for churn totals, plus type and churn
details for the three longest iterations in each sample. Repetition is evidence
to investigate, not proof that GTK work is safely avoidable.

When `GTK_EVENT_DRAIN_SESSION_PROFILE` / `GTK_EVENT_DRAIN_WIDGET_INSTANCE`
records are present (`attribution_version=3`), `profile-window-hotspots` also
reports cross-iteration identical versus changed geometry, the widgets and
subtrees that keep receiving the same allocation, and overlap among the most
expensive iterations. Those session records are likewise emitted after the
drain end marker.

The harness uses the deterministic GTK lifecycle smoke hook to exit through
GTK idles after `GTK_MAIN_ENTER`; it does not sleep, delay startup, or move work
past the main-loop milestone. Every run uses the same small SQLite fixture.
The `fresh-profile` samples each use a new HOME/XDG profile. One separately
recorded setup run initializes the profile used by all `reused-profile`
samples. These names deliberately describe application profile state only:
without privileged kernel cache eviction they must not be reported as true
storage-cold versus storage-warm measurements. Do not run unrelated workloads
during collection, and record build type, machine, display path, commit, and
working-tree state alongside results when publishing a baseline.

Each isolated profile starts with only these standard parent directories; the
collector does not copy or precreate application-specific configuration:

```text
HOME=<profile>/home
XDG_CONFIG_HOME=<profile>/config
XDG_DATA_HOME=<profile>/data
XDG_CACHE_HOME=<profile>/cache
XDG_STATE_HOME=<profile>/state
```

For a fresh sample `<profile>` is `profiles/fresh-N`. The independent setup and
all reused samples use `profiles/reused`. Biblia Elim itself must initialize
`XDG_CONFIG_HOME/xiphos`; a fresh run must not depend on the developer's real
HOME or on a pre-existing `HOME/.config`.

Interpret the `APP_START` to `GTK_MAIN_ENTER` total together with its component
phases. Report all samples and dispersion, not only the fastest run. The
dominant phase is descriptive evidence for a later optimization task; this
baseline establishes no performance budget and authorizes no optimization.
