import subprocess

def get_git_hash():
    try:
        result = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], stderr=subprocess.STDOUT)
        return result.strip().decode('utf-8')
    except (subprocess.CalledProcessError, FileNotFoundError, Exception) as e:
        print("Warning: Could not get git hash: %s" % str(e))
        return "nogit"

Import("env")

git_hash = get_git_hash()
print("=== extra_script.py: Git hash is: %s ===" % git_hash)

# Define FW_BUILD_HASH using BUILD_FLAGS to ensure it works
# Format matches platformio.ini: -DMBEDTLS_CONFIG_FILE=\"mbedtls_config.h\"
# The \\" in Python becomes \" in the compiler flag, which defines a string literal
build_flag = '-DFW_BUILD_HASH=\\"%s\\"' % git_hash
env.Append(BUILD_FLAGS=[build_flag])
print("=== Added build flag: %s ===" % build_flag)
