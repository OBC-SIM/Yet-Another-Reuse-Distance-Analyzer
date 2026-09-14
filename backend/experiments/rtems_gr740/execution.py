"""Run isolated processes while preserving commands, failures and raw output."""
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import time


def write_json(path, value):
    """Write readable experiment metadata with real newlines."""
    Path(path).write_text(json.dumps(value, indent=2) + "\n")


def read_json(path):
    """Read one saved input or diagnostic document."""
    return json.loads(Path(path).read_text())


def sha(path):
    """Hash file bytes without retaining large ELF or event files in memory."""
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run(command, prefix, cwd=None, timeout=180):
    """Execute an argv vector; on timeout kill its entire process group."""
    prefix = Path(prefix)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    command = [str(item) for item in command]
    start = time.monotonic_ns()
    with Path(str(prefix) + ".stdout").open("w") as stdout, \
         Path(str(prefix) + ".stderr").open("w") as stderr:
        process = subprocess.Popen(command, cwd=cwd, stdout=stdout, stderr=stderr,
                                   start_new_session=True)
        try:
            code = process.wait(timeout=timeout)
            status = "completed"
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            code = process.wait()
            status = "timeout"
    record = dict(command=command, cwd=str(cwd or Path.cwd()), exit_code=code,
                  status=status, wall_time_ns=time.monotonic_ns() - start)
    write_json(str(prefix) + ".command.json", record)
    return record


def checked(command, prefix, **kwargs):
    """Run a required successful command, leaving evidence before any exception."""
    result = run(command, prefix, **kwargs)
    if result["exit_code"] != 0:
        raise RuntimeError(f"command failed ({result['exit_code']}): {prefix}")
    return result


def cli_command(binary, row, destination):
    """Build the actual public CLI command with the case's exact allowances."""
    command = [binary, row["lat_path"], "--analysis", "hierarchy-rd", "--elf",
               row["elf_path"], "--cache", row["cache_path"], "--export", destination]
    for key, flag in (("single_loop", "single-loop-iterations"),
                      ("cumulative_loop", "cumulative-loop-iterations"),
                      ("source_accesses", "source-accesses"),
                      ("line_references", "line-references")):
        command += [f"--max-{flag}", str(row["effective_work_limits"][key])]
    return command
