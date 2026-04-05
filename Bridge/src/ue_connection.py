import asyncio
import json
import uuid
from typing import Any


class UEConnection:
    def __init__(self, host: str = "127.0.0.1", port: int = 55558):
        self.host = host
        self.port = port
        self.reader: asyncio.StreamReader | None = None
        self.writer: asyncio.StreamWriter | None = None
        self.pending: dict[str, asyncio.Future] = {}
        self._receive_task: asyncio.Task | None = None
        self._connected = False
        self.capabilities: list[str] = []

    async def connect(self) -> bool:
        try:
            self.reader, self.writer = await asyncio.open_connection(
                self.host, self.port
            )

            # 핸드셰이크
            hello = json.dumps({
                "type": "hello",
                "protocol_version": "1.0",
                "client": "n1-unreal-mcp-bridge",
                "client_version": "1.0.0",
            }) + "\n"
            self.writer.write(hello.encode())
            await self.writer.drain()

            line = await asyncio.wait_for(self.reader.readline(), timeout=5.0)
            if not line:
                raise ConnectionError("No handshake response")

            ack = json.loads(line.decode())

            if ack.get("type") == "error":
                raise ConnectionError(
                    f"Handshake rejected: {ack.get('error', 'unknown')}"
                )

            if ack.get("type") != "hello_ack":
                raise ConnectionError(f"Unexpected handshake response: {ack}")

            # protocol_version 검증
            server_version = ack.get("protocol_version", "")
            if server_version != "1.0":
                raise ConnectionError(
                    f"Protocol version mismatch: server={server_version}, client=1.0"
                )

            # capabilities 저장
            self.capabilities = ack.get("capabilities", [])

            self._connected = True
            self._receive_task = asyncio.create_task(self._receive_loop())
            return True

        except Exception as e:
            self._connected = False
            raise ConnectionError(f"Failed to connect to UE Editor: {e}") from e

    async def disconnect(self):
        self._connected = False
        if self._receive_task:
            self._receive_task.cancel()
            self._receive_task = None
        if self.writer:
            self.writer.close()
            self.writer = None
        self.reader = None
        # 미완료 요청들을 에러로 resolve
        for future in self.pending.values():
            if not future.done():
                future.set_result({
                    "status": "error",
                    "error": "Connection closed",
                })
        self.pending.clear()

    async def send_command(
        self,
        command: str,
        params: dict[str, Any] | None = None,
        timeout_ms: int = 10000,
    ) -> dict:
        if not self._connected:
            if not await self._reconnect():
                return {
                    "status": "error",
                    "error": "UE Editor is not running. Start the editor first.",
                }

        req_id = str(uuid.uuid4())
        msg = json.dumps({
            "type": "request",
            "id": req_id,
            "command": command,
            "params": params or {},
            "timeout_ms": timeout_ms,
        }) + "\n"

        loop = asyncio.get_event_loop()
        future: asyncio.Future = loop.create_future()
        self.pending[req_id] = future

        try:
            self.writer.write(msg.encode())
            await self.writer.drain()
            result = await asyncio.wait_for(
                future, timeout=timeout_ms / 1000 + 5
            )
            return result
        except asyncio.TimeoutError:
            self.pending.pop(req_id, None)
            return {
                "status": "error",
                "error": "Command timed out. UE Editor may be busy.",
            }
        except Exception as e:
            self.pending.pop(req_id, None)
            return {
                "status": "error",
                "error": f"Connection error: {e}",
            }

    async def _receive_loop(self):
        try:
            while self._connected and self.reader:
                line = await self.reader.readline()
                if not line:
                    break
                msg = json.loads(line.decode())
                msg_id = msg.get("id")
                msg_type = msg.get("type")

                if msg_type == "response" and msg_id in self.pending:
                    self.pending.pop(msg_id).set_result(msg)
                # progress 메시지는 v1에서 무시
        except asyncio.CancelledError:
            pass
        except Exception:
            pass

        self._connected = False

    async def _reconnect(self, retries: int = 3) -> bool:
        for i in range(retries):
            try:
                if await self.connect():
                    return True
            except ConnectionError:
                pass
            if i < retries - 1:
                await asyncio.sleep(1)
        return False


_connection: UEConnection | None = None


def get_connection() -> UEConnection:
    global _connection
    if _connection is None:
        _connection = UEConnection()
    return _connection
