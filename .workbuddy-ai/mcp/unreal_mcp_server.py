# -*- coding: utf-8 -*-
"""
WorkBuddy MCP server for the ArcaneVanguard Unreal Engine project.

Speaks MCP over stdio. Read-only tools inspect the project straight from disk
(no editor required). Editor tools forward Python snippets to ue_bridge.py,
which runs inside a live UE 5.4 editor via its bundled Python interpreter.

Transport to the editor: localhost HTTP + shared token, see ue_bridge.py.
"""

from __future__ import annotations

import json
import os
import re
import socket
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

from mcp.server.mcpserver import MCPServer

# --- Project discovery -------------------------------------------------------
# Walk up from this file looking for a .uproject, so the server keeps working
# even if the project folder is renamed or moved.

def _find_project_root() -> Path:
    here = Path(__file__).resolve()
    for candidate in [here.parent, *here.parents]:
        if list(candidate.glob("*.uproject")):
            return candidate
    # Fall back to the known workspace location.
    return Path(r"D:\UEProject\ArcaneVanguard")


PROJECT_ROOT = _find_project_root()
UPROJECT = next(iter(PROJECT_ROOT.glob("*.uproject")), None)

# --- Bridge configuration ----------------------------------------------------

BRIDGE_HOST = os.environ.get("UE_BRIDGE_HOST", "127.0.0.1")
BRIDGE_PORT = int(os.environ.get("UE_BRIDGE_PORT", "37777"))
BRIDGE_TOKEN = os.environ.get("UE_BRIDGE_TOKEN", "")
BRIDGE_TIMEOUT = float(os.environ.get("UE_BRIDGE_HTTP_TIMEOUT", "180"))

server = MCPServer("unreal-arcanevanguard")


# --- Helpers -----------------------------------------------------------------

def _read_text(path: Path, limit: int = 200_000) -> str:
    try:
        data = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return f"<无法读取 {path}: {exc}>"
    if len(data) > limit:
        return data[:limit] + f"\n... <截断，共 {len(data)} 字符>"
    return data


def _classify_uasset(path: Path) -> str:
    """Cheap structural sniff of a .uasset to guess its class.

    Blueprints store their generated class name right after the asset name in
    the package header, so a plain substring search is a decent heuristic.
    """
    try:
        raw = path.read_bytes()[:4096]
    except OSError:
        return "unknown"
    text = raw.decode("latin-1", errors="ignore")
    if "_C" in text and "Blueprint" in text:
        return "blueprint"
    for kind in ("WidgetBlueprint", "AnimBlueprint", "Material", "Texture",
                 "StaticMesh", "SkeletalMesh", "SoundWave", "DataTable"):
        if kind in text:
            return kind
    return "unknown"


def _ucx(path: Path) -> str:
    return path.relative_to(PROJECT_ROOT).as_posix()


# Directories that hold tooling/metadata rather than project source. Excluded
# from source scans so results stay focused on real code.
_IGNORED_DIRS = {".workbuddy-ai", ".workbuddy", "__pycache__", ".vs", ".idea"}


def _is_project_source(path: Path) -> bool:
    return not any(part in _IGNORED_DIRS for part in path.parts)


def _project_info() -> dict:
    info: dict = {
        "root": str(PROJECT_ROOT),
        "name": PROJECT_ROOT.name,
        "uproject": None,
        "engine_association": None,
        "modules": [],
        "plugins": [],
        "source_files": [],
        "maps": [],
        "blueprints": [],
    }
    if UPROJECT and UPROJECT.exists():
        info["uproject"] = str(UPROJECT)
        try:
            data = json.loads(UPROJECT.read_text(encoding="utf-8"))
            info["engine_association"] = data.get("EngineAssociation")
            info["modules"] = [m.get("Name") for m in data.get("Modules", [])]
            info["plugins"] = [p.get("Name") for p in data.get("Plugins", [])]
        except Exception as exc:
            info["uproject_error"] = str(exc)

    src = PROJECT_ROOT / "Source"
    if src.is_dir():
        info["source_files"] = sorted(_ucx(p) for p in src.rglob("*")
                                      if p.suffix in {".h", ".cpp", ".cs"})

    content = PROJECT_ROOT / "Content"
    if content.is_dir():
        info["maps"] = sorted(_ucx(p) for p in content.rglob("*.umap"))
        info["blueprints"] = sorted(_ucx(p) for p in content.rglob("*.uasset")
                                    if _classify_uasset(p) == "blueprint")
    return info


