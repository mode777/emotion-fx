#!/usr/bin/env python3
"""Pre-CI verification on the Linux SSH verification server.

Policy (AGENTS.md / docs/verification-server.md): run the golden + smoke
suites here BEFORE dispatching the GitHub Actions gate. If this fails, do
not dispatch `gh workflow run ci.yml`.

Usage:
    python3 tools/verify_remote.py native [ref]
    python3 tools/verify_remote.py web    [ref]
    python3 tools/verify_remote.py all    [ref]

    ref   branch name on origin to verify (default: the current branch).
          The ref must be pushed; the normal flow is:
          commit -> push branch -> verify_remote.py -> dispatch CI.

Options:
    --dir DIR   remote checkout path (default: ~/emotion-fx)
    --jobs N    parallel build jobs (default: nproc on the server)

Credentials are read from the environment only: SSH_HOST, SSH_USER,
SSH_PASSWORD. They are never written, logged, or stored anywhere in the
repository. Requires paramiko (pip3 install --user paramiko).

Exit code 0 means every requested suite passed.
"""

import argparse
import os
import shlex
import subprocess
import sys
import time

NATIVE_TIMEOUT = 1800  # configure + build + ctest under Xvfb
WEB_TIMEOUT = 3600     # emcmake + build + chrome-headless-shell run


def die(msg, code=2):
    print(f"verify_remote: {msg}", file=sys.stderr)
    sys.exit(code)


def git(*args):
    r = subprocess.run(["git", *args], capture_output=True, text=True)
    if r.returncode != 0:
        die(f"git {' '.join(args)} failed: {r.stderr.strip()}")
    return r.stdout.strip()


def require_env():
    missing = [k for k in ("SSH_HOST", "SSH_USER", "SSH_PASSWORD")
               if not os.environ.get(k)]
    if missing:
        die("missing env vars: " + ", ".join(missing))


def connect():
    host = os.environ["SSH_HOST"]
    user = os.environ["SSH_USER"]
    password = os.environ["SSH_PASSWORD"]
    try:
        import paramiko
    except ImportError:
        die("paramiko not installed: pip3 install --user paramiko")
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(host, username=user, password=password, timeout=20,
                   banner_timeout=20, auth_timeout=20)
    return client


def run(ssh, cmd, label, timeout):
    """Stream a merged-output remote command; return its exit status."""
    print(f"--- [{label}] {cmd if len(cmd) <= 120 else cmd[:117] + '...'}")
    merged = f"bash -o pipefail -c {shlex.quote(cmd)} 2>&1"
    chan = ssh.get_transport().open_session(timeout=timeout)
    chan.settimeout(timeout)
    chan.exec_command(merged)
    out = []
    deadline = time.monotonic() + timeout
    while True:
        while chan.recv_ready():
            data = chan.recv(8192).decode(errors="replace")
            out.append(data)
            print(data, end="")
        if chan.exit_status_ready() and not chan.recv_ready():
            break
        if time.monotonic() > deadline:
            chan.close()
            print(f"\n--- [{label}] TIMEOUT after {timeout}s", file=sys.stderr)
            return -1
        time.sleep(0.05)
    rc = chan.recv_exit_status()
    chan.close()
    print(f"--- [{label}] exit {rc}")
    return rc


def resolve_ref(ref_arg):
    """Return the pushed SHA for ref (default: current branch), or die."""
    remote_url = git("remote", "get-url", "origin")
    if ref_arg:
        branch = ref_arg
    else:
        branch = git("rev-parse", "--abbrev-ref", "HEAD")
        if branch == "HEAD":
            die("detached HEAD; pass a pushed branch name explicitly")
    ls = git("ls-remote", "--heads", "origin", branch)
    if not ls:
        die(f"branch '{branch}' is not pushed to origin; "
            f"push it first (git push -u origin {branch})")
    sha = ls.split()[0]
    print(f"verifying origin/{branch} @ {sha} ({remote_url})")
    local = git("rev-parse", "HEAD")
    if local != sha and branch == git("rev-parse", "--abbrev-ref", "HEAD"):
        print(f"NOTE: local HEAD {local[:12]} differs from origin/{branch}; "
              "verifying the pushed state, not your working tree")
    return remote_url, sha


