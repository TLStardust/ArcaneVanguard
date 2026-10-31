# -*- coding: utf-8 -*-
"""
UE Bridge -- runs INSIDE Unreal Engine 5.4's bundled Python interpreter.

Opens a tiny localhost HTTP server. The WorkBuddy MCP server (a separate Python
process with no `unreal` module) posts Python source here, and this script runs
it inside the editor where the `unreal` module and the live world exist.

Threading model -- this part matters:

  * The HTTP server runs on a BACKGROUND thread, so the editor's game thread is
    never blocked while the bridge waits for requests.
  * Snippets are dispatched back onto the GAME THREAD via a Slate post-tick
    callback. Editor APIs (spawning actors, creating levels, ...) are not
    thread-safe and will crash the editor if called off the game thread.

Start it from inside the editor (see README.md); it returns immediately.

Security: binds to 127.0.0.1 only and optionally requires a shared token.
"""

from __future__ import print_function

import json
import os
import sys
import threading
import traceback

# --- Configuration -----------------------------------------------------------

HOST = "127.0.0.1"
PORT = int(os.environ.get("UE_BRIDGE_PORT", "37780"))
TOKEN = os.environ.get("UE_BRIDGE_TOKEN", "")

# How long a single snippet may run on the game thread before we stop waiting.
SNIPPET_TIMEOUT_SECONDS = float(os.environ.get("UE_BRIDGE_TIMEOUT", "120"))

# A single game-thread tick must not run too long, or the editor stalls.
MAX_SNIPPETS_PER_TICK = 1


# --- Game-thread work queue --------------------------------------------------

# Items are dicts: {"code": str, "done": Event, "outcome": dict}
_work_queue = []
_work_lock = threading.Lock()
_tick_handle = None


def _execute_on_game_thread(source):
    """Run `source`, capturing stdout and any exception as structured data."""
    captured = []

    class _Capture(object):
        def write(self, text):
            if text:
                captured.append(text)
            return len(text) if text else 0

        def flush(self):
            pass

    namespace = {"__name__": "__ue_bridge__"}
    old_stdout = sys.stdout
    try:
        import unreal

        namespace["unreal"] = unreal
        sys.stdout = _Capture()
        exec(compile(source, "<ue_bridge_snippet>", "exec"), namespace)
    except Exception:
        return {
            "ok": False,
            "error": traceback.format_exc(),
            "stdout": "".join(captured),
        }
    finally:
        sys.stdout = old_stdout

    return {
        "ok": True,
        "result": _jsonable(namespace.get("result")),
        "stdout": "".join(captured),
    }


def _tick(_delta_seconds):
    """Runs on the game thread every frame; drains the work queue."""
    for _ in range(MAX_SNIPPETS_PER_TICK):
        with _work_lock:
            if not _work_queue:
                return
            item = _work_queue.pop(0)

        try:
            item["outcome"] = _execute_on_game_thread(item["code"])
        except BaseException as exc:  # never let the tick callback die
            item["outcome"] = {
                "ok": False,
                "error": "game-thread execution crashed: %s" % exc,
                "stdout": "",
            }
        finally:
            item["done"].set()


def _submit(source):
    """Queue `source` for the game thread and wait for its result."""
    item = {"code": source, "done": threading.Event(), "outcome": None}
    with _work_lock:
        _work_queue.append(item)

    if not item["done"].wait(SNIPPET_TIMEOUT_SECONDS):
        return {
            "ok": False,
            "error": "snippet exceeded %.0fs timeout" % SNIPPET_TIMEOUT_SECONDS,
            "stdout": "",
        }
    return item["outcome"] or {"ok": False, "error": "no result", "stdout": ""}


def _jsonable(value, depth=0):
    """Best-effort conversion of an arbitrary unreal value into JSON."""
    if depth > 6:
        return repr(value)
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, dict):
        return {str(k): _jsonable(v, depth + 1) for k, v in value.items()}
    if isinstance(value, (list, tuple, set)):
        return [_jsonable(v, depth + 1) for v in value]

    for attr in ("to_dict", "to_tuple"):
        method = getattr(value, attr, None)
        if callable(method):
            try:
                return _jsonable(method(), depth + 1)
            except Exception:
                pass

    get_name = getattr(value, "get_name", None)
    if callable(get_name):
        try:
            return get_name()
        except Exception:
            pass

    try:
        return repr(value)
    except Exception:
        return "<unserialisable>"


# --- HTTP handling (stdlib only -- the bundled Python is bare) ---------------

try:
    from http.server import BaseHTTPRequestHandler, HTTPServer
except ImportError:  # pragma: no cover - Python 2 fallback
    from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer


class _BridgeHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        pass

    def _respond(self, code, payload):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path != "/ping":
            self._respond(404, {"ok": False, "error": "not found"})
            return
        self._respond(200, {
            "ok": True,
            "service": "ue-bridge",
            "engine": _engine_version(),
            "game_thread_dispatch": _tick_handle is not None,
        })

    def do_POST(self):
        if self.path != "/exec":
            self._respond(404, {"ok": False, "error": "not found"})
            return

        if TOKEN and self.headers.get("X-Bridge-Token", "") != TOKEN:
            self._respond(403, {"ok": False, "error": "bad token"})
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self._respond(400, {"ok": False, "error": "bad content-length"})
            return

        raw = self.rfile.read(length) if length else b"{}"
        try:
            payload = json.loads(raw.decode("utf-8"))
        except Exception as exc:
            self._respond(400, {"ok": False, "error": "bad json: %s" % exc})
            return

        source = payload.get("code", "")
        if not isinstance(source, str) or not source.strip():
            self._respond(400, {"ok": False, "error": "missing 'code'"})
            return

        self._respond(200, _submit(source))

    def handle_one_request(self):
        try:
            BaseHTTPRequestHandler.handle_one_request(self)
        except (ConnectionResetError, BrokenPipeError):
            self.close_connection = True


def _engine_version():
    try:
        import unreal

        return unreal.SystemLibrary.get_engine_version()
    except Exception:
        return "unknown"


def _serve_http():
    server = HTTPServer((HOST, PORT), _BridgeHandler)
    server.serve_forever()


def main():
    global _tick_handle

    # This runs as an editor startup script. If anything goes wrong we log and
    # return rather than raising, so a bridge problem can never stop the editor
    # from starting.
    try:
        import unreal
    except ImportError:
        print("[ue-bridge] not running inside Unreal; nothing to do")
        return

    try:
        _tick_handle = unreal.register_slate_post_tick_callback(_tick)
    except Exception as exc:
        unreal.log_error("[ue-bridge] could not register tick callback: %s" % exc)
        return

    try:
        http_thread = threading.Thread(target=_serve_http)
        http_thread.daemon = True
        http_thread.start()
    except Exception as exc:
        unreal.log_error("[ue-bridge] could not start HTTP thread: %s" % exc)
        return

    unreal.log(
        "[ue-bridge] listening on http://%s:%d/exec (game-thread dispatch active)"
        % (HOST, PORT))


if __name__ == "__main__":
    main()