def _bridge_request(payload: dict, method: str = "POST", path: str = "/exec"):
    url = f"http://{BRIDGE_HOST}:{BRIDGE_PORT}{path}"
    body = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=body, method=method)
    req.add_header("Content-Type", "application/json")
    if BRIDGE_TOKEN:
        req.add_header("X-Bridge-Token", BRIDGE_TOKEN)
    with urllib.request.urlopen(req, timeout=BRIDGE_TIMEOUT) as resp:
        return json.loads(resp.read().decode("utf-8"))


def _bridge_alive() -> bool:
    try:
        with socket.create_connection((BRIDGE_HOST, BRIDGE_PORT), timeout=1.5):
            return True
    except OSError:
        return False


EDITOR_DOWN_HINT = (
    "编辑器桥接未运行。请在 UE 5.4 编辑器中通过 Python 控制台启动 .workbuddy-ai/mcp/ue_bridge.py，"
    "或使用 -ModelContextProtocolStartServer 之外的方式参考 README.md 的启动说明。"
)


# --- Project tools (disk only, no editor needed) ----------------------------

@server.tool()
def get_project_info() -> str:
    """读取 CTest 项目的整体信息：引擎版本、模块、已启用插件、源码/关卡/蓝图清单。"""
    return json.dumps(_project_info(), ensure_ascii=False, indent=2)


@server.tool()
def list_source_files(extension: str = "") -> str:
    """列出 Source 目录下的源码文件，可按扩展名过滤（如 ".cpp" / ".h" / ".cs"）。"""
    src = PROJECT_ROOT / "Source"
    if not src.is_dir():
        return "未找到 Source 目录。"
    ext = extension.strip().lower()
    files = sorted(_ucx(p) for p in src.rglob("*")
                   if p.is_file() and (not ext or p.suffix.lower() == ext))
    return "\n".join(files) if files else "没有匹配的文件。"


@server.tool()
def read_source_file(relative_path: str) -> str:
    """读取项目内某个源码或配置文件的内容。

    relative_path 使用项目根目录的相对路径，例如 "Source/CTest/TractorStatePacket.cpp"。
    """
    target = (PROJECT_ROOT / relative_path).resolve()
    if not str(target).startswith(str(PROJECT_ROOT.resolve())):
        return "拒绝访问：路径超出项目目录范围。"
    if not target.is_file():
        return f"文件不存在：{relative_path}"
    return _read_text(target)


@server.tool()
def search_in_sources(keyword: str, extension: str = "") -> str:
    """在 Source 目录里搜索关键字，返回命中的文件、行号与该行内容。

    keyword 为要搜索的文本；extension 可选，用于限定扩展名（如 ".cpp"）。
    """
    src = PROJECT_ROOT / "Source"
    if not src.is_dir():
        return "未找到 Source 目录。"
    ext = extension.strip().lower()
    if not keyword:
        return "请提供 keyword。"

    hits: list[str] = []
    pattern = re.compile(re.escape(keyword), re.IGNORECASE)
    for path in sorted(src.rglob("*")):
        if not path.is_file() or not _is_project_source(path):
            continue
        if ext and path.suffix.lower() != ext:
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for lineno, line in enumerate(text.splitlines(), start=1):
            if pattern.search(line):
                hits.append(f"{_ucx(path)}:{lineno}: {line.strip()}")
                if len(hits) >= 200:
                    hits.append("... <结果过多，已截断>")
                    return "\n".join(hits)
    return "\n".join(hits) if hits else f"未找到包含 “{keyword}” 的内容。"


@server.tool()
def list_content_assets(folder: str = "", asset_class: str = "") -> str:
    """列出 Content 下的资产。folder 为子目录（可选），asset_class 用于过滤类型（如 "blueprint"）。

    注意：这是基于文件头的启发式分类，不等同于编辑器内的精确资产类型。
    """
    content = PROJECT_ROOT / "Content"
    if not content.is_dir():
        return "未找到 Content 目录。"
    base = (content / folder).resolve() if folder else content
    if not str(base).startswith(str(content.resolve())) or not base.is_dir():
        return f"无效目录：{folder}"

    wanted = asset_class.strip().lower()
    lines: list[str] = []
    for path in sorted(base.rglob("*.uasset")):
        kind = _classify_uasset(path)
        if wanted and kind != wanted:
            continue
        lines.append(f"{_ucx(path)}  [{kind}]  {path.stat().st_size} B")
    return "\n".join(lines) if lines else "没有匹配的资产。"


