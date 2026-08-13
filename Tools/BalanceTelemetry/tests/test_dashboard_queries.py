import sqlite3
import unittest

from Tools.BalanceTelemetry.dashboard import DashboardFilters, load_raid_rows, load_summary
from Tools.BalanceTelemetry.import_telemetry import import_lines, initialize_database


def session_lines(session_id: str, environment: str, version: str, outcome: str):
    return [
        f"[DR_SUMMARY] Telemetry Schema=1 Event=RaidSessionStarted Session={session_id} Seq=1 "
        f"T=0 Environment={environment} BuildVersion=Dev BalanceVersion={version} "
        "ServerSlot=A Map=TestMap StartedUtc=2026-08-13T00:00:00Z",
        f"[DR_SUMMARY] Telemetry Schema=1 Event=RaidEnded Session={session_id} Seq=2 "
        f"T=20 Environment={environment} BuildVersion=Dev BalanceVersion={version} "
        f"Outcome={outcome} PlayerCount=1 Duration=20 BossHPRemaining=0 CompletionState=Completed",
    ]


class DashboardQueryTests(unittest.TestCase):
    def test_summary_filters_automation_and_marks_small_sample(self):
        connection = sqlite3.connect(":memory:")
        initialize_database(connection)
        import_lines(connection, "pie.log", session_lines("PIE1", "PIE", "B1", "BossDefeated"), "c1")
        import_lines(connection, "auto.log", session_lines("AUTO1", "Automation", "B1", "TimeOver"), "c1")

        summary = load_summary(
            connection,
            DashboardFilters(environments=("PIE",), balance_versions=("B1",)),
            minimum_completed_sessions=20,
        )

        self.assertEqual(summary.completed_sessions, 1)
        self.assertEqual(summary.clear_rate, 1.0)
        self.assertTrue(summary.sample_warning)
        rows = load_raid_rows(
            connection,
            DashboardFilters(environments=("PIE",), balance_versions=("B1",)),
        )
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["environment"], "PIE")
        connection.close()


if __name__ == "__main__":
    unittest.main()
