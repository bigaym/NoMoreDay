import os
import subprocess
import json

def get_git_status():
    try:
        status = subprocess.check_output(["git", "status", "--short"], encoding="utf-8")
        return status if status else "Clean"
    except:
        return "Unknown"

def get_current_track():
    track_path = "conductor/tracks.md"
    if os.path.exists(track_path):
        with open(track_path, "r", encoding="utf-8") as f:
            lines = f.readlines()
            for line in lines:
                if "[active]" in line or "[in-progress]" in line:
                    return line.strip()
    return "No active track"

def check_build_artifacts():
    tests_runner = "build/bin/tests/tests_runner.exe"
    exists = os.path.exists(tests_runner)
    return "Ready" if exists else "Missing (Build Required)"

def main():
    print("=== NoMoreDay Awareness Sync ===")
    print(f"Git Status: {get_git_status()}")
    print(f"Active Track: {get_current_track()}")
    print(f"Build State: {check_build_artifacts()}")
    print("===============================")

if __name__ == "__main__":
    main()
