#!/usr/bin/env python3

import argparse
import ssl
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit


class EndpointReachabilityHandler(BaseHTTPRequestHandler):
	protocol_version = "HTTP/1.1"

	def do_GET(self) -> None:
		request = urlsplit(self.path)
		query = parse_qs(request.query)
		if request.path == "/ok":
			self.send_response_with_body(204, b"")
			return
		if request.path.startswith("/status/"):
			try:
				status = int(request.path.removeprefix("/status/"))
			except ValueError:
				status = 400
			status = max(200, min(status, 599))
			self.send_response_with_body(status, f"status {status}\n".encode())
			return
		if request.path == "/redirect":
			self.send_response(302)
			self.send_header("Location", "/ok")
			self.send_header("Content-Length", "0")
			self.end_headers()
			return
		if request.path == "/large":
			byte_count = self.bounded_number(query, "bytes", 65537, 1, 2 * 1024 * 1024)
			self.send_response_with_body(200, b"x" * byte_count)
			return
		if request.path == "/slow":
			delay = self.bounded_number(query, "seconds", 2.0, 0.0, 30.0)
			time.sleep(delay)
			self.send_response_with_body(200, b"slow response\n")
			return
		self.send_response_with_body(404, b"not found\n")

	@staticmethod
	def bounded_number(query, name, default, minimum, maximum):
		try:
			value = type(default)(query.get(name, [default])[0])
		except (TypeError, ValueError):
			value = default
		return max(minimum, min(value, maximum))

	def send_response_with_body(self, status: int, body: bytes) -> None:
		self.send_response(status)
		self.send_header("Content-Type", "text/plain")
		self.send_header("Content-Length", str(len(body)))
		self.end_headers()
		try:
			self.wfile.write(body)
		except (BrokenPipeError, ConnectionResetError):
			pass

	def log_message(self, format: str, *args) -> None:
		pass


def create_http_server(host: str, port: int) -> ThreadingHTTPServer:
	server = ThreadingHTTPServer((host, port), EndpointReachabilityHandler)
	server.daemon_threads = True
	return server


def create_https_server(
	host: str,
	port: int,
	certificate: Path,
	private_key: Path,
) -> ThreadingHTTPServer:
	server = create_http_server(host, port)
	context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
	context.load_cert_chain(certificate, private_key)
	server.socket = context.wrap_socket(server.socket, server_side=True)
	return server


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("--host", default="0.0.0.0")
	parser.add_argument("--port", type=int, default=8443)
	parser.add_argument("--certificate", type=Path, required=True)
	parser.add_argument("--private-key", type=Path, required=True)
	arguments = parser.parse_args()
	server = create_https_server(
		arguments.host,
		arguments.port,
		arguments.certificate,
		arguments.private_key,
	)
	print(f"HTTPS fixture listening on {arguments.host}:{server.server_port}")
	try:
		server.serve_forever()
	except KeyboardInterrupt:
		pass
	finally:
		server.server_close()


if __name__ == "__main__":
	main()
