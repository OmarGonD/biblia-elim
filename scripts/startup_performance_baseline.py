#!/usr/bin/env python3
"""Collect and summarize Biblia Elim startup milestone traces."""

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
import statistics
import subprocess
import sys
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path


EVENTS = (
    "APP_START",
    "GTK_INITIALIZED",
    "WINDOW_CREATED",
    "MODULE_READY",
    "FIRST_CONTENT_REQUEST",
    "FIRST_CONTENT_READY",
    "FRONTEND_DISPLAY_DONE",
    "GTK_MAIN_ENTER",
)
PHASES = tuple(zip(EVENTS, EVENTS[1:]))
WINDOW_PROFILE_EVENTS = (
    "GTK_INITIALIZED",
    "WINDOW_PREPARED",
    "HTML_INITIALIZED",
    "THEME_INITIALIZED",
    "WINDOW_SHELL_READY",
    "BIBLE_PANE_READY",
    "PREVIEWER_PANE_READY",
    "COMMENTARY_PANE_READY",
    "BOOK_PANE_READY",
    "NOTES_PANE_READY",
    "DICTIONARY_PANE_READY",
    "DEVOTIONAL_PANE_READY",
    "WINDOW_TREE_SHOWN",
    "WINDOW_EVENTS_DRAINED",
    "WINDOW_CREATED",
)
WINDOW_PROFILE_PHASES = tuple(zip(WINDOW_PROFILE_EVENTS, WINDOW_PROFILE_EVENTS[1:]))
WINDOW_HOTSPOT_EVENTS = (
    "DEVOTIONAL_PANE_READY",
    "STATUSBAR_READY",
    "WINDOW_LAYOUT_RESTORED",
    "WINDOW_SHOW_ALL_BEGIN",
    "WINDOW_SHOW_ALL_END",
    "WINDOW_VISIBILITY_SYNCED",
    "WINDOW_TREE_SHOWN",
    "GTK_EVENT_DRAIN_BEGIN",
    "GTK_EVENT_DRAIN_END",
    "WINDOW_EVENTS_DRAINED",
    "WINDOW_SIGNALS_CONNECTED",
)
WINDOW_HOTSPOT_PHASES = tuple(zip(WINDOW_HOTSPOT_EVENTS, WINDOW_HOTSPOT_EVENTS[1:]))
TRACE = re.compile(
    r"^\[UI-LOAD\] (\S+) (\S+) ([0-9]+(?:[.,][0-9]+)?)ms(?:\s(.*))?$"
)
ITERATION_PROFILE_FIELDS = (
    "realize_renderer", "realize_paned", "realize_notebook", "realize_other",
    "map_renderer", "map_paned", "map_notebook", "map_other",
    "style_renderer", "style_paned", "style_notebook", "style_other",
    "allocate_renderer", "allocate_paned", "allocate_notebook", "allocate_other",
    "root_draws", "root_draw_us", "renderer_draws", "renderer_draw_us",
)
ITERATION_CHURN_FIELDS = (
    "unique_allocated_widgets", "repeated_allocations",
    "identical_geometry_repeats", "changed_geometry_repeats",
    "unique_style_widgets", "repeated_style_updates",
)
DRAIN_SESSION_FIELDS = (
    "distinct_allocated_widgets", "total_allocation_callbacks",
    "cross_iteration_repeated_allocations",
    "cross_iteration_identical_geometry",
    "cross_iteration_changed_geometry", "widgets_allocated_ge2",
    "widgets_allocated_ge3", "widgets_allocated_ge5",
    "max_allocation_iterations", "distinct_styled_widgets",
    "total_style_callbacks", "cross_iteration_repeated_styles",
    "widgets_styled_ge2", "widgets_styled_ge3", "widgets_styled_ge5",
    "max_style_iterations",
)


def parse_trace(text: str) -> dict[str, float]:
    return parse_events(text, EVENTS, "startup")


def parse_events(text: str, expected: tuple[str, ...], description: str) -> dict[str, float]:
    values: dict[str, float] = {}
    for line in text.splitlines():
        match = TRACE.match(line)
        if (match and match.group(1) == "app" and match.group(2) in expected
                and match.group(2) not in values):
            values[match.group(2)] = float(match.group(3).replace(",", "."))
    missing = [event for event in expected if event not in values]
    if missing:
        raise ValueError(f"missing {description} events: " + ", ".join(missing))
    timestamps = [values[event] for event in expected]
    if timestamps != sorted(timestamps):
        raise ValueError(f"{description} events are not monotonic")
    return values


def sample_from_trace(text: str) -> dict[str, object]:
    events = parse_trace(text)
    phases = {
        f"{start}->{end}": round(events[end] - events[start], 1)
        for start, end in PHASES
    }
    phases["APP_START->GTK_MAIN_ENTER"] = round(
        events["GTK_MAIN_ENTER"] - events["APP_START"], 1
    )
    return {"events_ms": events, "phases_ms": phases}


def metric(values: list[float]) -> dict[str, float]:
    median = statistics.median(values)
    return {
        "median_ms": round(median, 1),
        "mad_ms": round(statistics.median(abs(value - median) for value in values), 1),
        "min_ms": round(min(values), 1),
        "max_ms": round(max(values), 1),
    }


def window_profile_from_trace(text: str) -> dict[str, float]:
    events = parse_events(text, WINDOW_PROFILE_EVENTS, "window-profile")
    phases = {
        f"{start}->{end}": round(events[end] - events[start], 1)
        for start, end in WINDOW_PROFILE_PHASES
    }
    phases["GTK_INITIALIZED->WINDOW_CREATED"] = round(
        events["WINDOW_CREATED"] - events["GTK_INITIALIZED"], 1
    )
    return phases


def summarize_window_profile(directory: Path) -> dict[str, object]:
    scenarios: dict[str, object] = {}
    for scenario in ("fresh-profile", "reused-profile"):
        paths = sorted(directory.glob(f"{scenario}-*.log"))
        if len(paths) < 3:
            raise ValueError(f"at least three {scenario} logs are required in {directory}")
        samples = [window_profile_from_trace(path.read_text(encoding="utf-8"))
                   for path in paths]
        phase_names = list(samples[0])
        scenarios[scenario] = {
            "sample_count": len(samples),
            "metrics": {
                phase: metric([sample[phase] for sample in samples])
                for phase in phase_names
            },
        }
    return {"format_version": 1, "scenarios": scenarios}


