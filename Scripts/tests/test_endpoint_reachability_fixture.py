import http.client
import importlib.util
import threading
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
FIXTURE_PATH = (
	REPOSITORY_ROOT
	/ "Native"
	/ "OpenMobileDevice"
	/ "Tests"
	/ "Fixtures"
	/ "endpoint_reachability_server.py"
)


def load_fixture_module():
	spec = importlib.util.spec_from_file_location(
		"endpoint_reachability_server",
		FIXTURE_PATH,
	)
	module = importlib.util.module_from_spec(spec)
	assert spec.loader is not None
	spec.loader.exec_module(module)
	return module


class EndpointReachabilityFixtureTests(unittest.TestCase):
	@classmethod
	def setUpClass(cls) -> None:
		cls.fixture = load_fixture_module()
		cls.server = cls.fixture.create_http_server("127.0.0.1", 0)
		cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
		cls.thread.start()
		cls.port = cls.server.server_address[1]

	@classmethod
	def tearDownClass(cls) -> None:
		cls.server.shutdown()
		cls.server.server_close()
		cls.thread.join(timeout=2)

	def request(self, path: str) -> tuple[int, dict[str, str], bytes]:
		connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=2)
		connection.request("GET", path)
		response = connection.getresponse()
		body = response.read()
		headers = {key.casefold(): value for key, value in response.getheaders()}
		connection.close()
		return response.status, headers, body

	def test_success_and_rejected_status_routes(self) -> None:
		status, _, body = self.request("/ok")
		self.assertEqual(204, status)
		self.assertEqual(b"", body)

		status, _, body = self.request("/status/503")
		self.assertEqual(503, status)
		self.assertEqual(b"status 503\n", body)

	def test_redirect_and_oversized_routes(self) -> None:
		status, headers, _ = self.request("/redirect")
		self.assertEqual(302, status)
		self.assertEqual("/ok", headers["location"])

		status, _, body = self.request("/large?bytes=65537")
		self.assertEqual(200, status)
		self.assertEqual(65537, len(body))

	def test_slow_route_is_bounded_for_tests(self) -> None:
		status, _, body = self.request("/slow?seconds=0.01")
		self.assertEqual(200, status)
		self.assertEqual(b"slow response\n", body)


if __name__ == "__main__":
	unittest.main()
