#!/usr/bin/env python3
"""Validate console evidence from the real HelloHosts browser self-test.

Capture logs through the browser developer API; this tool does not launch a
browser or substitute mocked output for target execution.
"""
import argparse
import json
from pathlib import Path
import re


def verify(logs):
    errors = [item["message"] for item in logs if item.get("level") == "error"]
    if errors:
        raise ValueError("Browser runtime errors: " + " | ".join(errors))
    messages = [item["message"] for item in logs]
    ready = next((i for i, line in enumerate(messages) if line.startswith("HOST_SMOKE_READY ")), None)
    passed = next((i for i, line in enumerate(messages) if line.startswith("HOST_SMOKE_OK ")), None)
    closed = next((i for i, line in enumerate(messages) if line == "HOST_SMOKE_CLOSED"), None)
    if ready is None or passed is None or closed is None or not ready < passed < closed:
        raise ValueError("Expected ordered READY, OK, CLOSED from the real browser")
    line = messages[passed]
    fields = dict(re.findall(r"(\w+)=(\S+)", line))
    expected = {"backend": "6", "timer": "true", "background": "42", "resize": "true", "synced": "1", "capture": "true"}
    if any(fields.get(key) != value for key, value in expected.items()) or int(fields.get("frames", "0")) <= 0:
        raise ValueError("Browser did not complete every host check")
    return {"status": "browser_self_test_passed", "checks": fields,
            "normalCloseObserved": True, "consoleErrors": [], "messages": messages}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("console_json", type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(json.loads(args.console_json.read_text())), indent=2))


if __name__ == "__main__":
    main()
