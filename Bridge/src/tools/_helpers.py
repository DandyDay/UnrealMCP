import json
from typing import Any

from src.ue_connection import get_connection


async def cmd(command: str, params: dict[str, Any] | None = None, timeout_ms: int = 10000) -> str:
    """UE 커맨드를 실행하고 결과만 반환 (TCP 프로토콜 필드 제거)."""
    conn = get_connection()
    raw = await conn.send_command(command, params, timeout_ms)
    # 성공: result 내용만 추출 (status/type/id 제거)
    if raw.get("status") == "success" and "result" in raw:
        return json.dumps(raw["result"], ensure_ascii=False)
    # 에러: TCP 필드(type, id) 제거, error_code + error만
    return json.dumps(
        {k: v for k, v in raw.items() if k not in ("type", "id")},
        ensure_ascii=False,
    )
