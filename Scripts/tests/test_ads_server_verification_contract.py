import json
import unittest
from pathlib import Path
from urllib.parse import parse_qsl


FIXTURE_PATH = (
	Path(__file__).resolve().parent
	/ "fixtures"
	/ "admob_ssv_callbacks.json"
)


def evaluate_callback(case: dict) -> str:
	pairs = parse_qsl(case["raw_query"], keep_blank_values=True)
	keys = [key for key, value in pairs]
	if keys[-2:] != ["signature", "key_id"]:
		return "reject"

	payload = dict(pairs)
	required = {
		"ad_unit",
		"reward_amount",
		"reward_item",
		"timestamp",
		"transaction_id",
		"signature",
		"key_id",
	}
	if not required.issubset(payload):
		return "reject"
	if not case["signature_valid"]:
		return "reject"
	if payload["ad_unit"] != case["expected_ad_unit"]:
		return "reject"
	try:
		if int(payload["reward_amount"]) <= 0:
			return "reject"
	except ValueError:
		return "reject"
	if payload["transaction_id"] in case["seen_transaction_ids"]:
		return "already_processed"
	return "grant"


class AdsServerVerificationContractTests(unittest.TestCase):
	@classmethod
	def setUpClass(cls) -> None:
		with FIXTURE_PATH.open(encoding="utf-8") as fixture_file:
			cls.cases = json.load(fixture_file)

	def test_valid_invalid_duplicate_and_delayed_callbacks(self) -> None:
		self.assertEqual(
			{"valid", "invalid", "duplicate", "delayed"},
			{case["name"] for case in self.cases},
		)
		for case in self.cases:
			with self.subTest(case=case["name"]):
				self.assertEqual(case["expected_result"], evaluate_callback(case))

	def test_delayed_valid_callback_is_not_rejected_by_age_alone(self) -> None:
		delayed = next(case for case in self.cases if case["name"] == "delayed")
		self.assertGreater(
			delayed["received_timestamp_ms"] - delayed["issued_timestamp_ms"],
			24 * 60 * 60 * 1000,
		)
		self.assertEqual("grant", evaluate_callback(delayed))

	def test_fixture_contains_no_backend_credentials(self) -> None:
		serialized = json.dumps(self.cases).lower()
		for forbidden in (
			"private_key",
			"client_secret",
			"api_secret",
			"callback_secret",
		):
			self.assertNotIn(forbidden, serialized)


if __name__ == "__main__":
	unittest.main()
