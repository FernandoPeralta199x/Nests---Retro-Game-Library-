import copy
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from validate_catalog import CatalogError, validate_catalog


class CatalogValidationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.demo = json.loads((ROOT / "catalogs" / "stable.json").read_text(encoding="utf-8"))

    def test_current_demo_catalog_is_valid(self):
        self.assertEqual(validate_catalog(self.demo), [])

    def verified_record(self):
        item = copy.deepcopy(self.demo["items"][0])
        item.update({
            "distribution_status": "verified",
            "size_bytes": 12345,
            "source_url": "https://publisher.example/game",
            "download_url": "https://downloads.example/game.nes",
            "sha256": "a" * 64,
            "rights": {
                "license_id": "CC0-1.0",
                "license_url": "https://publisher.example/license",
                "evidence_url": "https://publisher.example/redistribution",
                "redistribution_allowed": True,
            },
        })
        return item

    def test_verified_item_requires_allowlisted_https_host_and_rights(self):
        data = copy.deepcopy(self.demo)
        data["trusted_download_hosts"] = ["downloads.example"]
        data["items"] = [self.verified_record()]
        self.assertEqual(validate_catalog(data), [])

    def test_demo_item_cannot_have_download_link(self):
        data = copy.deepcopy(self.demo)
        data["items"][0]["download_url"] = "https://downloads.example/file"
        with self.assertRaisesRegex(CatalogError, "demo_only"):
            validate_catalog(data)

    def test_verified_item_without_redistribution_permission_is_rejected(self):
        data = copy.deepcopy(self.demo)
        item = self.verified_record()
        item["rights"]["redistribution_allowed"] = False
        data["trusted_download_hosts"] = ["downloads.example"]
        data["items"] = [item]
        with self.assertRaisesRegex(CatalogError, "permissão explícita"):
            validate_catalog(data)

    def test_untrusted_download_host_is_rejected(self):
        data = copy.deepcopy(self.demo)
        data["trusted_download_hosts"] = ["other.example"]
        data["items"] = [self.verified_record()]
        with self.assertRaisesRegex(CatalogError, "allowlist"):
            validate_catalog(data)

    def test_http_download_is_rejected(self):
        data = copy.deepcopy(self.demo)
        item = self.verified_record()
        item["download_url"] = "http://downloads.example/file"
        data["trusted_download_hosts"] = ["downloads.example"]
        data["items"] = [item]
        with self.assertRaisesRegex(CatalogError, "HTTPS"):
            validate_catalog(data)

    def test_duplicate_game_ids_are_rejected(self):
        data = copy.deepcopy(self.demo)
        data["items"].append(copy.deepcopy(data["items"][0]))
        with self.assertRaisesRegex(CatalogError, "duplicado"):
            validate_catalog(data)

    def test_shared_catalog_rejects_personal_backup_status(self):
        data = copy.deepcopy(self.demo)
        data["items"][0]["distribution_status"] = "personal_backup"
        with self.assertRaisesRegex(CatalogError, "distribution_status"):
            validate_catalog(data)


if __name__ == "__main__":
    unittest.main()