def trace_records(text: str) -> list[dict[str, object]]:
    records = []
    for line in text.splitlines():
        match = TRACE.match(line)
        if match:
            records.append({
                "surface": match.group(1),
                "event": match.group(2),
                "timestamp_ms": float(match.group(3).replace(",", ".")),
                "detail": match.group(4) or "",
            })
    return records


def detail_values(detail: str, description: str) -> dict[str, str]:
    values = {}
    for token in detail.split():
        if "=" not in token:
            raise ValueError(f"malformed {description} record")
        name, value = token.split("=", 1)
        if not name or not value or name in values:
            raise ValueError(f"malformed {description} record")
        values[name] = value
    return values


def integer_value(values: dict[str, str], name: str, description: str) -> int:
    try:
        value = int(values[name])
    except (KeyError, ValueError) as error:
        raise ValueError(f"invalid {description} value: {name}") from error
    if value < 0:
        raise ValueError(f"invalid {description} value: {name}")
    return value


def parse_iteration_profile(detail: str) -> dict[str, int]:
    tokens = detail_values(detail, "GTK iteration profile")
    values = {}
    for name in ("iteration", *ITERATION_PROFILE_FIELDS):
        if name in tokens:
            values[name] = integer_value(tokens, name, "GTK iteration profile")
    missing = [name for name in ("iteration", *ITERATION_PROFILE_FIELDS)
               if name not in values]
    if missing:
        raise ValueError("incomplete GTK iteration profile: " + ", ".join(missing))
    if "attribution_version" in tokens:
        version = integer_value(tokens, "attribution_version", "GTK iteration profile")
        if version != 2:
            raise ValueError("unsupported GTK iteration attribution version")
        values["attribution_version"] = version
        missing = [name for name in ITERATION_CHURN_FIELDS if name not in tokens]
        if missing:
            raise ValueError("incomplete GTK iteration churn profile: " +
                             ", ".join(missing))
        for name in ITERATION_CHURN_FIELDS:
            values[name] = integer_value(tokens, name, "GTK iteration churn profile")
    return values


def parse_widget_type_profile(detail: str) -> dict[str, object]:
    values = detail_values(detail, "GTK widget type attribution")
    required = {"iteration", "kind", "type", "count", "repeated"}
    if set(values) != required or values["kind"] not in {"allocate", "style"}:
        raise ValueError("malformed GTK widget type attribution record")
    count = integer_value(values, "count", "GTK widget type attribution")
    repeated = integer_value(values, "repeated", "GTK widget type attribution")
    if not values["type"] or repeated > count:
        raise ValueError("malformed GTK widget type attribution record")
    return {
        "iteration": integer_value(values, "iteration", "GTK widget type attribution"),
        "kind": values["kind"], "type": values["type"],
        "count": count, "repeated": repeated,
    }


def parse_allocate_instance_profile(detail: str) -> dict[str, object]:
    values = detail_values(detail, "GTK allocation instance attribution")
    required = {"iteration", "id", "type", "count", "repeated", "identical",
                "changed", "x", "y", "width", "height"}
    if set(values) != required or not re.fullmatch(r"0x[0-9a-fA-F]+", values["id"]):
        raise ValueError("malformed GTK allocation instance attribution record")
    result: dict[str, object] = {
        "iteration": integer_value(values, "iteration", "GTK allocation instance attribution"),
        "id": values["id"], "type": values["type"],
    }
    for name in ("count", "repeated", "identical", "changed"):
        result[name] = integer_value(values, name, "GTK allocation instance attribution")
    for name in ("x", "y", "width", "height"):
        try:
            result[name] = int(values[name])
        except ValueError as error:
            raise ValueError("invalid GTK allocation instance geometry") from error
    if (not result["type"] or result["count"] < 1
            or result["repeated"] != result["count"] - 1
            or result["identical"] + result["changed"] != result["repeated"]):
        raise ValueError("inconsistent GTK allocation instance attribution record")
    return result


def parse_drain_session_profile(detail: str) -> dict[str, int]:
    values = detail_values(detail, "GTK drain session attribution")
    if set(values) != {"attribution_version", "iterations", "instances"}:
        raise ValueError("malformed GTK drain session attribution record")
    if integer_value(values, "attribution_version", "GTK drain session attribution") != 3:
        raise ValueError("unsupported GTK drain session attribution version")
    return {name: integer_value(values, name, "GTK drain session attribution")
            for name in ("iterations", "instances")}


def parse_geometry_sequence(value: str) -> list[dict[str, object]]:
    if value == "-":
        return []
    sequence = []
    for item in value.split(";"):
        match = re.fullmatch(r"(\d+):(-?\d+),(-?\d+),(-?\d+),(-?\d+)", item)
        if not match:
            raise ValueError("malformed GTK drain geometry sequence")
        sequence.append({
            "iteration": int(match.group(1)),
            "geometry": tuple(int(match.group(index)) for index in range(2, 6)),
        })
    if [item["iteration"] for item in sequence] != sorted(
            {item["iteration"] for item in sequence}):
        raise ValueError("malformed GTK drain geometry sequence")
    return sequence


def parse_drain_widget_profile(detail: str) -> dict[str, object]:
    values = detail_values(detail, "GTK drain widget attribution")
    required = {
        "attribution_version", "id", "type", "name", "parent_type",
        "parent_name", "path", "allocate_count", "allocate_iterations",
        "allocate_first", "allocate_last", "allocate_identical",
        "allocate_changed", "geometry", "style_count", "style_iterations",
        "style_first", "style_last",
    }
    if (set(values) != required or
            integer_value(values, "attribution_version",
                          "GTK drain widget attribution") != 3 or
            not re.fullmatch(r"0x[0-9a-fA-F]+", values["id"]) or
            any(not values[name] for name in
                ("type", "name", "parent_type", "parent_name", "path"))):
        raise ValueError("malformed GTK drain widget attribution record")
    result: dict[str, object] = {
        name: values[name] for name in
        ("id", "type", "name", "parent_type", "parent_name", "path")
    }
    for name in ("allocate_count", "allocate_iterations", "allocate_first",
                 "allocate_last", "allocate_identical", "allocate_changed",
                 "style_count", "style_iterations", "style_first", "style_last"):
        result[name] = integer_value(values, name, "GTK drain widget attribution")
    result["geometry_sequence"] = parse_geometry_sequence(values["geometry"])
    return result


