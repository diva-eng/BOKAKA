import subprocess
from datetime import datetime, timezone

def get_git_hash():
    try:
        result = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], stderr=subprocess.STDOUT)
        return result.strip().decode('utf-8')
    except (subprocess.CalledProcessError, FileNotFoundError, Exception) as e:
        print("Warning: Could not get git hash: %s" % str(e))
        return "nogit"

def get_build_datetime():
    # Use ISO 8601 format without spaces for cross-platform compatibility
    # Format: "2026-01-02T15:30:45Z" (UTC time for reproducibility)
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

Import("env")

git_hash = get_git_hash()
build_datetime = get_build_datetime()

print("=== extra_script.py: Git hash is: %s ===" % git_hash)
print("=== extra_script.py: Build datetime is: %s ===" % build_datetime)

# Define build metadata using CPPDEFINES for proper cross-platform escaping
# SCons handles the quoting automatically when using tuple format (name, value)
env.Append(CPPDEFINES=[
    ("FW_BUILD_HASH", env.StringifyMacro(git_hash)),
    ("FW_BUILD_DATETIME", env.StringifyMacro(build_datetime)),
])

print("=== Added FW_BUILD_HASH and FW_BUILD_DATETIME defines ===")
