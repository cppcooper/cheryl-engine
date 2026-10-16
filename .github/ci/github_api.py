"""GitHub.com API access for repository-owned workflow automation."""

import json
import os
from pathlib import Path
import re
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode, urlsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener


class GitHubError(RuntimeError):
    def __init__(self, method, path, status):
        self.status = status
        super().__init__(f"GitHub API {method} {path} returned HTTP {status}")


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, request, file, code, message, headers, new_url):
        return None


class GitHubApi:
    def __init__(self, repository, token):
        if (not isinstance(repository, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository)
                or any(part in {".", ".."} for part in repository.split("/"))):
            raise ValueError("GITHUB_REPOSITORY must be owner/repository")
        if not isinstance(token, str) or not token or any(character.isspace() for character in token):
            raise ValueError("A nonempty GitHub token without whitespace is required")
        self.repository = repository
        self.token = token
        self.base = "https://api.github.com/repos/" + repository
        self.opener = build_opener(NoRedirect())

    def _send(self, method, path, url, data=None, *, content_type="application/json", length=None, missing_ok=False):
        headers = {"Accept": "application/vnd.github+json", "Authorization": "Bearer " + self.token,
                   "X-GitHub-Api-Version": "2026-03-10", "User-Agent": "cheryl-release-automation"}
        if data is not None:
            headers["Content-Type"] = content_type
        if length is not None:
            headers["Content-Length"] = str(length)
        request = Request(url, method=method, data=data, headers=headers)
        try:
            with self.opener.open(request, timeout=120) as response:
                contents = response.read()
        except HTTPError as error:
            status = error.code
            error.close()
            if status == 404 and missing_ok:
                return None
            raise GitHubError(method, path, status) from None
        except (URLError, OSError):
            raise RuntimeError(f"GitHub API {method} {path} could not be reached") from None
        result = json.loads(contents) if contents else None
        if result is not None and not isinstance(result, (dict, list)):
            raise ValueError("The GitHub API returned an unexpected response")
        return result

    def request(self, method, path, body=None, *, missing_ok=False, **query):
        if (method not in {"GET", "POST", "PATCH", "PUT", "DELETE"} or not isinstance(path, str)
                or not path.startswith("/") or any(character.isspace() for character in path)
                or any(value in path for value in ("//", "?", "#")) or ".." in path.split("/")):
            raise ValueError("Expected a repository-relative GitHub API request")
        data = json.dumps(body).encode("utf-8") if body is not None else None
        url = self.base + path + ("?" + urlencode(query) if query else "")
        return self._send(method, path, url, data, missing_ok=missing_ok)

    def pages(self, path, field=None, **query):
        entries = []
        for page in range(1, 1001):
            value = self.request("GET", path, per_page=100, page=page, **query)
            batch = value.get(field) if field and isinstance(value, dict) else value
            if not isinstance(batch, list):
                raise ValueError("The GitHub API returned an unexpected page")
            entries.extend(batch)
            if len(batch) < 100:
                return entries
        raise RuntimeError("GitHub API pagination exceeded the automation limit")

    def upload(self, upload_url, path):
        url = upload_url.split("{", 1)[0]
        parsed = urlsplit(url)
        if (parsed.scheme != "https" or parsed.netloc != "uploads.github.com" or parsed.query or parsed.fragment
                or not re.fullmatch(r"/repos/" + re.escape(self.repository) + r"/releases/[1-9][0-9]*/assets", parsed.path)):
            raise ValueError("Expected this repository's GitHub release upload URL")
        path = Path(path)
        with path.open("rb") as source:
            size = os.fstat(source.fileno()).st_size
            result = self._send("POST", parsed.path, url + "?" + urlencode({"name": path.name}), source,
                                content_type="application/octet-stream", length=size)
        if not isinstance(result, dict):
            raise ValueError("The GitHub API did not confirm the uploaded asset")
        return result