def aggregate_session_widgets(records: list[dict[str, object]], key: str) -> dict:
    groups = defaultdict(lambda: {
        "widgets": 0, "allocation_count": 0, "allocation_iterations": 0,
        "identical": 0, "changed": 0, "style_count": 0,
        "style_iterations": 0, "iteration_ids": set(),
    })
    for record in records:
        group = groups[record[key]]
        group["widgets"] += 1
        for target, source in (
                ("allocation_count", "allocate_count"),
                ("allocation_iterations", "allocate_iterations"),
                ("identical", "allocate_identical"),
                ("changed", "allocate_changed"),
                ("style_count", "style_count"),
                ("style_iterations", "style_iterations")):
            group[target] += record[source]
        group["iteration_ids"].update(
            entry["iteration"] for entry in record["geometry_sequence"])
    return {
        name: {**group, "iterations_touched": len(group.pop("iteration_ids"))}
        for name, group in sorted(groups.items())
    }


def widget_set_overlap(left: set[str], right: set[str]) -> dict[str, object]:
    union = left | right
    intersection = left & right
    return {
        "intersection": len(intersection), "union": len(union),
        "similarity": round(len(intersection) / len(union), 3) if union else 1.0,
    }


def drain_session_attribution(summary_records, widget_records, per_iteration):
    if len(summary_records) != 1:
        raise ValueError("missing or duplicate GTK drain session attribution profile")
    summary = parse_drain_session_profile(summary_records[0]["detail"])
    records = [parse_drain_widget_profile(record["detail"])
               for record in widget_records]
    if summary["iterations"] != len(per_iteration) or summary["instances"] != len(records):
        raise ValueError("inconsistent GTK drain session attribution profile")
    if len({record["id"] for record in records}) != len(records):
        raise ValueError("duplicate GTK drain widget attribution record")
    by_id = {record["id"]: record for record in records}
    iteration_sets = {
        item["iteration"]: {entry["id"] for entry in item["allocation_instances"]}
        for item in per_iteration
    }
    per_iteration_instances = defaultdict(list)
    for item in per_iteration:
        for entry in item["allocation_instances"]:
            per_iteration_instances[entry["id"]].append(entry)
    for record in records:
        allocate_iterations = record["allocate_iterations"]
        style_iterations = record["style_iterations"]
        sequence = record["geometry_sequence"]
        if (record["allocate_count"] < allocate_iterations or
                record["style_count"] < style_iterations or
                record["allocate_identical"] + record["allocate_changed"] !=
                max(allocate_iterations - 1, 0) or len(sequence) != allocate_iterations):
            raise ValueError("inconsistent GTK drain widget attribution record")
        if allocate_iterations:
            sequence_iterations = [entry["iteration"] for entry in sequence]
            if (record["allocate_first"] != sequence_iterations[0] or
                    record["allocate_last"] != sequence_iterations[-1] or
                    sequence_iterations[-1] >= len(per_iteration)):
                raise ValueError("inconsistent GTK drain allocation iteration range")
            observed = per_iteration_instances[record["id"]]
            if ([entry["iteration"] for entry in observed] != sequence_iterations or
                    sum(entry["count"] for entry in observed) != record["allocate_count"] or
                    any((entry["x"], entry["y"], entry["width"], entry["height"]) !=
                        sequence_entry["geometry"]
                        for entry, sequence_entry in zip(observed, sequence))):
                raise ValueError("inconsistent cross-iteration allocation attribution")
        elif (record["allocate_count"] or sequence or record["allocate_first"] or
              record["allocate_last"] or record["allocate_identical"] or
              record["allocate_changed"]):
            raise ValueError("inconsistent empty GTK drain allocation attribution")
        if style_iterations:
            if (record["style_first"] > record["style_last"] or
                    record["style_last"] >= len(per_iteration) or
                    style_iterations > record["style_last"] - record["style_first"] + 1):
                raise ValueError("inconsistent GTK drain style iteration range")
        elif record["style_count"] or record["style_first"] or record["style_last"]:
            raise ValueError("inconsistent empty GTK drain style attribution")
    unknown = set(per_iteration_instances) - set(by_id)
    if unknown:
        raise ValueError("GTK iteration instance missing from drain session attribution")

    allocated = [record for record in records if record["allocate_count"]]
    styled = [record for record in records if record["style_count"]]
    metrics = {
        "distinct_allocated_widgets": len(allocated),
        "total_allocation_callbacks": sum(r["allocate_count"] for r in allocated),
        "cross_iteration_repeated_allocations": sum(
            r["allocate_iterations"] - 1 for r in allocated),
        "cross_iteration_identical_geometry": sum(r["allocate_identical"] for r in allocated),
        "cross_iteration_changed_geometry": sum(r["allocate_changed"] for r in allocated),
        "widgets_allocated_ge2": sum(r["allocate_iterations"] >= 2 for r in allocated),
        "widgets_allocated_ge3": sum(r["allocate_iterations"] >= 3 for r in allocated),
        "widgets_allocated_ge5": sum(r["allocate_iterations"] >= 5 for r in allocated),
        "max_allocation_iterations": max((r["allocate_iterations"] for r in allocated), default=0),
        "distinct_styled_widgets": len(styled),
        "total_style_callbacks": sum(r["style_count"] for r in styled),
        "cross_iteration_repeated_styles": sum(r["style_iterations"] - 1 for r in styled),
        "widgets_styled_ge2": sum(r["style_iterations"] >= 2 for r in styled),
        "widgets_styled_ge3": sum(r["style_iterations"] >= 3 for r in styled),
        "widgets_styled_ge5": sum(r["style_iterations"] >= 5 for r in styled),
        "max_style_iterations": max((r["style_iterations"] for r in styled), default=0),
    }
    clusters = []
    for left in range(len(per_iteration)):
        for right in range(left + 1, len(per_iteration)):
            overlap = widget_set_overlap(iteration_sets[left], iteration_sets[right])
            if overlap["intersection"] and overlap["similarity"] >= 0.5:
                clusters.append({"left": left, "right": right, **overlap})
    clusters.sort(key=lambda item: (-item["similarity"], item["left"], item["right"]))

    expensive = sorted(per_iteration,
                       key=lambda item: (-item["duration_ms"], item["iteration"]))[:3]
    expensive_numbers = sorted(item["iteration"] for item in expensive)
    for item in expensive:
        number = item["iteration"]
        position = expensive_numbers.index(number)
        ids = iteration_sets[number]
        item["cross_iteration"] = {
            "overlap_previous_expensive": (
                widget_set_overlap(ids, iteration_sets[expensive_numbers[position - 1]])
                if position else None),
            "overlap_next_expensive": (
                widget_set_overlap(ids, iteration_sets[expensive_numbers[position + 1]])
                if position + 1 < len(expensive_numbers) else None),
        }
        previous_ids = {widget_id for widget_id in ids
                        if by_id[widget_id]["allocate_first"] < number}
        same = changed = 0
        parent_counts = Counter()
        for widget_id in previous_ids:
            sequence = by_id[widget_id]["geometry_sequence"]
            current_index = next(index for index, entry in enumerate(sequence)
                                 if entry["iteration"] == number)
            if sequence[current_index - 1]["geometry"] == sequence[current_index]["geometry"]:
                same += 1
            else:
                changed += 1
            parent_counts[by_id[widget_id]["path"].rsplit("/", 1)[0]] += 1
        item["cross_iteration"].update({
            "allocated_previously": len(previous_ids),
            "same_geometry_as_previous": same,
            "changed_geometry_from_previous": changed,
            "dominant_subtree": (parent_counts.most_common(1)[0][0]
                                 if parent_counts else "-"),
        })
    ordering = lambda record: (-record["allocate_iterations"],
                               -record["allocate_count"],
                               -record["allocate_identical"],
                               -record["allocate_changed"], record["path"], record["id"])
    aggregate_ordering = lambda entry: (
        -entry[1]["iterations_touched"], -entry[1]["allocation_count"],
        -entry[1]["identical"], -entry[1]["changed"], entry[0])
    types = aggregate_session_widgets(records, "type")
    subtree_records = [
        {**record, "subtree": record["path"].rsplit("/", 1)[0]}
        for record in records
    ]
    subtrees = aggregate_session_widgets(subtree_records, "subtree")
    return {
        "metrics": metrics, "widgets": records,
        "types": types, "subtrees": subtrees,
        "top_repeated_widgets": sorted(allocated, key=ordering)[:10],
        "top_repeated_types": sorted(types.items(), key=aggregate_ordering)[:10],
        "top_repeated_subtrees": sorted(subtrees.items(), key=aggregate_ordering)[:10],
        "clusters": clusters, "expensive_iterations": expensive,
    }


