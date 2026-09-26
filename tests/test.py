import subprocess
import sys

result = subprocess.run(["./build/ifj26"])

if result.returncode == 0:
    print("PASSED: program returned 0")
    sys.exit(0)
else:
    print("FAILED: program returned {result.returncode}")
    sys.exit(1)