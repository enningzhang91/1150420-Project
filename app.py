import json
import os
import subprocess
import uuid
from pathlib import Path

from flask import Flask, jsonify, render_template, request, session


BASE_DIR = Path(__file__).resolve().parent
STATE_DIR = BASE_DIR / "game_states"
BACKEND_SOURCE = BASE_DIR / "mine_backend.c"
BACKEND_EXE = BASE_DIR / ("mine_backend.exe" if os.name == "nt" else "mine_backend")

app = Flask(__name__)
app.config["SECRET_KEY"] = "replace-this-with-a-random-secret-key"


def ensure_backend_compiled() -> None:
    STATE_DIR.mkdir(exist_ok=True)
    if BACKEND_EXE.exists() and BACKEND_EXE.stat().st_mtime >= BACKEND_SOURCE.stat().st_mtime:
        return

    compile_result = subprocess.run(
        ["gcc", str(BACKEND_SOURCE), "-O2", "-o", str(BACKEND_EXE)],
        capture_output=True,
        text=True,
        check=False,
    )
    if compile_result.returncode != 0:
        raise RuntimeError(
            "C backend compile failed.\n"
            f"stdout:\n{compile_result.stdout}\n"
            f"stderr:\n{compile_result.stderr}"
        )


def get_session_id() -> str:
    if "game_id" not in session:
        session["game_id"] = str(uuid.uuid4())
    return session["game_id"]


def game_file_path() -> Path:
    return STATE_DIR / f"{get_session_id()}.dat"


def run_backend(args: list[str]) -> dict:
    ensure_backend_compiled()
    result = subprocess.run(
        [str(BACKEND_EXE), *args],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"Backend error: {result.stderr.strip() or result.stdout.strip()}")
    return json.loads(result.stdout)


@app.route("/")
def index():
    return render_template("index.html")


@app.post("/api/new")
def new_game():
    payload = request.get_json(silent=True) or {}
    rows = int(payload.get("rows", 9))
    cols = int(payload.get("cols", 9))
    mines = int(payload.get("mines", 10))
    seed = int(payload.get("seed", 0))

    state_file = game_file_path()
    if state_file.exists():
        state_file.unlink()

    game = run_backend(["init", str(state_file), str(rows), str(cols), str(mines), str(seed)])
    return jsonify(game)


@app.post("/api/open")
def open_cell():
    payload = request.get_json(silent=True) or {}
    row = int(payload.get("row", -1))
    col = int(payload.get("col", -1))
    game = run_backend(["open", str(game_file_path()), str(row), str(col)])
    return jsonify(game)


@app.post("/api/flag")
def flag_cell():
    payload = request.get_json(silent=True) or {}
    row = int(payload.get("row", -1))
    col = int(payload.get("col", -1))
    game = run_backend(["flag", str(game_file_path()), str(row), str(col)])
    return jsonify(game)


@app.post("/api/undo")
def undo():
    game = run_backend(["undo", str(game_file_path())])
    return jsonify(game)


if __name__ == "__main__":
    ensure_backend_compiled()
    port = int(os.environ.get("PORT", "8000"))
    app.run(host="127.0.0.1", port=port, debug=True)
