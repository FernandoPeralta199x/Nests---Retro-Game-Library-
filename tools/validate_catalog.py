#!/usr/bin/env python3
"""Validate a NESTS v2 catalog and fail closed on unsafe download records."""

from __future__ import annotations

import argparse
import json
import re
import sys
from datetime import datetime
from pathlib import Path
from urllib.parse import urlsplit

MAX_CATALOG_ITEMS = 20_000
MAX_ITEM_SIZE = 2 * 1024 * 1024 * 1024
ID_RE = re.compile(r"^[a-z0-9][a-z0-9-]{1,79}$")
SHA256_RE = re.compile(r"^[a-fA-F0-9]{64}$")
STATUSES = {"demo_only", "verified"}


class CatalogError(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise CatalogError(message)


def https_url(value: object, field: str) -> str:
    require(isinstance(value, str) and len(value) <= 2048, f"{field}: URL ausente ou inválida")
    parsed = urlsplit(value)
    require(parsed.scheme == "https", f"{field}: somente HTTPS é permitido")
    require(bool(parsed.hostname), f"{field}: hostname ausente")
    require(parsed.username is None and parsed.password is None, f"{field}: credenciais embutidas não são permitidas")
    try:
        port = parsed.port
    except ValueError as exc:
        raise CatalogError(f"{field}: porta inválida") from exc
    require(port in (None, 443), f"{field}: porta não padrão não é permitida")
    require(not parsed.fragment, f"{field}: fragmentos não são permitidos")
    return parsed.hostname.lower().rstrip(".")


def validate_catalog(catalog: object) -> list[str]:
    require(isinstance(catalog, dict), "catálogo deve ser um objeto JSON")
    require(catalog.get("schema_version") == 2, "schema_version deve ser 2")
    require(isinstance(catalog.get("catalog_id"), str) and catalog["catalog_id"], "catalog_id obrigatório")
    version = catalog.get("catalog_version")
    require(isinstance(version, int) and not isinstance(version, bool) and version > 0, "catalog_version inválido")
    require(isinstance(catalog.get("catalog_name"), str) and catalog["catalog_name"], "catalog_name obrigatório")
    try:
        generated = datetime.fromisoformat(str(catalog.get("generated_at", "")).replace("Z", "+00:00"))
    except ValueError as exc:
        raise CatalogError("generated_at deve ser ISO-8601") from exc
    require(generated.tzinfo is not None, "generated_at deve conter timezone")

    hosts = catalog.get("trusted_download_hosts")
    require(isinstance(hosts, list), "trusted_download_hosts deve ser uma lista")
    trusted_hosts: set[str] = set()
    for host in hosts:
        require(isinstance(host, str) and host == host.lower() and host == host.strip(), "host confiável inválido")
        require("." in host and "/" not in host and "*" not in host and ":" not in host, "host confiável deve ser hostname exato")
        require(host not in trusted_hosts, f"host confiável duplicado: {host}")
        trusted_hosts.add(host)

    items = catalog.get("items")
    require(isinstance(items, list), "items deve ser uma lista")
    require(len(items) <= MAX_CATALOG_ITEMS, "catálogo excede o limite de itens")
    seen_ids: set[str] = set()

    for index, item in enumerate(items):
        prefix = f"items[{index}]"
        require(isinstance(item, dict), f"{prefix}: registro deve ser objeto")
        item_id = item.get("id")
        require(isinstance(item_id, str) and ID_RE.fullmatch(item_id), f"{prefix}: id inválido")
        require(item_id not in seen_ids, f"{prefix}: id duplicado: {item_id}")
        seen_ids.add(item_id)
        for field in ("title", "system", "genre", "description", "format", "source_name"):
            require(isinstance(item.get(field), str) and item[field].strip(), f"{prefix}: {field} obrigatório")
            require(len(item[field]) <= 4000, f"{prefix}: {field} muito longo")
        year = item.get("year")
        require(isinstance(year, int) and not isinstance(year, bool) and 1970 <= year <= 2100, f"{prefix}: year inválido")
        size = item.get("size_bytes")
        require(isinstance(size, int) and not isinstance(size, bool) and 0 <= size <= MAX_ITEM_SIZE, f"{prefix}: size_bytes inválido")

        cover = item.get("cover_url")
        if cover is not None:
            https_url(cover, f"{prefix}.cover_url")
        source = item.get("source_url")
        if source is not None:
            https_url(source, f"{prefix}.source_url")

        status = item.get("distribution_status")
        require(isinstance(status, str) and status in STATUSES, f"{prefix}: distribution_status deve ser demo_only ou verified")
        rights = item.get("rights")
        require(isinstance(rights, dict), f"{prefix}: rights deve ser objeto")
        redistribution = rights.get("redistribution_allowed")
        require(type(redistribution) is bool, f"{prefix}: redistribution_allowed deve ser boolean")

        download = item.get("download_url")
        digest = item.get("sha256")
        if status == "demo_only":
            require(download is None, f"{prefix}: item demo_only não pode ter download_url")
            require(redistribution is False, f"{prefix}: item demo_only não pode permitir redistribuição")
            require(digest is None, f"{prefix}: item demo_only não deve declarar SHA-256")
            continue

        require(redistribution is True, f"{prefix}: item verified exige permissão explícita de redistribuição")
        require(isinstance(rights.get("license_id"), str) and rights["license_id"].strip(), f"{prefix}: license_id obrigatório")
        https_url(rights.get("license_url"), f"{prefix}.rights.license_url")
        https_url(rights.get("evidence_url"), f"{prefix}.rights.evidence_url")
        require(source is not None, f"{prefix}: source_url obrigatório para item verified")
        require(isinstance(download, str), f"{prefix}: download_url obrigatório para item verified")
        host = https_url(download, f"{prefix}.download_url")
        require(host in trusted_hosts, f"{prefix}: host de download não está na allowlist: {host}")
        require(isinstance(size, int) and size > 0, f"{prefix}: size_bytes deve ser maior que zero")
        require(isinstance(digest, str) and SHA256_RE.fullmatch(digest), f"{prefix}: SHA-256 inválido ou ausente")

    return []


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalog", nargs="?", default="catalogs/stable.json", type=Path)
    args = parser.parse_args()
    try:
        data = json.loads(args.catalog.read_text(encoding="utf-8"))
        validate_catalog(data)
    except (OSError, json.JSONDecodeError, CatalogError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    print(f"OK: {args.catalog} — {len(data['items'])} registros validados; downloads autorizados: "
          f"{sum(1 for item in data['items'] if item['distribution_status'] == 'verified')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