def window_hotspot_from_trace(text: str) -> dict[str, object]:
    events = parse_events(text, WINDOW_HOTSPOT_EVENTS, "window-hotspot")
    phases = {
        f"{start}->{end}": round(events[end] - events[start], 1)
        for start, end in WINDOW_HOTSPOT_PHASES
    }
    records = trace_records(text)
    drain_start = events["GTK_EVENT_DRAIN_BEGIN"]
    drain_end = events["GTK_EVENT_DRAIN_END"]
    drain_record_index = next(
        index for index, record in enumerate(records)
        if record["surface"] == "app" and record["event"] == "GTK_EVENT_DRAIN_BEGIN"
        and record["timestamp_ms"] == drain_start
    )
    next_drain_index = next(
        (index for index, record in enumerate(records[drain_record_index + 1:],
                                               drain_record_index + 1)
         if record["surface"] == "app" and record["event"] == "GTK_EVENT_DRAIN_BEGIN"),
        len(records),
    )
    attribution_records = records[drain_record_index:next_drain_index]
    begins = [record for record in records
              if record["surface"] == "app"
              and record["event"] == "GTK_EVENT_ITERATION_BEGIN"
              and drain_start <= record["timestamp_ms"] <= drain_end]
    ends = [record for record in records
            if record["surface"] == "app"
            and record["event"] == "GTK_EVENT_ITERATION_END"
            and drain_start <= record["timestamp_ms"] <= drain_end]
    if len(begins) != len(ends):
        raise ValueError("unpaired GTK event-drain iteration boundaries")
    iterations = []
    for begin, end in zip(begins, ends):
        if begin["detail"] != end["detail"] or end["timestamp_ms"] < begin["timestamp_ms"]:
            raise ValueError("invalid GTK event-drain iteration pair")
        iterations.append(round(end["timestamp_ms"] - begin["timestamp_ms"], 1))
    profile_records = [record for record in attribution_records
                       if record["surface"] == "app"
                       and record["event"] == "GTK_EVENT_ITERATION_PROFILE"]
    attribution = None
    if profile_records:
        if len(profile_records) != len(iterations):
            raise ValueError("unpaired GTK iteration attribution profiles")
        profiles = [parse_iteration_profile(record["detail"])
                    for record in profile_records]
        if [profile["iteration"] for profile in profiles] != list(range(len(iterations))):
            raise ValueError("unordered GTK iteration attribution profiles")
        extended = [profile.get("attribution_version") == 2 for profile in profiles]
        if any(extended) and not all(extended):
            raise ValueError("mixed legacy and extended GTK iteration profiles")
        type_records = [parse_widget_type_profile(record["detail"])
                        for record in attribution_records
                        if record["surface"] == "app" and
                        record["event"] == "GTK_EVENT_ITERATION_WIDGET_TYPE"]
        instance_records = [parse_allocate_instance_profile(record["detail"])
                            for record in attribution_records
                            if record["surface"] == "app" and
                            record["event"] == "GTK_EVENT_ITERATION_ALLOCATE_INSTANCE"]
        if (type_records or instance_records) and not all(extended):
            raise ValueError("extended GTK attribution records without version 2 profile")
        per_iteration = []
        for duration, profile in zip(iterations, profiles):
            values = {name: profile[name] for name in ITERATION_PROFILE_FIELDS}
            item = {
                "iteration": profile["iteration"],
                "duration_ms": duration,
                "callbacks": values,
            }
            if all(extended):
                number = profile["iteration"]
                item["churn"] = {name: profile[name] for name in ITERATION_CHURN_FIELDS}
                item["widget_types"] = {
                    kind: {
                        entry["type"]: {
                            "count": entry["count"], "repeated": entry["repeated"]
                        }
                        for entry in type_records
                        if entry["iteration"] == number and entry["kind"] == kind
                    }
                    for kind in ("allocate", "style")
                }
                item["allocation_instances"] = [
                    entry for entry in instance_records if entry["iteration"] == number
                ]
            per_iteration.append(item)
        if all(extended):
            expected_iterations = set(range(len(iterations)))
            if ({entry["iteration"] for entry in type_records + instance_records}
                    - expected_iterations):
                raise ValueError("GTK attribution record has unknown iteration")
            type_keys = [(entry["iteration"], entry["kind"], entry["type"])
                         for entry in type_records]
            instance_keys = [(entry["iteration"], entry["id"])
                             for entry in instance_records]
            if len(type_keys) != len(set(type_keys)) or \
                    len(instance_keys) != len(set(instance_keys)):
                raise ValueError("duplicate GTK widget attribution record")
            for item in per_iteration:
                allocation_types = item["widget_types"]["allocate"]
                style_types = item["widget_types"]["style"]
                instances = item["allocation_instances"]
                churn = item["churn"]
                if len(instances) != churn["unique_allocated_widgets"]:
                    raise ValueError("inconsistent unique allocated widget attribution")
                checks = {
                    "repeated_allocations": sum(entry["repeated"] for entry in instances),
                    "identical_geometry_repeats": sum(entry["identical"] for entry in instances),
                    "changed_geometry_repeats": sum(entry["changed"] for entry in instances),
                    "repeated_style_updates": sum(
                        entry["repeated"] for entry in style_types.values()),
                }
                if churn["unique_style_widgets"] != sum(
                        entry["count"] - entry["repeated"]
                        for entry in style_types.values()):
                    raise ValueError("inconsistent unique styled widget attribution")
                if any(churn[name] != value for name, value in checks.items()):
                    raise ValueError("inconsistent GTK widget churn attribution")
                allocate_total = sum(entry["count"] for entry in allocation_types.values())
                style_total = sum(entry["count"] for entry in style_types.values())
                if allocate_total != sum(item["callbacks"][name] for name in (
                        "allocate_renderer", "allocate_paned", "allocate_notebook",
                        "allocate_other")) or style_total != sum(
                            item["callbacks"][name] for name in (
                                "style_renderer", "style_paned", "style_notebook",
                                "style_other")):
                    raise ValueError("inconsistent GTK widget type attribution")
        totals = {
            name: sum(item["callbacks"][name] for item in per_iteration)
            for name in ITERATION_PROFILE_FIELDS
        }
        attribution = {
            "totals": totals,
            "expensive_iterations": sorted(
                per_iteration, key=lambda item: item["duration_ms"], reverse=True
            )[:5],
            "per_iteration": per_iteration,
        }
        if all(extended):
            attribution["churn_totals"] = {
                name: sum(item["churn"][name] for item in per_iteration)
                for name in ITERATION_CHURN_FIELDS
            }
            attribution["widget_type_totals"] = {
                kind: {
                    widget_type: {
                        field: sum(
                            item["widget_types"][kind].get(widget_type, {}).get(field, 0)
                            for item in per_iteration
                        )
                        for field in ("count", "repeated")
                    }
                    for widget_type in sorted({
                        widget_type for item in per_iteration
                        for widget_type in item["widget_types"][kind]
                    })
                }
                for kind in ("allocate", "style")
            }
            session_profiles = [
                record for record in attribution_records
                if record["surface"] == "app" and
                record["event"] == "GTK_EVENT_DRAIN_SESSION_PROFILE"
            ]
            session_widgets = [
                record for record in attribution_records
                if record["surface"] == "app" and
                record["event"] == "GTK_EVENT_DRAIN_WIDGET_INSTANCE"
            ]
            if session_profiles or session_widgets:
                attribution["drain_session"] = drain_session_attribution(
                    session_profiles, session_widgets, per_iteration)
    lifecycle = [record for record in records
                 if drain_start <= record["timestamp_ms"] <= drain_end
                 and (record["surface"] != "app" or record["event"] in {
                     "WINDOW_REALIZE", "WINDOW_MAP", "WINDOW_STYLE_UPDATED",
                     "WINDOW_SIZE_ALLOCATE",
                 })]
    drain_duration = round(drain_end - drain_start, 1)
    iteration_total = round(sum(iterations), 1)
    result = {
        "phases_ms": phases,
        "iteration_count": len(iterations),
        "iteration_total_ms": iteration_total,
        "longest_iteration_ms": max(iterations, default=0.0),
        "drain_residual_ms": round(drain_duration - iteration_total, 1),
        "lifecycle": lifecycle,
    }
    if attribution is not None:
        result["attribution"] = attribution
    return result


