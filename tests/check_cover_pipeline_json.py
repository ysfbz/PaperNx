import json
import subprocess
import sys

result = subprocess.run([sys.argv[1]], check=True, capture_output=True, text=True)
report = json.loads(result.stdout)
assert report["sourceCommit"] == "abcdef0123456789"
assert report["dataset"] == "generated-documents-v1"
assert report["runType"] == "cold"
assert report["renderer"] == "software"
assert report["cacheBudgetBytes"] == 0
assert report["cacheEntryBudget"] == 0
assert report["counters"]["coverRequests"] == 303
assert report["counters"]["uploadFailures"] == 1
assert report["counters"]["outstandingRequests"] == 0
assert report["lastEpoch"]["firstVisibleMs"] is not None
assert report["lastEpoch"]["fill50Ms"] is not None
assert report["lastEpoch"]["fill95Ms"] is not None
assert len(report["uploadQueueWaitMs"]) == 256
assert abs(report["uploadQueueWaitMs"][0] - 0.44) < 1e-6
assert abs(report["uploadQueueWaitMs"][-1] - 2.99) < 1e-6
assert len(report["thumbnailDecodeMs"]) == 256
assert len(report["frameTimeMs"]) == 256
assert "book-alpha" not in result.stdout and "book-failed" not in result.stdout
