import os
import subprocess
import sys
import platform

# Get the directory of the current script
tools_dir = os.path.dirname(os.path.abspath(__file__))
vcpkg_dir = os.path.join(tools_dir, "vcpkg")

# Check if vcpkg directory exists, if not, clone it
if not os.path.isdir(vcpkg_dir):
    print("Cloning vcpkg from GitHub...")
    subprocess.check_call(["git", "clone", "https://github.com/Microsoft/vcpkg.git", vcpkg_dir])


system_platform = platform.system()

if system_platform == "Windows":
    bootstrap_script = os.path.join(vcpkg_dir, "bootstrap-vcpkg.bat")
else:
    bootstrap_script = os.path.join(vcpkg_dir, "bootstrap-vcpkg.sh")

# Install vcpkg
print("Installing vcpkg...")
subprocess.check_call([bootstrap_script, "-disableMetrics"])