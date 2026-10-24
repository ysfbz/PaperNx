#!/usr/bin/env python3
"""Generate the protected corpus, run PaperNxApp cold/warm three times, and keep raw evidence."""
import argparse
import json
import math
import os
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
METRICS = ("initMs", "libraryScanMs", "startupToFirstFrameMs", "firstViewportReadyMs",
           "fastScrollMs", "fullLibrarySweepMs", "categoryChangeMs", "openReturnMs",
           "cacheClearReloadMs", "peakRssKb")


def distribution(values):
    if not values:
        return {"n": 0, "median": None, "p95": None, "p99": None,
                "method": "nearest-rank; no qualifying samples"}
    ordered = sorted(values)
    def nearest_rank(percent):
        return ordered[max(0, math.ceil(percent * len(ordered)) - 1)]
    return {"n": len(ordered), "median": statistics.median(ordered),
            "p95": nearest_rank(.95), "p99": nearest_rank(.99),
            "method": "nearest-rank; pXX index=ceil(XX*n)-1"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, default=3)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("output exists; choose a new evidence folder to preserve prior results")
    args.binary = args.binary.resolve()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True)
    expected_source = json.loads(subprocess.check_output(
        [sys.executable, str(ROOT / "scripts/source_identity.py"), "--format", "json", "--root", str(ROOT)], text=True))
    benchmark_env = os.environ.copy()
    benchmark_env["SDL_VIDEODRIVER"] = "dummy"
    benchmark_env["SDL_RENDER_DRIVER"] = "software"
    corpus = args.output / "corpus"
    subprocess.run(["python3", str(ROOT / "tests/generate_cover_benchmark_corpus.py"), str(corpus), "--count", "1000"], check=True)
    manifest = json.loads((corpus / "manifest.json").read_text(encoding="utf-8"))
    all_results = {}
    for count in (100, 500, 1000):
        dataset = args.output / f"dataset-{count}"
        dataset.mkdir()
        for source in sorted(corpus.iterdir()):
            if source.suffix.lower() in (".epub", ".pdf", ".cbz", ".cbr"):
                ordinal = int(source.stem.split("-")[-1])
                if ordinal < count:
                    os.link(source, dataset / source.name)
        for run in range(1, args.repetitions + 1):
            app_root = args.output / f"app-{count}-run-{run}"
            for mode in ("cold", "warm"):
                raw = args.output / f"raw-{count}-{mode}-{run}.json"
                label = f"generated-mixed-v1-{count}"
                subprocess.run([str(args.binary), str(app_root), str(dataset), label, mode, str(raw)],
                               cwd=ROOT, env=benchmark_env, check=True, timeout=300)
                report = json.loads(raw.read_text(encoding="utf-8"))
                all_results.setdefault(str(count), {}).setdefault(mode, []).append(report)

    all_runs = [run for modes in all_results.values() for runs in modes.values() for run in runs]
    source_records = [run["finalTelemetry"] for run in all_runs]
    source_keys = {(item["sourceCommit"], item["sourceDirty"], item["sourceTreeSha256"], item["sourceIdentity"], item["appVersion"])
                   for item in source_records}
    renderers = {run["renderer"] for run in all_runs}
    if len(source_keys) != 1 or len(renderers) != 1:
        raise RuntimeError("Benchmark runs used inconsistent source identities or renderers")
    if renderers != {"software"}:
        raise RuntimeError(f"Expected the SDL dummy software renderer, got {sorted(renderers)}")
    source_commit, source_dirty, source_tree, source_identity, app_version = next(iter(source_keys))
    expected_key = (expected_source["git_revision"], str(expected_source["dirty"]).lower(),
                    expected_source["source_tree_sha256"], expected_source["source_identity"], app_version)
    if (source_commit, source_dirty, source_tree, source_identity, app_version) != expected_key:
        raise RuntimeError("Compiled benchmark telemetry differs from the current source identity")
    if source_identity == "unspecified" or source_tree == "unspecified":
        raise RuntimeError("Benchmark telemetry lacks a source identity")
    unique_content = len({entry["sha256"] for entry in manifest["books"]})
    manifest_summary = {key: value for key, value in manifest.items() if key != "books"}
    manifest_summary["uniqueContentCount"] = unique_content
    manifest_summary["repeatedContentRatio"] = (manifest["count"] - unique_content) / manifest["count"]
    summary = {"schema": "papernx-cover-benchmark-summary-1", "repetitionsPerMode": args.repetitions,
               "source": {"appVersion": app_version, "sourceCommit": source_commit, "sourceDirty": source_dirty,
                          "sourceTreeSha256": source_tree, "sourceIdentity": source_identity},
               "renderer": next(iter(renderers)),
               "sampleMethod": "nearest-rank; p95/p99 use ceil(p*n)-1; repeated fixture rate is derived from SHA-256 content identities",
               "dataset": manifest_summary, "results": {}}
    for count, modes in all_results.items():
        summary["results"][count] = {}
        for mode, runs in modes.items():
            metrics = {metric: distribution([run[metric] for run in runs]) for metric in METRICS}
            frames = [sample for run in runs for sample in run["finalTelemetry"]["frameTimeMs"]]
            decodes = [sample for run in runs for sample in run["finalTelemetry"]["thumbnailDecodeMs"]]
            waits = [sample for run in runs for sample in run["finalTelemetry"]["uploadQueueWaitMs"]]
            counters = {}
            for key in ("metadataDocumentOpens", "metadataOpensAvoided", "thumbnailDocumentOpens", "archiveOpens",
                        "diskCacheHits", "diskCacheMisses", "successfulTextureUploads", "uploadFailures",
                        "discardedStaleResults", "coverRequests"):
                counters[key] = distribution([run["finalTelemetry"]["counters"][key] for run in runs])
            first = [run["firstViewportTelemetry"] for run in runs]
            summary["results"][count][mode] = {
                "metrics": metrics, "countersAcrossRuns": counters,
                "thumbnailDecodeMs": distribution(decodes), "frameTimeMs": distribution(frames), "uploadQueueWaitMs": distribution(waits),
                "viewport": {"books": [run["sampleCount"] for run in runs],
                             "texturesLoaded": [run["initialViewportTextures"] for run in runs],
                             "firstVisibleMs": distribution([item["lastEpoch"]["firstVisibleMs"] for item in first if item["lastEpoch"]["firstVisibleMs"] is not None]),
                             "fill50Ms": distribution([item["lastEpoch"]["fill50Ms"] for item in first if item["lastEpoch"]["fill50Ms"] is not None]),
                             "fill95Ms": distribution([item["lastEpoch"]["fill95Ms"] for item in first if item["lastEpoch"]["fill95Ms"] is not None]),
                             "frameSamples": len(frames), "uploadWaitSamples": len(waits)},
                "rawFiles": [f"raw-{count}-{mode}-{index + 1}.json" for index in range(len(runs))],
            }
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    with (args.output / "summary.md").open("w", encoding="utf-8") as report:
        report.write("# PaperNX app-level cover benchmark\n\n")
        report.write(f"Corpus: {manifest['count']} generated documents; {manifest['countsByFormat']}; unique content: {manifest['uniqueContentByFormat']}; overall repeated-content ratio: {summary['dataset']['repeatedContentRatio']:.2%}.\n\n")
        report.write(f"Source: `{source_commit}` (`{source_identity}`); app {app_version}; renderer `{summary['renderer']}`.\n\n")
        report.write("Generated EPUB, PDF and CBZ documents are distinct deterministic content. CBR files use five original synthetic fixtures; the overall repeated-content ratio is included in `summary.json`. The application was run with SDL dummy/software renderer in the desktop container; this is not Switch hardware evidence.\n\n")
        report.write("All metric distributions use nearest-rank and preserve per-run raw JSON beside this report. Decode, frame and upload-wait distributions pool their final bounded rings (256 entries each) from every repetition.\n\n")
        report.write("| Books | Mode | Startup median/p95/p99 ms | Scan median ms | First viewport median ms | Sweep median ms | Frame p50/p95/p99 ms | Disk hits/misses per run | Peak RSS median KiB |\n|---:|---|---:|---:|---:|---:|---:|---:|---:|\n")
        for count, modes in summary["results"].items():
            for mode, result in modes.items():
                m=result["metrics"]
                frame=result["frameTimeMs"]
                hits=result["countersAcrossRuns"]["diskCacheHits"]["median"]
                misses=result["countersAcrossRuns"]["diskCacheMisses"]["median"]
                report.write(f"| {count} | {mode} | {m['startupToFirstFrameMs']['median']:.2f} / {m['startupToFirstFrameMs']['p95']:.2f} / {m['startupToFirstFrameMs']['p99']:.2f} | {m['libraryScanMs']['median']:.2f} | {m['firstViewportReadyMs']['median']:.2f} | {m['fullLibrarySweepMs']['median']:.2f} | {frame['median']:.2f} / {frame['p95']:.2f} / {frame['p99']:.2f} | {hits:g} / {misses:g} | {m['peakRssKb']['median']:.0f} |\n")
    print(args.output / "summary.json")


if __name__ == "__main__":
    main()
