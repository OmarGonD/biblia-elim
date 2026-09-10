#!/usr/bin/env python3

import argparse
import importlib.util
import json
import os
import tempfile
import textwrap
import unittest
from pathlib import Path
from unittest import mock


SCRIPT = Path(__file__).parents[1] / "scripts" / "startup_performance_baseline.py"
SPEC = importlib.util.spec_from_file_location("startup_baseline", SCRIPT)
BASELINE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BASELINE)


def trace(values):
    return "\n".join(
        f"[UI-LOAD] app {event} {value:.1f}ms"
        for event, value in zip(BASELINE.EVENTS, values)
    )


def window_trace(values):
    return "\n".join(
        f"[UI-LOAD] app {event} {value:.1f}ms"
        for event, value in zip(BASELINE.WINDOW_PROFILE_EVENTS, values)
    )


def hotspot_trace(values, iteration_durations=(40, 20), attributed=False):
    lines = [
        f"[UI-LOAD] app {event} {value:.1f}ms"
        for event, value in zip(BASELINE.WINDOW_HOTSPOT_EVENTS, values)
    ]
    drain_start = values[7]
    cursor = drain_start + 5
    for number, duration in enumerate(iteration_durations):
        lines.extend([
            f"[UI-LOAD] app GTK_EVENT_ITERATION_BEGIN {cursor:.1f}ms iteration={number}",
            f"[UI-LOAD] app WINDOW_SIZE_ALLOCATE {cursor + 1:.1f}ms app width=960 height=640",
            f"[UI-LOAD] bible-main MAP {cursor + 2:.1f}ms",
            f"[UI-LOAD] app GTK_EVENT_ITERATION_END {cursor + duration:.1f}ms iteration={number}",
        ])
        if attributed:
            fields = {
                name: 0 for name in BASELINE.ITERATION_PROFILE_FIELDS
            }
            fields["allocate_renderer"] = number + 1
            fields["root_draws"] = 1 if number == 0 else 0
            fields["root_draw_us"] = 30000 if number == 0 else 0
            prefix = [f"iteration={number}"]
            if attributed == "extended":
                prefix.append("attribution_version=2")
                churn = {
                    "unique_allocated_widgets": 1,
                    "repeated_allocations": number,
                    "identical_geometry_repeats": number,
                    "changed_geometry_repeats": 0,
                    "unique_style_widgets": 0,
                    "repeated_style_updates": 0,
                }
            detail = " ".join(
                prefix
                + [f"{name}={fields[name]}"
                   for name in BASELINE.ITERATION_PROFILE_FIELDS]
                + ([f"{name}={churn[name]}"
                    for name in BASELINE.ITERATION_CHURN_FIELDS]
                   if attributed == "extended" else [])
            )
            lines.append(
                f"[UI-LOAD] app GTK_EVENT_ITERATION_PROFILE "
                f"{cursor + duration + 0.1:.1f}ms {detail}"
            )
            if attributed == "extended":
                lines.extend([
                    f"[UI-LOAD] app GTK_EVENT_ITERATION_WIDGET_TYPE "
                    f"{cursor + duration + 0.2:.1f}ms iteration={number} "
                    f"kind=allocate type=WkHtml count={number + 1} repeated={number}",
                    f"[UI-LOAD] app GTK_EVENT_ITERATION_ALLOCATE_INSTANCE "
                    f"{cursor + duration + 0.3:.1f}ms iteration={number} "
                    f"id=0x{number + 1:x} type=WkHtml count={number + 1} "
                    f"repeated={number} identical={number} changed=0 "
                    "x=0 y=0 width=960 height=640",
                ])
        cursor += duration + 5
    return "\n".join(lines)


