"""Exercise the migrated STDIO MCP using its real personal Codex configuration.

Only fixture copies under --output are modified; source game databases are hashed.
Uses the installed Python 3.11 standard library, without another agent or MCP SDK.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import shutil
import struct
import subprocess
import threading
import time
import tomllib


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def walk(value):
    if isinstance(value, dict):
        yield value
        for child in value.values():
            yield from walk(child)
    elif isinstance(value, list):
        for child in value:
            yield from walk(child)


def pe_info(binary: Path):
    data = binary.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    optional = pe + 24
    magic = struct.unpack_from('<H', data, optional)[0]
    base = struct.unpack_from('<I' if magic == 0x10b else '<Q', data, optional + (28 if magic == 0x10b else 24))[0]
    entry = struct.unpack_from('<I', data, optional + 16)[0]
    sections = struct.unpack_from('<H', data, pe + 6)[0]
    section_table = optional + struct.unpack_from('<H', data, pe + 20)[0]
    for i in range(sections):
        offset = section_table + i * 40
        if data[offset:offset + 8].rstrip(b'\0') == b'.rdata':
            rva, raw_size, raw = struct.unpack_from('<III', data, offset + 12)
            if raw_size >= 8:
                # IAT slots at the start of .rdata are normalized by IDA. Choose a
                # real pair of code pointers (vtable/table), not import slots.
                if magic == 0x10b:
                    for relative in range(0, raw_size - 7, 4):
                        words = struct.unpack_from('<II', data, raw + relative)
                        if all(base + 0x1000 <= word < base + rva for word in words):
                            return base, entry, base + rva + relative, list(words)
                return base, entry, base + rva, list(struct.unpack_from('<II', data, raw))
    raise RuntimeError('No readable .rdata fixture')


class Client:
    def __init__(self, config: dict, output: Path):
        self.log = (output / "server.log").open("w", encoding="utf-8")
        self.proc = subprocess.Popen(
            [config["command"], *config["args"]], cwd=config["cwd"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, encoding="utf-8", errors="replace",
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        self.responses: queue.Queue = queue.Queue()
        self.sequence = 0
        self.transcript: list[dict] = []
        self.session: str | None = None
        threading.Thread(target=self.read_output, daemon=True).start()
        threading.Thread(target=self.read_errors, daemon=True).start()

    def read_output(self):
        for line in self.proc.stdout:
            try:
                value = json.loads(line)
            except ValueError:
                self.log.write(line)
                continue
            self.responses.put(value)
        self.responses.put({"eof": True})

    def read_errors(self):
        for line in self.proc.stderr:
            self.log.write(line)
            self.log.flush()

    def send(self, message: dict):
        self.proc.stdin.write(json.dumps(message) + "\n")
        self.proc.stdin.flush()

    def rpc(self, method: str, params: dict, timeout: int = 180):
        self.sequence += 1
        request_id = self.sequence
        self.send({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params})
        deadline = time.monotonic() + timeout
        while True:
            response = self.responses.get(timeout=max(0.1, deadline - time.monotonic()))
            if response.get("eof"):
                raise RuntimeError("MCP exited; see server.log")
            if response.get("id") != request_id:
                if time.monotonic() > deadline:
                    raise TimeoutError(method)
                continue
            if "error" in response:
                raise RuntimeError(response["error"])
            return response["result"]

    def call(self, name: str, arguments: dict, routed: bool = True):
        args = dict(arguments)
        if routed:
            args["database"] = self.session
        response = self.rpc("tools/call", {"name": name, "arguments": args})
        data = response.get("structuredContent")
        if data is None:
            texts = [c["text"] for c in response.get("content", []) if c.get("type") == "text"]
            try:
                data = json.loads("\n".join(texts))
            except ValueError:
                data = texts
        self.transcript.append({"tool": name, "arguments": args, "response": data})
        if response.get("isError"):
            raise RuntimeError(f"{name}: {data}")
        errors = [item["error"] for item in walk(data) if item.get("error")]
        if errors:
            raise RuntimeError(f"{name}: {errors}")
        print(f"MCP {name}: response received", flush=True)
        return data

    def close(self):
        if self.session:
            self.call("idb_close", {"database": self.session, "save": True}, routed=False)
            self.session = None
        self.proc.stdin.close()
        try:
            self.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.proc.terminate()
            self.proc.wait(timeout=10)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source-engine", type=Path)
    parser.add_argument("--discover-only", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    config = tomllib.loads(args.config.read_text(encoding="utf-8"))["mcp_servers"]["idalib"]
    client = Client(config, args.output)
    try:
        init = client.rpc("initialize", {"protocolVersion": "2024-11-05", "capabilities": {},
                                         "clientInfo": {"name": "audiosurf-migration-check", "version": "1"}})
        client.send({"jsonrpc": "2.0", "method": "notifications/initialized"})
        tools = client.rpc("tools/list", {})["tools"]
        (args.output / "tools.json").write_text(json.dumps(tools, indent=2), encoding="utf-8")
        names = {tool["name"] for tool in tools}
        required = {"idb_open", "idb_close", "decompile", "xrefs_to", "get_int", "read_struct",
                    "rename", "set_comments", "set_type"}
        if required - names:
            raise RuntimeError(f"Missing capabilities: {sorted(required - names)}")
        print(json.dumps({"server": init.get("serverInfo"), "toolCount": len(tools),
                          "requiredCapabilities": "present"}), flush=True)
        if not args.discover_only:
            if not args.source_engine:
                parser.error('--source-engine is required for fixture checks')
            source = args.source_engine / 'HighPoly.dll'
            source_db = source.with_name(source.name + '.i64')
            before = {str(p): digest(p) for p in (source, source_db)}
            fixture = args.output / 'fixture-x86'
            fixture.mkdir(exist_ok=False)
            copied = fixture / source.name
            shutil.copy2(source, copied)
            shutil.copy2(source_db, copied.with_name(copied.name + '.i64'))

            def open_database(binary):
                value = client.call('idb_open', {'input_path': str(binary.resolve()), 'mode': 'force_headless',
                                                'run_auto_analysis': True}, routed=False)
                sessions = [item['session_id'] for item in walk(value) if 'session_id' in item]
                if not sessions:
                    raise RuntimeError(f'No session returned: {value}')
                client.session = sessions[0]

            try:
                open_database(copied)
                base, entry, data_addr, words = pe_info(copied)
                function = hex(base + 0x8e20)  # Known HighPoly GetChannel function, documented in the Lua audit.
                client.call('lookup_funcs', {'queries': function})
                code = client.call('decompile', {'addr': function, 'max_lines': 30})
                if not any(item.get('code') for item in walk(code)):
                    raise RuntimeError('No x86 pseudocode returned')
                client.call('xrefs_to', {'addrs': function, 'limit': 5})
                integers = client.call('get_int', {'queries': [{'addr': hex(data_addr + i*4), 'ty': 'u32'} for i in range(2)]})
                actual = [item['value'] for item in walk(integers) if 'value' in item]
                if [int(v, 0) if isinstance(v, str) else v for v in actual] != words:
                    raise RuntimeError(f'Data reads disagree with PE bytes: {actual} versus {words}')
                client.call('declare_type', {'decls': 'struct migration_probe_words { unsigned int first; unsigned int second; };'})
                client.call('set_type', {'edits': {'addr': hex(data_addr), 'ty': 'struct migration_probe_words', 'kind': 'global'}})
                structure = client.call('read_struct', {'queries': {'addr': hex(data_addr), 'struct': 'migration_probe_words'}})
                if not any(item.get('members') for item in walk(structure)):
                    raise RuntimeError('No structure members returned')
                client.call('rename', {'batch': {'func': {'addr': function, 'name': 'migration_probe_get_channel'}}})
                marker = 'AUDIOSURF_AGENT_MIGRATION_PERSISTENCE_CHECK'
                client.call('set_comments', {'items': {'addr': function, 'comment': marker}})
                client.call('idb_close', {'database': client.session, 'save': True}, routed=False)
                client.session = None
                open_database(copied)
                renamed = client.call('lookup_funcs', {'queries': 'migration_probe_get_channel'})
                if 'migration_probe_get_channel' not in json.dumps(renamed):
                    raise RuntimeError('Rename did not persist')
                saved_code = client.call('decompile', {'addr': function, 'max_lines': 30})
                if marker not in json.dumps(saved_code):
                    raise RuntimeError('Comment did not persist')
                saved_struct = client.call('read_struct', {'queries': {'addr': hex(data_addr)}})
                if 'migration_probe_words' not in json.dumps(saved_struct):
                    raise RuntimeError('Applied type did not persist')
                client.call('idb_close', {'database': client.session, 'save': True}, routed=False)
                client.session = None
                # Preserve the previously verified x64 capability on a separate OS DLL copy.
                system_binary = Path(os.environ['WINDIR']) / 'System32' / 'version.dll'
                fixture64 = args.output / 'fixture-x64'
                fixture64.mkdir(exist_ok=False)
                copied64 = fixture64 / 'version.dll'
                shutil.copy2(system_binary, copied64)
                open_database(copied64)
                base64, entry64, _, _ = pe_info(copied64)
                code64 = client.call('decompile', {'addr': hex(base64 + entry64), 'max_lines': 20})
                if not any(item.get('code') for item in walk(code64)):
                    raise RuntimeError('No x64 pseudocode returned')
                client.call('idb_close', {'database': client.session, 'save': True}, routed=False)
                client.session = None
                sessions = client.call('idb_list', {}, routed=False)
                report = {'x86': 'passed', 'x64': 'passed', 'integerReads': 'matched PE bytes',
                          'structReads': 'passed', 'renameCommentTypePersistence': 'passed',
                          'fixtureSessionsClosed': True, 'sourceHashes': before,
                          'sessionInventory': sessions}
                (args.output / 'acceptance.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
                print('IDA acceptance: x86/x64, data, persisted rename/comment/type: PASSED', flush=True)
            finally:
                after = {str(p): digest(p) for p in (source, source_db)}
                (args.output / 'source-integrity.json').write_text(json.dumps({'before': before, 'after': after}, indent=2), encoding='utf-8')
                if before != after:
                    raise RuntimeError('Source game data changed during fixture check')
    finally:
        try:
            client.close()
        finally:
            (args.output / "transcript.json").write_text(json.dumps(client.transcript, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