@server.tool()
def read_config_file(name: str) -> str:
    """读取 Config 目录下的 ini 配置，name 可传 "DefaultEngine.ini" 等文件名。"""
    target = (PROJECT_ROOT / "Config" / name).resolve()
    config_dir = (PROJECT_ROOT / "Config").resolve()
    if not str(target).startswith(str(config_dir)):
        return "拒绝访问：路径超出 Config 目录。"
    if not target.is_file():
        available = ", ".join(sorted(p.name for p in config_dir.glob("*.ini")))
        return f"配置不存在：{name}\n可用配置：{available}"
    return _read_text(target)


# --- Editor tools (require the running bridge) ------------------------------

@server.tool()
def run_editor_python(code: str) -> str:
    """在运行中的 UE 5.4 编辑器里执行 Python 代码，并返回 stdout 与结果。

    代码运行在编辑器 Python 上下文中，可直接使用 `unreal` 模块，例如：
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
        result = [a.get_actor_label() for a in actors]

    将想要返回的值赋给变量 `result`；用 print() 输出的内容会一并返回。
    """
    if not _bridge_alive():
        return EDITOR_DOWN_HINT

    try:
        outcome = _bridge_request({"code": code})
    except urllib.error.HTTPError as exc:
        return f"桥接返回错误 HTTP {exc.code}：{exc.read().decode('utf-8', 'replace')}"
    except Exception as exc:
        return f"调用桥接失败：{exc}"

    parts = []
    if outcome.get("ok"):
        result = outcome.get("result")
        if result is not None:
            parts.append("结果：\n" + json.dumps(result, ensure_ascii=False, indent=2))
        else:
            parts.append("执行成功（无返回值）。")
    else:
        parts.append("执行失败：\n" + str(outcome.get("error", "未知错误")))

    if outcome.get("stdout"):
        parts.append("stdout：\n" + outcome["stdout"])
    return "\n\n".join(parts)


@server.tool()
def get_editor_status() -> str:
    """检查编辑器桥接是否在线，并返回引擎版本与当前打开的关卡信息。"""
    if not _bridge_alive():
        return "编辑器桥接：离线。\n" + EDITOR_DOWN_HINT

    try:
        ping = _bridge_request({}, method="GET", path="/ping")
    except Exception as exc:
        return f"桥接在线但握手失败：{exc}"

    probe = (
        "import unreal\n"
        "world = unreal.EditorLevelLibrary.get_editor_world()\n"
        "result = {\n"
        "    'level': world.get_name() if world else None,\n"
        "    'engine': unreal.SystemLibrary.get_engine_version(),\n"
        "    'project': unreal.Paths.get_project_file_path(),\n"
        "}\n"
    )
    try:
        outcome = _bridge_request({"code": probe})
    except Exception as exc:
        return f"桥接在线，但探测关卡失败：{exc}"

    lines = ["编辑器桥接：在线", f"引擎：{ping.get('engine', '未知')}"]
    if outcome.get("ok"):
        lines.append("状态：" + json.dumps(outcome.get("result"),
                                           ensure_ascii=False, indent=2))
    else:
        lines.append("探测失败：" + str(outcome.get("error")))
    return "\n".join(lines)


@server.tool()
def get_editor_log(tail: int = 80, keyword: str = "") -> str:
    """读取项目 Saved/Logs 下最新的日志文件尾部，可按 keyword 过滤。

    tail 为返回的最大行数（默认 80）。日志文件由编辑器写入，无需桥接。
    """
    logs_dir = PROJECT_ROOT / "Saved" / "Logs"
    if not logs_dir.is_dir():
        return "未找到 Saved/Logs 目录。"

    candidates = sorted(logs_dir.glob("*.log"), key=lambda p: p.stat().st_mtime,
                        reverse=True)
    if not candidates:
        return "日志目录为空。"

    latest = candidates[0]
    try:
        text = latest.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return f"无法读取 {latest.name}：{exc}"

    lines = text.splitlines()
    if keyword:
        lines = [ln for ln in lines if keyword.lower() in ln.lower()]

    tail = max(1, min(int(tail), 2000))
    selected = lines[-tail:]
    header = f"# {latest.name}（最后 {len(selected)} 行" + \
             (f"，过滤 “{keyword}”" if keyword else "") + "）"
    return header + "\n" + "\n".join(selected)


if __name__ == "__main__":
    server.run()
