"""Check workflow API routing and token handling without network access."""

import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import MagicMock
from urllib.error import HTTPError

from github_api import GitHubApi, GitHubError


class ApiTests(unittest.TestCase):
    def setUp(self):
        self.token = "ghs_123_" + "a" * 250 + "." + "b" * 250 + ".signature"
        self.api = GitHubApi("cppcooper/cheryl-engine", self.token)
        self.api.opener = MagicMock()

    def respond(self, contents=b'{}'):
        self.api.opener.open.return_value.__enter__.return_value.read.return_value = contents

    def test_token(self):
        self.respond()
        self.api.request("POST", "/releases", {"tag_name": "v4.0.0"})
        request = self.api.opener.open.call_args.args[0]
        self.assertEqual(request.get_header("Authorization"), "Bearer " + self.token)
        self.assertEqual(request.full_url, "https://api.github.com/repos/cppcooper/cheryl-engine/releases")
        self.assertIn(b'v4.0.0', request.data)

    def test_paths(self):
        for path in ("https://example.com", "//example.com", "/releases?token=x", "/../outside", "/bad\npath"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                self.api.request("GET", path)
        self.api.opener.open.assert_not_called()

    def test_missing(self):
        self.api.opener.open.side_effect = HTTPError("https://api.github.com", 404, "missing", {}, io.BytesIO())
        self.assertIsNone(self.api.request("GET", "/git/ref/tags/v4.0.0", missing_ok=True))
        with self.assertRaises(GitHubError) as failure:
            self.api.request("GET", "/releases")
        self.assertEqual(failure.exception.status, 404)
        self.assertNotIn(self.token, str(failure.exception))

    def test_no_content(self):
        self.respond(b'')
        self.assertIsNone(self.api.request("DELETE", "/releases/assets/3"))

    def test_pages(self):
        with unittest.mock.patch.object(self.api, "request", side_effect=[list(range(100)), [100]]) as request:
            self.assertEqual(self.api.pages("/releases"), list(range(101)))
            self.assertEqual(request.call_args_list[1].kwargs, {"per_page": 100, "page": 2})

    def test_upload(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "SHA256SUMS"
            path.write_bytes(b"checked")
            self.respond(b'{"id": 1}')
            self.assertEqual(self.api.upload("https://uploads.github.com/repos/cppcooper/cheryl-engine/releases/3/assets{?name,label}", path), {"id": 1})
            request = self.api.opener.open.call_args.args[0]
            self.assertEqual(request.get_header("Content-length"), "7")
            self.assertTrue(request.full_url.endswith("?name=SHA256SUMS"))

    def test_upload_scope(self):
        for url in ("http://uploads.github.com/repos/cppcooper/cheryl-engine/releases/3/assets",
                    "https://uploads.github.com/repos/another/repo/releases/3/assets",
                    "https://example.com/repos/cppcooper/cheryl-engine/releases/3/assets"):
            with self.subTest(url=url), self.assertRaises(ValueError):
                self.api.upload(url, "/unused")
        self.api.opener.open.assert_not_called()


if __name__ == "__main__":
    unittest.main()