def sync_cmd(url, sha, remote_dir):
    # Bootstrap a bare-enough checkout if the path was never used.
    have = f"test -d {remote_dir}/.git"
    clone = f"git clone --filter=blob:none {shlex.quote(url)} {remote_dir}"
    sync = (f"cd {remote_dir} && git fetch origin && "
            f"git checkout -q --detach {sha} && git reset -q --hard {sha} "
            f"&& git log --oneline -1")
    return f"{have} || {clone}; {sync}"


NATIVE_STEPS = [
    ("configure",
     "cmake -B build -DEFX_BUILD_DEV_HARNESS=ON -DEFX_BUILD_GOLDEN_TESTS=ON"
     " -DCMAKE_BUILD_TYPE=Release",
     300),
    ("build", "cmake --build build -j{JOBS}", 1200),
    # Canonical golden config: Xvfb display + Mesa llvmpipe (ADR 0020).
    ("test",
     "xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1"
     " ctest --test-dir build -C Release --output-on-failure",
     600),
]

WEB_STEPS = [
    ("bootstrap native tools",
     "test -f build-golden-tools/tests/efx_imgdiff ||"
     " (cmake -B build-golden-tools -DEFX_BUILD_GOLDEN_TESTS=ON"
     " -DCMAKE_BUILD_TYPE=Release &&"
     " cmake --build build-golden-tools --target efx_imgdiff -j{JOBS})",
     900),
    ("configure capture build",
     "source /opt/emsdk/emsdk_env.sh >/dev/null &&"
     " emcmake cmake -B build-web-golden -DEFX_BUILD_GOLDEN_TESTS=ON"
     " -DCMAKE_BUILD_TYPE=Release",
     300),
    ("build capture player",
     "source /opt/emsdk/emsdk_env.sh >/dev/null &&"
     " cmake --build build-web-golden --target player_web_golden -j{JOBS}",
     1800),
    ("ensure chrome-headless-shell@131",
     "CHROME_BIN=$(ls ~/browsers/chrome-headless-shell/*/"
     "chrome-headless-shell-*/chrome-headless-shell 2>/dev/null | head -1);"
     ' if [ -z "$CHROME_BIN" ]; then npx --yes @puppeteer/browsers install'
     " chrome-headless-shell@131 --path ~/browsers;"
     " CHROME_BIN=$(ls ~/browsers/chrome-headless-shell/*/"
     "chrome-headless-shell-*/chrome-headless-shell | head -1); fi;"
     ' echo "chrome-shell: $CHROME_BIN"',
     600),
    ("ensure node deps",
     "test -d node_modules/puppeteer-core ||"
     " npm install --no-save puppeteer-core @puppeteer/browsers",
     600),
    ("web golden tests",
     'CHROME_SHELL_PATH=$(ls ~/browsers/chrome-headless-shell/*/'
     'chrome-headless-shell-*/chrome-headless-shell | head -1)'
     " node tools/run_web_goldens.mjs",
     900),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("suite", choices=["native", "web", "all"])
    ap.add_argument("ref", nargs="?", default=None)
    ap.add_argument("--dir", default="~/emotion-fx")
    ap.add_argument("--jobs", type=int, default=0,
                    help="build parallelism (default: server nproc)")
    args = ap.parse_args()

    require_env()
    url, sha = resolve_ref(args.ref)
    ssh = connect()
    try:
        rc = run(ssh, sync_cmd(url, sha, args.dir), "sync", 600)
        if rc != 0:
            sys.exit(1)
        nproc = 3
        _, out, _ = ssh.exec_command("nproc", timeout=30)
        nproc = int(out.read().decode().strip() or 3)
        jobs = args.jobs or min(nproc, 4)

        suites = []
        if args.suite in ("native", "all"):
            suites.append(("native", NATIVE_STEPS, NATIVE_TIMEOUT))
        if args.suite in ("web", "all"):
            suites.append(("web", WEB_STEPS, WEB_TIMEOUT))

        failed = []
        for name, steps, _ in suites:
            print(f"\n=== suite: {name} ===")
            for label, cmd, step_timeout in steps:
                cmd = cmd.replace("{JOBS}", str(jobs))
                rc = run(ssh, f"cd {args.dir} && {cmd}", label, step_timeout)
                if rc != 0:
                    failed.append(f"{name}/{label}")
                    break  # later steps depend on earlier ones
        if failed:
            die("FAILED: " + ", ".join(failed) +
                " — do NOT dispatch the GitHub Actions gate", 1)
        print("\nALL PASSED — server verification green; "
              "you may dispatch the GitHub Actions gate.")
    finally:
        ssh.close()


if __name__ == "__main__":
    main()
