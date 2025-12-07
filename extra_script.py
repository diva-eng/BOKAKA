import subprocess

def get_git_hash():
    try:
        return subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).strip().decode()
    except:
        return "nogit"

env = DefaultEnvironment()
env.Execute("echo Collecting git hash...")
env["GIT_COMMIT_HASH"] = get_git_hash()