def cross_iteration_trace(values):
    text = hotspot_trace(
        values, iteration_durations=(30, 25, 20, 15, 10), attributed="extended"
    )
    lines = []
    x_positions = (0, 0, 12, 12, 12)
    for line in text.splitlines():
        if "GTK_EVENT_ITERATION_ALLOCATE_INSTANCE" in line:
            iteration = int(line.split("iteration=", 1)[1].split()[0])
            line = line.replace(f"id=0x{iteration + 1:x}", "id=0x1")
            line = line.replace("x=0 y=0", f"x={x_positions[iteration]} y=0")
        lines.append(line)
    lines.extend([
        "[UI-LOAD] app GTK_EVENT_DRAIN_SESSION_PROFILE 500.0ms "
        "attribution_version=3 iterations=5 instances=2",
        "[UI-LOAD] app GTK_EVENT_DRAIN_WIDGET_INSTANCE 500.1ms "
        "attribution_version=3 id=0x1 type=WkHtml name=bible parent_type=GtkBox "
        "parent_name=content path=GtkWindow%23main/GtkBox%23content/WkHtml%23bible "
        "allocate_count=15 allocate_iterations=5 allocate_first=0 allocate_last=4 "
        "allocate_identical=3 allocate_changed=1 "
        "geometry=0:0,0,960,640;1:0,0,960,640;2:12,0,960,640;"
        "3:12,0,960,640;4:12,0,960,640 "
        "style_count=4 style_iterations=3 style_first=0 style_last=4",
        "[UI-LOAD] app GTK_EVENT_DRAIN_WIDGET_INSTANCE 500.2ms "
        "attribution_version=3 id=0x9 type=GtkLabel name=caption "
        "parent_type=GtkBox parent_name=content "
        "path=GtkWindow%23main/GtkBox%23content/GtkLabel%23caption "
        "allocate_count=0 allocate_iterations=0 allocate_first=0 allocate_last=0 "
        "allocate_identical=0 allocate_changed=0 geometry=- "
        "style_count=2 style_iterations=2 style_first=1 style_last=3",
    ])
    return "\n".join(lines)


