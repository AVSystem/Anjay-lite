# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

import dataclasses
import json
import socket
import struct

SOF_BYTE = 0xF7


def _recv_exact(sock, n):
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("socket closed")
        data += chunk
    return data


def _json_default(obj):
    if dataclasses.is_dataclass(obj):
        return dataclasses.asdict(obj)
    raise TypeError(
        f"Object of type {type(obj).__name__} is not JSON serializable")


class AppManager:
    def __init__(self,
                 host,
                 port=None,
                 timeout=1.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock = None
        self.srv = None

    def create_socket(self):
        if self.sock is None:
            self.srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.srv.settimeout(self.timeout)
            self.srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.srv.bind((self.host, 0 if self.port is None else self.port))
            self.srv.listen(1)

            self.host, self.port = self.srv.getsockname()

    def connect(self):
        conn, _ = self.srv.accept()
        self.sock = conn

    def close(self):
        if self.sock is not None:
            self.sock.close()
            self.sock = None

        if self.srv is not None:
            self.srv.close()
            self.srv = None

    def get_port(self):
        return self.port

    def _send_tlv_json(self, msg_type: int, payload: dict):
        raw = json.dumps(payload, separators=(",", ":"),
                         default=_json_default).encode("utf-8")
        self.sock.send(struct.pack(">BI", msg_type, len(raw)) + raw)

    def _recv_tlv_json(self) -> tuple[int, dict]:
        header = _recv_exact(self.sock, 5)
        msg_type, length = struct.unpack(">BI", header)
        payload = _recv_exact(self.sock, length)
        return msg_type, json.loads(payload.decode("utf-8"))

    def call(self, name: str, *args: object):
        self._send_tlv_json(SOF_BYTE, {"name": name, "args": args})
        _, payload = self._recv_tlv_json()

        if "error" in payload:
            raise Exception(payload["error"])

        return payload.get("result")
