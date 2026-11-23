# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

"""Regression checks for the HTTP exchanges observed on CyAICodex and RealityAssistant.

Run with: python tests/http_compatibility.py path/to/CyMCPHubMcp
"""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class TestProcess(subprocess.Popen):
    def __exit__(self, exc_type, exc, traceback):
        if exc_type and self.poll() is None:
            self.terminate()
        return super().__exit__(exc_type, exc, traceback)


class Connector(BaseHTTPRequestHandler):
    def do_POST(self):
        assert self.headers.get('Authorization') is None
        request = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        method = request['method']
        refusal = {'/token-required': 401, '/forbidden': 403}.get(self.path.split('?', 1)[0])
        if refusal:
            assert method == 'initialize'
            self.send_response(refusal)
            if refusal == 401:
                self.send_header('WWW-Authenticate', 'Bearer')
            self.end_headers()
            self.wfile.write(b'fixture-private-response')
            return
        response = {'jsonrpc': '2.0', 'id': request.get('id')}
        status = 200
        protocol = '2025-03-26' if self.path == '/old-peer' else '2025-11-25'
        if method == 'server/discover':
            status = 400
            response['id'] = 'server-error'
            response['error'] = {'code': -32600, 'message': 'Unsupported protocol version'}
        elif method == 'initialize':
            response['result'] = {'serverInfo': {'name': 'Test connector', 'version': '1'},
                                  'instructions': 'Search the test database.',
                                  'protocolVersion': protocol, 'capabilities': {'tools': {}}}
        elif method == 'notifications/initialized':
            assert self.headers['MCP-Protocol-Version'] == protocol
            self.send_response(204 if self.path == '/old-peer' else 202)
            self.end_headers()
            return
        elif method == 'tools/list':
            assert self.headers['MCP-Protocol-Version'] == protocol
            response['result'] = {'tools': [{'name': 'check', 'inputSchema': {'type': 'object'}}]}
        elif method == 'resources/list':
            response['result'] = {'resources': [{'uri': 'resource://fixture/SKILL.md', 'name': 'SKILL.md'}]}
        elif method == 'resources/read':
            response['result'] = {'contents': [{'uri': request['params']['uri'],
                'text': '---\nname: test-connector\ndescription: Inspect the test connector.\n---\n\nUse check.\n'}]}
        else:
            raise AssertionError(method)
        body = json.dumps(response).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_):
        pass


def check(executable, root, port):
    settings = root / 'mcp.json'
    origin = f'http://127.0.0.1:{port}'
    settings.write_text(json.dumps({'servers': {
        'old-peer': {'url': origin + '/old-peer'},
        'current-peer': {'url': origin + '/current-peer'},
        'token-required': {'url': origin + '/token-required?access_token=fixture-private-query'},
        'forbidden': {'url': origin + '/forbidden'},
    }}), encoding='utf-8')
    (root / 'SKILL.md').write_text('---\nname: local-test\ndescription: Identify the local test SDK.\n---\n\nRead the SDK catalogue.\n', encoding='utf-8')
    environment = dict(os.environ, HOME=str(root), USERPROFILE=str(root), APPDATA=str(root), LOCALAPPDATA=str(root),
                       XDG_CONFIG_HOME=str(root / 'config'), XDG_DATA_HOME=str(root / 'xdg-data'),
                       CODEX_HOME=str(root), CYMCP_HUB_DATA=str(root / 'data'))
    command = [executable, '--workspace', str(root), '--config', str(settings), '--no-port-scan']
    inventory = subprocess.check_output(command + ['--list'], env=environment, encoding='utf-8', timeout=15)
    connectors = {item['name']: item['id'] for item in json.loads(inventory)['servers']}

    with TestProcess(command, env=environment, stdin=subprocess.PIPE, stdout=subprocess.PIPE, encoding='utf-8') as hub:
        def send(method, params, id=None):
            request = {'jsonrpc': '2.0', 'method': method, 'params': params}
            if id is not None:
                request['id'] = id
            hub.stdin.write(json.dumps(request) + '\n')
            hub.stdin.flush()
            if id is None:
                return None
            result = json.loads(hub.stdout.readline())
            assert result['id'] == id and 'error' not in result and not result['result'].get('isError'), result
            return result['result']

        send('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {},
                            'clientInfo': {'name': 'http_compatibility', 'version': '1'}}, 1)
        send('notifications/initialized', {})
        for offset, name in enumerate(('old-peer', 'current-peer'), 2):
            tools = send('tools/call', {'name': 'mcp_tools', 'arguments': {'id': connectors[name]}}, offset)
            assert tools['structuredContent']['tools'][0]['name'] == 'check'
        log = send('tools/call', {'name': 'mcp_journal', 'arguments': {}}, 4)['structuredContent']['jsonLines']
        entries = [json.loads(line) for line in log.splitlines()]
        assert any(entry['operation'] == 'server/discover' and entry['status'] == 'HTTP 400' for entry in entries)
        assert any(entry['operation'] == 'notifications/initialized' and entry['status'] == 'HTTP 204' for entry in entries)
        assert len([entry for entry in entries if entry['operation'] == 'tools/list' and entry['status'] == 'HTTP 200']) == 2
        refusals = {entry['source']: entry for entry in entries if entry['operation'] == 'inventory/initialize'}
        assert refusals['token-required']['status'] in ('HTTP 401', 'Access denied'), refusals
        assert refusals['forbidden']['status'] in ('HTTP 403', 'Access denied'), refusals
        assert all(entry['detail'] == origin for entry in refusals.values()), refusals
        assert 'fixture-private-' not in log, log
        catalogue = send('tools/call', {'name': 'mcp_catalog', 'arguments': {'id': connectors['old-peer'],
                         'format': 'markdown', 'query': 'check'}}, 5)['structuredContent']
        assert catalogue['applications'][0]['summary'] == 'Search the test database.'
        assert '**check**' in catalogue['markdown']
        local = send('tools/call', {'name': 'sdk_scan', 'arguments': {'path': 'SKILL.md', 'includePath': False}}, 6)
        assert local['structuredContent']['skills'][0]['name'] == 'local-test'
        imported = send('tools/call', {'name': 'mcp_import_skill', 'arguments': {'path': 'SKILL.md'}}, 7)
        assert imported['structuredContent']['connectorId'] == 'library'
        collected = send('tools/call', {'name': 'mcp_collect_skills', 'arguments': {'id': connectors['old-peer']}}, 8)
        assert collected['structuredContent']['skills'][0]['name'] == 'test-connector'
        authored = '---\nname: authoring-test\ndescription: Use the check action safely.\n---\n\nCall check after inspecting the schema.\n'
        send('tools/call', {'name': 'mcp_save_skill', 'arguments': {'id': connectors['old-peer'], 'markdown': authored}}, 9)
        loaded = send('tools/call', {'name': 'mcp_read_skill', 'arguments': {'connectorId': connectors['old-peer'], 'name': 'authoring-test'}}, 10)
        assert loaded['structuredContent']['markdown'] == authored
        hub.stdin.close()
        stopped = hub.wait(timeout=15)
        assert stopped == 0, hex(stopped)


if __name__ == '__main__':
    with tempfile.TemporaryDirectory() as directory, ThreadingHTTPServer(('127.0.0.1', 0), Connector) as server:
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            check(str(Path(sys.argv[1]).resolve()), Path(directory), server.server_port)
        finally:
            server.shutdown()
            worker.join()
    print('Verified HTTP compatibility, inventory refusals in the journal, Markdown catalogue, and skill operations.')