def summarize_window_hotspots(directory: Path) -> dict[str, object]:
    scenarios: dict[str, object] = {}
    for scenario in ("fresh-profile", "reused-profile"):
        paths = sorted(directory.glob(f"{scenario}-*.log"))
        if len(paths) < 3:
            raise ValueError(f"at least three {scenario} logs are required in {directory}")
        samples = [window_hotspot_from_trace(path.read_text(encoding="utf-8"))
                   for path in paths]
        phase_names = list(samples[0]["phases_ms"])
        scenario_result = {
            "sample_count": len(samples),
            "metrics": {
                phase: metric([sample["phases_ms"][phase] for sample in samples])
                for phase in phase_names
            },
            "drain": {
                field: metric([float(sample[field]) for sample in samples])
                for field in ("iteration_count", "iteration_total_ms",
                              "longest_iteration_ms", "drain_residual_ms")
            },
            "samples": samples,
        }
        attributed = [sample["attribution"] for sample in samples
                      if "attribution" in sample]
        if attributed:
            if len(attributed) != len(samples):
                raise ValueError("mixed attributed and legacy GTK traces")
            scenario_result["callback_totals"] = {
                field: metric([float(sample["totals"][field])
                               for sample in attributed])
                for field in ITERATION_PROFILE_FIELDS
            }
            scenario_result["expensive_iterations"] = [
                {
                    "sample": path.name,
                    **item,
                    **({
                        "top_allocation_widget_types": sorted(
                            item["widget_types"]["allocate"].items(),
                            key=lambda entry: (-entry[1]["count"], entry[0]))[:5],
                        "top_style_widget_types": sorted(
                            item["widget_types"]["style"].items(),
                            key=lambda entry: (-entry[1]["count"], entry[0]))[:5],
                    } if "widget_types" in item else {}),
                }
                for path, sample in zip(paths, attributed)
                for item in sample["expensive_iterations"][:3]
            ]
            extended = ["churn_totals" in sample for sample in attributed]
            if any(extended) and not all(extended):
                raise ValueError("mixed legacy and extended GTK attribution samples")
            if all(extended):
                scenario_result["churn_totals"] = {
                    field: metric([float(sample["churn_totals"][field])
                                   for sample in attributed])
                    for field in ITERATION_CHURN_FIELDS
                }
                scenario_result["widget_types"] = {}
                for kind in ("allocate", "style"):
                    widget_types = sorted({
                        widget_type for sample in attributed
                        for widget_type in sample["widget_type_totals"][kind]
                    })
                    scenario_result["widget_types"][kind] = {
                        widget_type: {
                            field: metric([
                                float(sample["widget_type_totals"][kind].get(
                                    widget_type, {}).get(field, 0))
                                for sample in attributed
                            ])
                            for field in ("count", "repeated")
                        }
                        for widget_type in widget_types
                    }
                scenario_result["top_widget_types"] = {
                    kind: sorted(
                        scenario_result["widget_types"][kind].items(),
                        key=lambda entry: (-entry[1]["count"]["median_ms"], entry[0]),
                    )[:10]
                    for kind in ("allocate", "style")
                }
                session_flags = ["drain_session" in sample for sample in attributed]
                if any(session_flags) and not all(session_flags):
                    raise ValueError("mixed version 2 and drain-session GTK samples")
                if all(session_flags):
                    sessions = [sample["drain_session"] for sample in attributed]
                    scenario_result["drain_session_metrics"] = {
                        field: metric([float(session["metrics"][field])
                                       for session in sessions])
                        for field in DRAIN_SESSION_FIELDS
                    }
                    scenario_result["iteration_clusters"] = [
                        {"sample": path.name, **cluster}
                        for path, session in zip(paths, sessions)
                        for cluster in session["clusters"][:10]
                    ]
                    scenario_result["top_repeated_widgets"] = [
                        {"sample": path.name, **widget}
                        for path, session in zip(paths, sessions)
                        for widget in session["top_repeated_widgets"][:10]
                    ]
                    for kind in ("types", "subtrees"):
                        scenario_result[f"top_repeated_{kind}"] = [
                            {"sample": path.name, "name": name, **values}
                            for path, session in zip(paths, sessions)
                            for name, values in session[f"top_repeated_{kind}"][:10]
                        ]
        scenarios[scenario] = scenario_result
    return {"format_version": 1, "scenarios": scenarios}