class StartupPerformanceBaselineTest(unittest.TestCase):
    def test_parses_complete_trace_with_decimal_dot(self):
        parsed = BASELINE.parse_trace(
            """[UI-LOAD] app APP_START 0.0ms
[UI-LOAD] app GTK_INITIALIZED 51.2ms
[UI-LOAD] app WINDOW_CREATED 812.4ms
[UI-LOAD] app MODULE_READY 815.3ms
[UI-LOAD] app FIRST_CONTENT_REQUEST 1147.4ms
[UI-LOAD] app FIRST_CONTENT_READY 1158.9ms
[UI-LOAD] app FRONTEND_DISPLAY_DONE 1165.6ms
[UI-LOAD] app GTK_MAIN_ENTER 1195.5ms"""
        )
        self.assertEqual(parsed["GTK_INITIALIZED"], 51.2)
        self.assertEqual(parsed["GTK_MAIN_ENTER"], 1195.5)

    def test_parses_complete_trace_with_decimal_comma(self):
        parsed = BASELINE.parse_trace(
            """[UI-LOAD] app APP_START 0,0ms
[UI-LOAD] app GTK_INITIALIZED 51,2ms
[UI-LOAD] app WINDOW_CREATED 812,4ms
[UI-LOAD] app MODULE_READY 815,3ms
[UI-LOAD] app FIRST_CONTENT_REQUEST 1147,4ms
[UI-LOAD] app FIRST_CONTENT_READY 1158,9ms
[UI-LOAD] app FRONTEND_DISPLAY_DONE 1165,6ms
[UI-LOAD] app GTK_MAIN_ENTER 1195,5ms"""
        )
        self.assertEqual(parsed["GTK_INITIALIZED"], 51.2)
        self.assertEqual(parsed["GTK_MAIN_ENTER"], 1195.5)

    def test_parses_real_mixed_decimal_trace(self):
        parsed = BASELINE.parse_trace(
            """[UI-LOAD] app APP_START 0.0ms
[UI-LOAD] app GTK_INITIALIZED 51,2ms
[UI-LOAD] app WINDOW_CREATED 812,4ms
[UI-LOAD] app MODULE_READY 815,3ms
[UI-LOAD] app FIRST_CONTENT_REQUEST 1147,4ms
[UI-LOAD] app FIRST_CONTENT_READY 1158,9ms
[UI-LOAD] app FRONTEND_DISPLAY_DONE 1165,6ms
[UI-LOAD] app GTK_MAIN_ENTER 1195,5ms"""
        )
        self.assertEqual(parsed["APP_START"], 0.0)
        self.assertEqual(parsed["GTK_INITIALIZED"], 51.2)

    def test_ignores_locale_formatted_auxiliary_timestamps(self):
        parsed = BASELINE.parse_trace(
            """[UI-LOAD] app APP_START 0.0ms
[UI-LOAD] app GTK_INITIALIZED 51,2ms
[UI-LOAD] app WINDOW_CREATED 812,4ms
[UI-LOAD] app MODULE_READY 815,3ms havebible=true
[UI-LOAD] app FIRST_CONTENT_REQUEST 1147,4ms John 3:16
[UI-LOAD] app FIRST_CONTENT_READY 1158,9ms display_ms=11,5
[UI-LOAD] app FRONTEND_DISPLAY_DONE 1165,6ms display_ms=54,9
[UI-LOAD] app GTK_MAIN_ENTER 1195,5ms"""
        )
        self.assertEqual(parsed["FIRST_CONTENT_READY"], 1158.9)
        self.assertEqual(parsed["FRONTEND_DISPLAY_DONE"], 1165.6)

    def test_still_rejects_a_genuinely_missing_milestone(self):
        incomplete = """[UI-LOAD] app APP_START 0.0ms
[UI-LOAD] app GTK_INITIALIZED 51,2ms
[UI-LOAD] app WINDOW_CREATED 812,4ms
[UI-LOAD] app MODULE_READY 815,3ms
[UI-LOAD] app FIRST_CONTENT_REQUEST 1147,4ms
[UI-LOAD] app FIRST_CONTENT_READY 1158,9ms
[UI-LOAD] app FRONTEND_DISPLAY_DONE 1165,6ms"""
        with self.assertRaisesRegex(ValueError, "GTK_MAIN_ENTER"):
            BASELINE.parse_trace(incomplete)

    def test_profile_contains_standard_xdg_parents_only(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            environment = BASELINE.make_profile(root)
            self.assertEqual(
                set(environment),
                {
                    "HOME",
                    "XDG_CONFIG_HOME",
                    "XDG_DATA_HOME",
                    "XDG_CACHE_HOME",
                    "XDG_STATE_HOME",
                },
            )
            for path in environment.values():
                self.assertTrue(Path(path).is_dir())
            self.assertFalse((Path(environment["XDG_CONFIG_HOME"]) / "xiphos").exists())
            self.assertFalse((Path(environment["HOME"]) / ".config").exists())

    def test_requires_every_ordered_milestone(self):
        values = [0, 100, 180, 240, 260, 300, 330, 350]
        parsed = BASELINE.sample_from_trace(trace(values))
        self.assertEqual(parsed["phases_ms"]["APP_START->GTK_MAIN_ENTER"], 350)
        self.assertEqual(parsed["phases_ms"]["GTK_INITIALIZED->WINDOW_CREATED"], 80)
        with self.assertRaisesRegex(ValueError, "missing startup events"):
            BASELINE.parse_trace(trace(values)[:-25])
        with self.assertRaisesRegex(ValueError, "not monotonic"):
            BASELINE.parse_trace(trace([0, 100, 90, 240, 260, 300, 330, 350]))

    def test_summarizes_multiple_scenarios_and_dispersion(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for scenario, totals in (
                ("fresh-profile", [340, 350, 390]),
                ("reused-profile", [300, 310, 330]),
            ):
                for number, total in enumerate(totals, 1):
                    values = [0, 50, 100, 150, 180, 220, 250, total]
                    (directory / f"{scenario}-{number:02d}.log").write_text(
                        trace(values), encoding="utf-8"
                    )
            summary = BASELINE.summarize(directory)
            fresh = summary["scenarios"]["fresh-profile"]
            total = fresh["metrics"]["APP_START->GTK_MAIN_ENTER"]
            self.assertEqual(fresh["sample_count"], 3)
            self.assertEqual(total["median_ms"], 350)
            self.assertEqual(total["mad_ms"], 10)
            self.assertEqual((total["min_ms"], total["max_ms"]), (340, 390))
            self.assertIn(fresh["dominant_phase"], fresh["metrics"])

    def test_window_profile_partitions_entire_dominant_phase(self):
        values = [50, 60, 80, 100, 140, 200, 250, 300, 340, 350, 410, 450,
                  700, 960, 970]
        phases = BASELINE.window_profile_from_trace(window_trace(values))
        components = [
            value for phase, value in phases.items()
            if phase != "GTK_INITIALIZED->WINDOW_CREATED"
        ]
        self.assertEqual(phases["GTK_INITIALIZED->WINDOW_CREATED"], 920)
        self.assertEqual(round(sum(components), 1), 920)
        self.assertEqual(phases["WINDOW_TREE_SHOWN->WINDOW_EVENTS_DRAINED"], 260)

    def test_window_profile_requires_every_ordered_boundary(self):
        values = list(range(0, len(BASELINE.WINDOW_PROFILE_EVENTS) * 10, 10))
        with self.assertRaisesRegex(ValueError, "missing window-profile events"):
            BASELINE.window_profile_from_trace(window_trace(values)[:-35])
        values[-2] = values[-3] - 1
        with self.assertRaisesRegex(ValueError, "not monotonic"):
            BASELINE.window_profile_from_trace(window_trace(values))

    def test_summarizes_multiple_window_profile_runs(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            base = list(range(0, len(BASELINE.WINDOW_PROFILE_EVENTS) * 10, 10))
            for scenario, offsets in (("fresh-profile", [0, 2, 10]),
                                      ("reused-profile", [0, 4, 8])):
                for number, offset in enumerate(offsets, 1):
                    values = base.copy()
                    values[-1] += offset
                    (directory / f"{scenario}-{number:02d}.log").write_text(
                        window_trace(values), encoding="utf-8"
                    )
            summary = BASELINE.summarize_window_profile(directory)
            fresh = summary["scenarios"]["fresh-profile"]
            total = fresh["metrics"]["GTK_INITIALIZED->WINDOW_CREATED"]
            self.assertEqual(fresh["sample_count"], 3)
            self.assertEqual(total["median_ms"], 142)
            self.assertEqual(total["mad_ms"], 2)

    def test_window_hotspot_profile_pairs_iterations_and_accounts_for_drain(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 391, 391, 392]
        sample = BASELINE.window_hotspot_from_trace(hotspot_trace(values))
        self.assertEqual(sample["phases_ms"][
            "WINDOW_SHOW_ALL_BEGIN->WINDOW_SHOW_ALL_END"], 200)
        self.assertEqual(sample["iteration_count"], 2)
        self.assertEqual(sample["iteration_total_ms"], 60)
        self.assertEqual(sample["longest_iteration_ms"], 40)
        self.assertEqual(sample["drain_residual_ms"], 15)
        self.assertEqual(len(sample["lifecycle"]), 4)

    def test_window_hotspot_profile_rejects_unpaired_iterations(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 391, 391, 392]
        malformed = hotspot_trace(values).replace(
            "[UI-LOAD] app GTK_EVENT_ITERATION_END 386.0ms iteration=1", ""
        )
        with self.assertRaisesRegex(ValueError, "unpaired"):
            BASELINE.window_hotspot_from_trace(malformed)

    def test_attributes_long_iterations_to_callback_categories(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 391, 391, 392]
        sample = BASELINE.window_hotspot_from_trace(
            hotspot_trace(values, attributed=True)
        )
        attribution = sample["attribution"]
        self.assertEqual(attribution["totals"]["allocate_renderer"], 3)
        self.assertEqual(attribution["totals"]["root_draw_us"], 30000)
        self.assertEqual(attribution["expensive_iterations"][0]["iteration"], 0)
        self.assertEqual(attribution["expensive_iterations"][0]["duration_ms"], 40)

    def test_rejects_incomplete_iteration_attribution(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 391, 391, 392]
        malformed = hotspot_trace(values, attributed=True).replace(
            " renderer_draw_us=0", "", 1
        )
        with self.assertRaisesRegex(ValueError, "incomplete GTK iteration profile"):
            BASELINE.window_hotspot_from_trace(malformed)

    def test_legacy_attribution_trace_remains_compatible(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 391, 391, 392]
        sample = BASELINE.window_hotspot_from_trace(
            hotspot_trace(values, attributed=True)
        )
        self.assertIn("attribution", sample)
        self.assertNotIn("churn_totals", sample["attribution"])

    def test_aggregates_widget_types_across_scenario_samples(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            values = [100, 105, 110, 111, 311, 315, 316, 316, 391, 391, 392]
            for scenario in ("fresh-profile", "reused-profile"):
                for number in range(1, 4):
                    (directory / f"{scenario}-{number:02d}.log").write_text(
                        hotspot_trace(values, attributed="extended"), encoding="utf-8"
                    )
            summary = BASELINE.summarize_window_hotspots(directory)
            allocation = summary["scenarios"]["fresh-profile"]["widget_types"][
                "allocate"]["WkHtml"]["count"]
            self.assertEqual(allocation["median_ms"], 3)
            self.assertEqual(allocation["mad_ms"], 0)
            self.assertEqual((allocation["min_ms"], allocation["max_ms"]), (3, 3))
            report = BASELINE.window_hotspot_markdown(summary)
            self.assertIn("Widget types by allocate count", report)
            self.assertIn("unique allocated=1", report)
            self.assertIn("allocation types: WkHtml=2", report)

    def test_tracks_unique_allocated_widget_instances(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 391, 391, 392]
        sample = BASELINE.window_hotspot_from_trace(
            hotspot_trace(values, attributed="extended")
        )
        self.assertEqual(
            sample["attribution"]["churn_totals"]["unique_allocated_widgets"], 2
        )
        self.assertEqual(len(sample["attribution"]["per_iteration"][0][
            "allocation_instances"]), 1)

    def test_detects_repeated_same_widget_allocation(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 391, 391, 392]
        sample = BASELINE.window_hotspot_from_trace(
            hotspot_trace(values, attributed="extended")
        )
        second = sample["attribution"]["per_iteration"][1]
        self.assertEqual(second["churn"]["repeated_allocations"], 1)
        self.assertEqual(second["allocation_instances"][0]["repeated"], 1)

    def test_detects_identical_geometry_repeat(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 391, 391, 392]
        sample = BASELINE.window_hotspot_from_trace(
            hotspot_trace(values, attributed="extended")
        )
        self.assertEqual(sample["attribution"]["per_iteration"][1]["churn"][
            "identical_geometry_repeats"], 1)

    def test_detects_changed_geometry_repeat(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 391, 391, 392]
        changed = hotspot_trace(values, attributed="extended").replace(
            "identical_geometry_repeats=1 changed_geometry_repeats=0",
            "identical_geometry_repeats=0 changed_geometry_repeats=1",
        ).replace("repeated=1 identical=1 changed=0 x=0",
                  "repeated=1 identical=0 changed=1 x=12")
        sample = BASELINE.window_hotspot_from_trace(changed)
        instance = sample["attribution"]["per_iteration"][1][
            "allocation_instances"][0]
        self.assertEqual(instance["changed"], 1)
        self.assertEqual(instance["x"], 12)

    def test_detects_repeated_style_update(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 391, 391, 392]
        styled = hotspot_trace(values, attributed="extended").replace(
            "style_other=0", "style_other=2", 1
        ).replace(
            "unique_style_widgets=0 repeated_style_updates=0",
            "unique_style_widgets=1 repeated_style_updates=1", 1
        )
        marker = "[UI-LOAD] app GTK_EVENT_ITERATION_ALLOCATE_INSTANCE"
        styled = styled.replace(
            marker,
            "[UI-LOAD] app GTK_EVENT_ITERATION_WIDGET_TYPE 356.2ms "
            "iteration=0 kind=style type=GtkLabel count=2 repeated=1\n" + marker,
            1,
        )
        sample = BASELINE.window_hotspot_from_trace(styled)
        first = sample["attribution"]["per_iteration"][0]
        self.assertEqual(first["churn"]["repeated_style_updates"], 1)
        self.assertEqual(first["widget_types"]["style"]["GtkLabel"]["repeated"], 1)

    def test_rejects_malformed_extended_attribution_records(self):
        with self.assertRaisesRegex(ValueError, "GTK widget type attribution"):
            BASELINE.parse_widget_type_profile(
                "iteration=0 kind=allocate type=GtkLabel count=no repeated=0"
            )
        with self.assertRaisesRegex(ValueError, "allocation instance"):
            BASELINE.parse_allocate_instance_profile(
                "iteration=0 id=not-a-pointer type=GtkLabel count=1 repeated=0 "
                "identical=0 changed=0 x=0 y=0 width=10 height=10"
            )

    def test_tracks_widget_identity_and_geometry_across_iterations(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 450, 450, 451]
        session = BASELINE.window_hotspot_from_trace(
            cross_iteration_trace(values)
        )["attribution"]["drain_session"]
        widget = session["widgets"][0]
        self.assertEqual(widget["id"], "0x1")
        self.assertEqual(widget["allocate_count"], 15)
        self.assertEqual(widget["allocate_iterations"], 5)
        self.assertEqual((widget["allocate_first"], widget["allocate_last"]), (0, 4))
        self.assertEqual(widget["allocate_identical"], 3)
        self.assertEqual(widget["allocate_changed"], 1)
        self.assertEqual(widget["geometry_sequence"][2]["geometry"], (12, 0, 960, 640))

    def test_summarizes_cross_iteration_buckets_overlap_types_and_subtrees(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 450, 450, 451]
        session = BASELINE.window_hotspot_from_trace(
            cross_iteration_trace(values)
        )["attribution"]["drain_session"]
        metrics = session["metrics"]
        self.assertEqual(metrics["cross_iteration_repeated_allocations"], 4)
        self.assertEqual(
            (metrics["widgets_allocated_ge2"], metrics["widgets_allocated_ge3"],
             metrics["widgets_allocated_ge5"], metrics["max_allocation_iterations"]),
            (1, 1, 1, 5),
        )
        self.assertEqual(
            (metrics["cross_iteration_repeated_styles"], metrics["widgets_styled_ge2"],
             metrics["widgets_styled_ge3"], metrics["widgets_styled_ge5"],
             metrics["max_style_iterations"]),
            (3, 2, 1, 0, 3),
        )
        self.assertEqual(BASELINE.widget_set_overlap({"a", "b"}, {"b", "c"}), {
            "intersection": 1, "union": 3, "similarity": 0.333,
        })
        self.assertEqual(session["clusters"][0]["similarity"], 1.0)
        self.assertEqual(session["top_repeated_types"][0][0], "WkHtml")
        self.assertEqual(session["top_repeated_types"][0][1]["iterations_touched"], 5)
        self.assertIn("GtkBox%23content", session["top_repeated_subtrees"][0][0])

    def test_reports_cross_iteration_context_for_longest_iterations(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 450, 450, 451]
        sample = BASELINE.window_hotspot_from_trace(cross_iteration_trace(values))
        expensive = sample["attribution"]["drain_session"]["expensive_iterations"]
        middle = next(item for item in expensive if item["iteration"] == 1)
        cross = middle["cross_iteration"]
        self.assertEqual(cross["allocated_previously"], 1)
        self.assertEqual(cross["same_geometry_as_previous"], 1)
        self.assertEqual(cross["changed_geometry_from_previous"], 0)
        self.assertEqual(cross["overlap_previous_expensive"]["similarity"], 1.0)
        self.assertEqual(cross["overlap_next_expensive"]["similarity"], 1.0)
        self.assertIn("GtkBox%23content", cross["dominant_subtree"])

    def test_rejects_malformed_cross_iteration_session_attribution(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 450, 450, 451]
        trace_text = cross_iteration_trace(values)
        malformed_cases = (
            trace_text.replace("iterations=5 instances=2", "iterations=4 instances=2"),
            trace_text.replace("allocate_first=0 allocate_last=4",
                               "allocate_first=1 allocate_last=4"),
            trace_text.replace("2:12,0,960,640", "2:13,0,960,640"),
            trace_text.replace("style_iterations=3 style_first=0 style_last=4",
                               "style_iterations=6 style_first=0 style_last=4"),
        )
        for malformed in malformed_cases:
            with self.subTest(malformed=malformed[-180:]):
                with self.assertRaisesRegex(ValueError, "GTK drain|cross-iteration"):
                    BASELINE.window_hotspot_from_trace(malformed)

    def test_summarizes_version_3_while_preserving_v1_v2_compatibility(self):
        values = [100, 105, 110, 111, 311, 315, 316, 316, 450, 450, 451]
        for attributed in (True, "extended"):
            legacy = BASELINE.window_hotspot_from_trace(
                hotspot_trace(values, attributed=attributed)
            )
            self.assertNotIn("drain_session", legacy["attribution"])
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for scenario in ("fresh-profile", "reused-profile"):
                for number in range(1, 4):
                    (directory / f"{scenario}-{number:02d}.log").write_text(
                        cross_iteration_trace(values), encoding="utf-8"
                    )
            summary = BASELINE.summarize_window_hotspots(directory)
            fresh = summary["scenarios"]["fresh-profile"]
            self.assertEqual(fresh["drain_session_metrics"][
                "widgets_allocated_ge5"]["median_ms"], 1)
            report = BASELINE.window_hotspot_markdown(summary)
            self.assertIn("Top repeated types", report)
            self.assertIn("previous/next expensive overlap=", report)

    def test_summarizes_multiple_window_hotspot_runs(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for scenario, show_durations in (("fresh-profile", [200, 220, 260]),
                                             ("reused-profile", [180, 190, 210])):
                for number, duration in enumerate(show_durations, 1):
                    values = [100, 105, 110, 111, 111 + duration,
                              315 + duration, 316 + duration, 316 + duration,
                              391 + duration, 391 + duration, 392 + duration]
                    (directory / f"{scenario}-{number:02d}.log").write_text(
                        hotspot_trace(values), encoding="utf-8"
                    )
            summary = BASELINE.summarize_window_hotspots(directory)
            show = summary["scenarios"]["fresh-profile"]["metrics"][
                "WINDOW_SHOW_ALL_BEGIN->WINDOW_SHOW_ALL_END"]
            self.assertEqual(show["median_ms"], 220)
            self.assertEqual(show["mad_ms"], 20)

    def test_summarizes_attributed_expensive_iterations(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for scenario in ("fresh-profile", "reused-profile"):
                for number in range(1, 4):
                    values = [100, 105, 110, 111, 311, 315, 316, 316,
                              391, 391, 392]
                    (directory / f"{scenario}-{number:02d}.log").write_text(
                        hotspot_trace(values, attributed=True), encoding="utf-8"
                    )
            summary = BASELINE.summarize_window_hotspots(directory)
            fresh = summary["scenarios"]["fresh-profile"]
            self.assertEqual(
                fresh["callback_totals"]["allocate_renderer"]["median_ms"], 3
            )
            self.assertEqual(len(fresh["expensive_iterations"]), 6)
            report = BASELINE.window_hotspot_markdown(summary)
            self.assertIn("fresh-profile-01.log", report)
            self.assertIn("allocate_renderer=1", report)

    def test_rejects_fewer_than_three_runs(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            values = [0, 50, 100, 150, 180, 220, 250, 300]
            for number in range(1, 3):
                (directory / f"fresh-profile-{number:02d}.log").write_text(
                    trace(values), encoding="utf-8"
                )
            with self.assertRaisesRegex(ValueError, "at least three"):
                BASELINE.summarize(directory)

    def test_collect_uses_fresh_and_reused_isolated_profiles(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fixture = root / "fixture.sql"
            fixture.write_text("deterministic sqlite fixture\n", encoding="utf-8")
            records = root / "records.jsonl"
            fake_sqlite = root / "fake-sqlite"
            fake_sqlite.write_text(
                "#!/usr/bin/env python3\n"
                "import pathlib, sys\n"
                "pathlib.Path(sys.argv[1]).write_bytes(sys.stdin.buffer.read())\n",
                encoding="utf-8",
            )
            fake_sqlite.chmod(0o700)
            fake_app = root / "fake-biblia-elim"
            fake_app.write_text(
                textwrap.dedent(
                    f"""\
                    #!/usr/bin/env python3
                    import json, os, pathlib, sys
                    names = ("HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME",
                             "XDG_CACHE_HOME", "XDG_STATE_HOME")
                    paths = {{name: pathlib.Path(os.environ[name]) for name in names}}
                    module_dir = pathlib.Path(sys.argv[1].split(":", 1)[1])
                    profile = paths["XDG_CONFIG_HOME"] / "xiphos"
                    record = {{
                        "paths": {{name: str(path) for name, path in paths.items()}},
                        "parents_exist": all(path.is_dir() for path in paths.values()),
                        "profile_existed": profile.exists(),
                        "fixture": (module_dir / "baseline.sqlite").read_text(),
                    }}
                    with open({str(records)!r}, "a", encoding="utf-8") as output:
                        output.write(json.dumps(record) + "\\n")
                    profile.mkdir(parents=True, exist_ok=True)
                    events = {BASELINE.EVENTS!r}
                    for number, event in enumerate(events):
                        print(f"[UI-LOAD] app {{event}} {{number * 10}}.0ms")
                    """
                ),
                encoding="utf-8",
            )
            fake_app.chmod(0o700)
            output = root / "baseline"
            args = argparse.Namespace(
                app=fake_app,
                fixture=fixture,
                sqlite3=fake_sqlite,
                xvfb_run=None,
                output=output,
                runs=7,
                timeout=5,
            )
            real_home = os.environ.get("HOME")
            with mock.patch.dict(os.environ, {}, clear=False):
                BASELINE.collect(args)

            collected = [json.loads(line) for line in records.read_text().splitlines()]
            self.assertEqual(len(collected), 15)
            self.assertTrue(all(record["parents_exist"] for record in collected))
            self.assertTrue(all(record["fixture"] == fixture.read_text() for record in collected))
            fresh = collected[:7]
            setup = collected[7]
            reused = collected[8:]
            self.assertEqual(len({record["paths"]["HOME"] for record in fresh}), 7)
            self.assertTrue(all(not record["profile_existed"] for record in fresh))
            self.assertFalse(setup["profile_existed"])
            self.assertEqual(
                {record["paths"]["HOME"] for record in [setup, *reused]},
                {setup["paths"]["HOME"]},
            )
            self.assertTrue(all(record["profile_existed"] for record in reused))
            if real_home:
                self.assertTrue(
                    all(record["paths"]["HOME"] != real_home for record in collected)
                )
            summary = json.loads((output / "summary.json").read_text())
            self.assertEqual(summary["scenarios"]["fresh-profile"]["sample_count"], 7)
            self.assertEqual(summary["scenarios"]["reused-profile"]["sample_count"], 7)
            self.assertTrue((output / "profile-setup.log").is_file())
            self.assertFalse((output / "profiles").exists())
            for log in output.glob("*.log"):
                self.assertNotIn("cannot create directory", log.read_text())


if __name__ == "__main__":
    unittest.main()
