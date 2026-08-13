import sqlite3
import tempfile
import unittest
from pathlib import Path

from Tools.BalanceTelemetry.import_telemetry import (
    export_views,
    import_lines,
    initialize_database,
    parse_telemetry_line,
)


SESSION_START = (
    "[DR_SUMMARY] Telemetry Schema=1 Event=RaidSessionStarted Session=S1 Seq=1 "
    "T=0.000 Environment=PIE BuildVersion=Dev BalanceVersion=B1 "
    "ServerSlot=Local Map=TestMap StartedUtc=2026-08-13T00:00:00Z"
)
SESSION_END = (
    "[DR_SUMMARY] Telemetry Schema=1 Event=RaidEnded Session=S1 Seq=2 "
    "T=30.000 Environment=PIE BuildVersion=Dev BalanceVersion=B1 "
    "Outcome=BossDefeated PlayerCount=1 Duration=30.000 "
    "BossHPRemaining=0 CompletionState=Completed"
)


class TelemetryImportTests(unittest.TestCase):
    def setUp(self):
        self.connection = sqlite3.connect(":memory:")
        initialize_database(self.connection)

    def tearDown(self):
        self.connection.close()

    def test_parser_ignores_legacy_and_rejects_identity_fields(self):
        self.assertIsNone(parse_telemetry_line("[DR_SUMMARY] Attack Accepted: Player=Old"))
        with self.assertRaisesRegex(ValueError, "forbidden identity field"):
            parse_telemetry_line(SESSION_START + " Callsign=Pilot")

    def test_import_is_idempotent_and_marks_completed_session(self):
        first = import_lines(self.connection, "DroneProto.log", [SESSION_START, SESSION_END], "abc123")
        second = import_lines(self.connection, "DroneProto.log", [SESSION_START, SESSION_END], "abc123")

        self.assertEqual(first.inserted, 2)
        self.assertEqual(first.rejected, 0)
        self.assertEqual(second.inserted, 0)
        row = self.connection.execute(
            "SELECT completion_state, outcome, duration FROM sessions WHERE session_id='S1'"
        ).fetchone()
        self.assertEqual(row, ("Completed", "BossDefeated", 30.0))

    def test_session_without_end_remains_aborted(self):
        import_lines(self.connection, "crashed.log", [SESSION_START], "abc123")
        state = self.connection.execute(
            "SELECT completion_state FROM sessions WHERE session_id='S1'"
        ).fetchone()[0]
        self.assertEqual(state, "Aborted")

    def test_rotated_log_reusing_file_name_and_line_numbers_imports_new_session(self):
        import_lines(self.connection, "DroneProto.log", [SESSION_START, SESSION_END], "abc123")
        second_session = [line.replace("Session=S1", "Session=S2") for line in (SESSION_START, SESSION_END)]
        stats = import_lines(self.connection, "DroneProto.log", second_session, "def456")

        self.assertEqual(stats.inserted, 2)
        self.assertEqual(
            self.connection.execute("SELECT COUNT(*) FROM sessions").fetchone()[0],
            2,
        )

    def test_export_creates_balancing_csv_files(self):
        import_lines(self.connection, "DroneProto.log", [SESSION_START, SESSION_END], "abc123")
        with tempfile.TemporaryDirectory() as directory:
            exported = export_views(self.connection, Path(directory))
            self.assertEqual(
                {path.name for path in exported},
                {"loadout_balance.csv", "pattern_balance.csv", "raid_balance.csv"},
            )
            self.assertIn("balance_version", (Path(directory) / "raid_balance.csv").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