def window_hotspot_markdown(summary: dict[str, object]) -> str:
    lines = ["# Window show and GTK event-drain profile", ""]
    for name, scenario in summary["scenarios"].items():
        lines.extend([
            f"## {name}", "", f"Samples: {scenario['sample_count']}.", "",
            "| Sub-phase | Median (ms) | MAD (ms) | Min (ms) | Max (ms) |",
            "|---|---:|---:|---:|---:|",
        ])
        for phase, values in scenario["metrics"].items():
            lines.append(
                f"| `{phase}` | {values['median_ms']:.1f} | "
                f"{values['mad_ms']:.1f} | {values['min_ms']:.1f} | "
                f"{values['max_ms']:.1f} |"
            )
        lines.extend(["", "| Drain measure | Median | MAD | Min | Max |",
                      "|---|---:|---:|---:|---:|"])
        for field, values in scenario["drain"].items():
            lines.append(
                f"| `{field}` | {values['median_ms']:.1f} | "
                f"{values['mad_ms']:.1f} | {values['min_ms']:.1f} | "
                f"{values['max_ms']:.1f} |"
            )
        lines.append("")
        if "callback_totals" in scenario:
            lines.extend([
                "| Callback/lifecycle total | Median | MAD | Min | Max |",
                "|---|---:|---:|---:|---:|",
            ])
            for field, values in scenario["callback_totals"].items():
                lines.append(
                    f"| `{field}` | {values['median_ms']:.1f} | "
                    f"{values['mad_ms']:.1f} | {values['min_ms']:.1f} | "
                    f"{values['max_ms']:.1f} |"
                )
            lines.append("")
            if "churn_totals" in scenario:
                lines.extend([
                    "| Widget churn per drain | Median | MAD | Min | Max |",
                    "|---|---:|---:|---:|---:|",
                ])
                for field, values in scenario["churn_totals"].items():
                    lines.append(
                        f"| `{field}` | {values['median_ms']:.1f} | "
                        f"{values['mad_ms']:.1f} | {values['min_ms']:.1f} | "
                        f"{values['max_ms']:.1f} |"
                    )
                lines.append("")
                for kind in ("allocate", "style"):
                    lines.extend([
                        f"### Widget types by {kind} count", "",
                        "| Widget type | Median count | MAD | Min | Max | Median repeats |",
                        "|---|---:|---:|---:|---:|---:|",
                    ])
                    ordered = sorted(
                        scenario["widget_types"][kind].items(),
                        key=lambda entry: (-entry[1]["count"]["median_ms"], entry[0]),
                    )
                    for widget_type, values in ordered:
                        counts = values["count"]
                        lines.append(
                            f"| `{widget_type}` | {counts['median_ms']:.1f} | "
                            f"{counts['mad_ms']:.1f} | {counts['min_ms']:.1f} | "
                            f"{counts['max_ms']:.1f} | "
                            f"{values['repeated']['median_ms']:.1f} |"
                        )
                    lines.append("")
                if "drain_session_metrics" in scenario:
                    lines.extend([
                        "### Cross-iteration drain session", "",
                        "| Session measure | Median | MAD | Min | Max |",
                        "|---|---:|---:|---:|---:|",
                    ])
                    for field, values in scenario["drain_session_metrics"].items():
                        lines.append(
                            f"| `{field}` | {values['median_ms']:.1f} | "
                            f"{values['mad_ms']:.1f} | {values['min_ms']:.1f} | "
                            f"{values['max_ms']:.1f} |"
                        )
                    lines.extend(["", "### Similar iteration widget sets", "",
                                  "| Sample | Left | Right | Intersection | Union | Jaccard |",
                                  "|---|---:|---:|---:|---:|---:|"])
                    for cluster in scenario["iteration_clusters"]:
                        lines.append(
                            f"| `{cluster['sample']}` | {cluster['left']} | "
                            f"{cluster['right']} | {cluster['intersection']} | "
                            f"{cluster['union']} | {cluster['similarity']:.3f} |"
                        )
                    lines.append("")
                    for kind in ("types", "subtrees"):
                        lines.extend([
                            f"### Top repeated {kind}", "",
                            "| Sample | Name | Iterations | Callbacks | Identical | Changed |",
                            "|---|---|---:|---:|---:|---:|",
                        ])
                        for entry in scenario[f"top_repeated_{kind}"]:
                            lines.append(
                                f"| `{entry['sample']}` | `{entry['name']}` | "
                                f"{entry['iterations_touched']} | "
                                f"{entry['allocation_count']} | {entry['identical']} | "
                                f"{entry['changed']} |"
                            )
                        lines.append("")
                    lines.extend([
                        "### Top repeated widgets", "",
                        "| Sample | Widget | Type | Path | Iterations | Callbacks | Identical | Changed |",
                        "|---|---|---|---|---:|---:|---:|---:|",
                    ])
                    for entry in scenario["top_repeated_widgets"]:
                        lines.append(
                            f"| `{entry['sample']}` | `{entry['id']}` | "
                            f"`{entry['type']}` | `{entry['path']}` | "
                            f"{entry['allocate_iterations']} | {entry['allocate_count']} | "
                            f"{entry['allocate_identical']} | {entry['allocate_changed']} |"
                        )
                    lines.append("")
            lines.extend([
                "| Sample | Iteration | Duration (ms) | Attribution |",
                "|---|---:|---:|---|",
            ])
            for item in scenario["expensive_iterations"]:
                if "churn" in item:
                    allocation_types = ", ".join(
                        f"{widget_type}={values['count']}"
                        for widget_type, values in item["top_allocation_widget_types"]
                    ) or "none"
                    style_types = ", ".join(
                        f"{widget_type}={values['count']}"
                        for widget_type, values in item["top_style_widget_types"]
                    ) or "none"
                    churn = item["churn"]
                    callbacks = item["callbacks"]
                    evidence = (
                        f"allocation types: {allocation_types}; style types: {style_types}; "
                        f"unique allocated={churn['unique_allocated_widgets']}; "
                        f"repeated allocations={churn['repeated_allocations']}; "
                        f"identical geometry={churn['identical_geometry_repeats']}; "
                        f"changed geometry={churn['changed_geometry_repeats']}; "
                        f"repeated style={churn['repeated_style_updates']}; "
                        f"allocate_paned={callbacks['allocate_paned']}; "
                        f"allocate_notebook={callbacks['allocate_notebook']}; "
                        f"root_draw_us={callbacks['root_draw_us']}"
                    )
                    if "cross_iteration" in item:
                        cross = item["cross_iteration"]
                        def overlap_text(value):
                            return ("none" if value is None else
                                    f"{value['intersection']}/{value['union']} "
                                    f"({value['similarity']:.3f})")
                        evidence += (
                            f"; allocated previously={cross['allocated_previously']}; "
                            f"same/changed geometry={cross['same_geometry_as_previous']}/"
                            f"{cross['changed_geometry_from_previous']}; "
                            f"previous/next expensive overlap="
                            f"{overlap_text(cross['overlap_previous_expensive'])}/"
                            f"{overlap_text(cross['overlap_next_expensive'])}; "
                            f"dominant subtree={cross['dominant_subtree']}"
                        )
                else:
                    evidence = ", ".join(
                        f"{field}={value}"
                        for field, value in item["callbacks"].items() if value
                    ) or "none observed"
                lines.append(
                    f"| `{item['sample']}` | {item['iteration']} | "
                    f"{item['duration_ms']:.1f} | {evidence} |"
                )
            lines.append("")
    return "\n".join(lines)


def window_profile_markdown(summary: dict[str, object]) -> str:
    lines = ["# GTK initialization to window creation profile", ""]
    for name, scenario in summary["scenarios"].items():
        lines.extend([
            f"## {name}", "", f"Samples: {scenario['sample_count']}.", "",
            "| Sub-phase | Median (ms) | MAD (ms) | Min (ms) | Max (ms) |",
            "|---|---:|---:|---:|---:|",
        ])
        for phase, values in scenario["metrics"].items():
            lines.append(
                f"| `{phase}` | {values['median_ms']:.1f} | "
                f"{values['mad_ms']:.1f} | {values['min_ms']:.1f} | "
                f"{values['max_ms']:.1f} |"
            )
        lines.append("")
    return "\n".join(lines)


def summarize(directory: Path) -> dict[str, object]:
    scenarios: dict[str, object] = {}
    for scenario in ("fresh-profile", "reused-profile"):
        samples = []
        paths = sorted(directory.glob(f"{scenario}-*.log"))
        if len(paths) < 3:
            raise ValueError(
                f"at least three {scenario} logs are required in {directory}"
            )
        for path in paths:
            sample = sample_from_trace(path.read_text(encoding="utf-8"))
            sample["log"] = path.name
            samples.append(sample)
        phase_names = list(samples[0]["phases_ms"])
        metrics = {
            phase: metric([sample["phases_ms"][phase] for sample in samples])
            for phase in phase_names
        }
        startup_phases = phase_names[:-1]
        dominant = max(startup_phases, key=lambda phase: metrics[phase]["median_ms"])
        scenarios[scenario] = {
            "sample_count": len(samples),
            "dominant_phase": dominant,
            "metrics": metrics,
            "samples": samples,
        }
    return {
        "format_version": 1,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "scenarios": scenarios,
    }


def markdown(summary: dict[str, object]) -> str:
    lines = ["# Startup performance baseline", ""]
    for name, scenario in summary["scenarios"].items():
        lines.extend(
            [
                f"## {name}",
                "",
                f"Samples: {scenario['sample_count']}. Dominant median phase: "
                f"`{scenario['dominant_phase']}`.",
                "",
                "| Phase | Median (ms) | MAD (ms) | Min (ms) | Max (ms) |",
                "|---|---:|---:|---:|---:|",
            ]
        )
        for phase, values in scenario["metrics"].items():
            lines.append(
                f"| `{phase}` | {values['median_ms']:.1f} | "
                f"{values['mad_ms']:.1f} | {values['min_ms']:.1f} | "
                f"{values['max_ms']:.1f} |"
            )
        lines.append("")
    return "\n".join(lines)


def make_profile(root: Path) -> dict[str, str]:
    home = root / "home"
    environment = {
        "HOME": str(home),
        "XDG_CONFIG_HOME": str(root / "config"),
        "XDG_DATA_HOME": str(root / "data"),
        "XDG_CACHE_HOME": str(root / "cache"),
        "XDG_STATE_HOME": str(root / "state"),
    }
    for path in environment.values():
        Path(path).mkdir(parents=True, exist_ok=True)
    return environment


def run_app(args: argparse.Namespace, profile: Path, log: Path) -> None:
    environment = os.environ.copy()
    environment.update(make_profile(profile))
    environment.update(
        {
            "GDK_BACKEND": "x11",
            "BIBLIA_ELIM_GTK_LIFECYCLE_SMOKE": "1",
            "BIBLIA_ELIM_UI_LOAD_DEBUG": "1",
        }
    )
    command = [str(args.app), f"--backend=sqlite:{args.output / 'modules'}"]
    if args.xvfb_run:
        command = [str(args.xvfb_run), "-a", "-s", "-screen 0 1280x800x24"] + command
    result = subprocess.run(
        command, env=environment, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, timeout=args.timeout, check=False
    )
    log.write_text(result.stdout, encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(f"startup exited {result.returncode}; retained {log}")
    parse_trace(result.stdout)


def collect(args: argparse.Namespace) -> None:
    if args.runs < 3:
        raise ValueError("at least three measured runs are required")
    args.output.mkdir(parents=True, exist_ok=False)
    module_dir = args.output / "modules"
    module_dir.mkdir()
    revision = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=Path(__file__).parents[1],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=False
    )
    status = subprocess.run(
        ["git", "status", "--short"], cwd=Path(__file__).parents[1],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=False
    )
    manifest = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "app": str(args.app.resolve()),
        "fixture": str(args.fixture.resolve()),
        "runs_per_scenario": args.runs,
        "display_path": str(args.xvfb_run) if args.xvfb_run else "current X11 display",
        "platform": platform.platform(),
        "git_revision": revision.stdout.strip() if revision.returncode == 0 else "unknown",
        "git_status_short": status.stdout.splitlines() if status.returncode == 0 else [],
    }
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    with args.fixture.open("rb") as source:
        fixture = subprocess.run(
            [str(args.sqlite3), str(module_dir / "baseline.sqlite")],
            stdin=source, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False
        )
    if fixture.returncode != 0:
        raise RuntimeError(f"could not create fixture: {fixture.stderr.decode()}")

    profiles = args.output / "profiles"
    for number in range(1, args.runs + 1):
        run_app(
            args, profiles / f"fresh-{number}",
            args.output / f"fresh-profile-{number:02d}.log"
        )

    reused = profiles / "reused"
    run_app(args, reused, args.output / "reused-profile-setup.log")
    (args.output / "reused-profile-setup.log").rename(args.output / "profile-setup.log")
    for number in range(1, args.runs + 1):
        run_app(
            args, reused, args.output / f"reused-profile-{number:02d}.log"
        )

    result = summarize(args.output)
    (args.output / "summary.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    (args.output / "summary.md").write_text(markdown(result), encoding="utf-8")
    shutil.rmtree(profiles)
    print(markdown(result))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    analyze = subparsers.add_parser("analyze", help="summarize retained trace logs")
    analyze.add_argument("directory", type=Path)
    profile = subparsers.add_parser(
        "profile-window", help="profile GTK_INITIALIZED through WINDOW_CREATED"
    )
    profile.add_argument("directory", type=Path)
    hotspots = subparsers.add_parser(
        "profile-window-hotspots", help="profile window show and GTK event drain"
    )
    hotspots.add_argument("directory", type=Path)
    collector = subparsers.add_parser("collect", help="run the instrumented application")
    collector.add_argument("--app", required=True, type=Path)
    collector.add_argument("--fixture", required=True, type=Path)
    collector.add_argument("--sqlite3", default="sqlite3", type=Path)
    collector.add_argument("--xvfb-run", type=Path)
    collector.add_argument("--output", required=True, type=Path)
    collector.add_argument("--runs", default=7, type=int)
    collector.add_argument("--timeout", default=35, type=int)
    args = parser.parse_args()
    try:
        if args.command == "collect":
            collect(args)
        elif args.command == "analyze":
            result = summarize(args.directory)
            print(markdown(result))
        elif args.command == "profile-window":
            result = summarize_window_profile(args.directory)
            print(window_profile_markdown(result))
        else:
            result = summarize_window_hotspots(args.directory)
            print(window_hotspot_markdown(result))
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"startup baseline failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
