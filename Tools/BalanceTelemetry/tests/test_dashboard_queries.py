import sqlite3
import unittest

from Tools.BalanceTelemetry.dashboard import (
    DashboardFilters,
    load_attack_result_rows,
    load_attack_rows,
    load_damage_rows,
    load_dodge_rows,
    load_raid_rows,
    load_raw_event_rows,
    load_summary,
    load_survival_rows,
)
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


def event_line(session_id: str, sequence: int, event: str, payload: str) -> str:
    return (
        f"[DR_SUMMARY] Telemetry Schema=1 Event={event} Session={session_id} Seq={sequence} "
        f"T={sequence} Environment=PIE BuildVersion=Dev BalanceVersion=B1 {payload}"
    )


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

    def test_completed_only_toggle_includes_aborted_sessions(self):
        connection = sqlite3.connect(":memory:")
        initialize_database(connection)
        import_lines(connection, "complete.log", session_lines("DONE", "PIE", "B1", "BossDefeated"), "c1")
        import_lines(connection, "aborted.log", [session_lines("STOP", "PIE", "B1", "TimeOver")[0]], "c1")

        completed = load_raw_event_rows(connection, DashboardFilters(environments=("PIE",)))
        all_sessions = load_raw_event_rows(
            connection,
            DashboardFilters(environments=("PIE",), completed_only=False),
        )

        self.assertEqual({row["session_id"] for row in completed}, {"DONE"})
        self.assertEqual({row["session_id"] for row in all_sessions}, {"DONE", "STOP"})
        connection.close()

    def test_combat_queries_aggregate_attack_damage_dodge_and_survival(self):
        connection = sqlite3.connect(":memory:")
        initialize_database(connection)
        lines = [session_lines("S1", "PIE", "B1", "BossDefeated")[0]]
        lines.extend([
            event_line("S1", 2, "LoadoutLocked", "Player=P1 Core=Zenith Left=PulseLaser Right=PulseLaser"),
            event_line("S1", 3, "AttackAttempted", "Player=P1 Core=Zenith Left=PulseLaser Right=PulseLaser DistanceCm=100"),
            event_line("S1", 4, "AttackResolved", "Player=P1 Result=Hit Reason=None Core=Zenith Left=PulseLaser Right=PulseLaser AppliedDamage=12"),
            event_line("S1", 5, "AttackAttempted", "Player=P1 Core=Zenith Left=PulseLaser Right=PulseLaser DistanceCm=100"),
            event_line("S1", 6, "AttackResolved", "Player=P1 Result=Rejected Reason=Dodging Core=Zenith Left=PulseLaser Right=PulseLaser AppliedDamage=0"),
            event_line("S1", 7, "PlayerDamageResolved", "Player=P1 Cause=Corrupted Result=Hit AppliedDamage=10 HPBefore=100 HPAfter=90"),
            event_line("S1", 8, "DodgeResolved", "Player=P1 Result=Accepted Reason=None DistanceMeters=5"),
            event_line("S1", 9, "DodgeResolved", "Player=P1 Result=Rejected Reason=Cooldown DistanceMeters=0"),
            event_line("S1", 10, "DroneReportCreated", "Player=P1 SurvivalTime=30 BossDamage=12 MoveDistance=20 HealAmount=0 DamageTakenCount=1 BonusScore=10 Grade=A ReportScore=80"),
            session_lines("S1", "PIE", "B1", "BossDefeated")[1].replace("Seq=2", "Seq=11"),
        ])
        import_lines(connection, "combat.log", lines, "c1")
        filters = DashboardFilters(environments=("PIE",))

        attack = load_attack_rows(connection, filters)[0]
        self.assertEqual((attack["attempts"], attack["hits"], attack["rejected"]), (2, 1, 1))
        self.assertEqual(attack["avg_applied_damage"], 6.0)
        self.assertEqual(
            {row["result"]: row["events"] for row in load_attack_result_rows(connection, filters)},
            {"Hit": 1, "Rejected": 1},
        )
        damage = load_damage_rows(connection, filters)[0]
        self.assertEqual((damage["cause"], damage["events"], damage["total_applied_damage"]), ("Corrupted", 1, 10.0))
        dodge = {row["result"]: row for row in load_dodge_rows(connection, filters)}
        self.assertEqual(dodge["Accepted"]["avg_distance_meters"], 5.0)
        self.assertEqual(dodge["Rejected"]["reason"], "Cooldown")
        survival = load_survival_rows(connection, filters)[0]
        self.assertEqual((survival["avg_survival_time"], survival["avg_boss_damage"]), (30.0, 12.0))
        connection.close()

    def test_raw_events_are_limited_to_latest_500(self):
        connection = sqlite3.connect(":memory:")
        initialize_database(connection)
        lines = [session_lines("S500", "PIE", "B1", "BossDefeated")[0]]
        for sequence in range(2, 512):
            lines.append(event_line("S500", sequence, "AttackAttempted", "Player=P1 Core=None Left=None Right=None"))
        lines.append(session_lines("S500", "PIE", "B1", "BossDefeated")[1].replace("Seq=2", "Seq=512"))
        import_lines(connection, "large.log", lines, "c1")

        rows = load_raw_event_rows(connection, DashboardFilters(environments=("PIE",)))

        self.assertEqual(len(rows), 500)
        self.assertEqual(rows[0]["sequence"], 512)
        self.assertEqual(rows[-1]["sequence"], 13)
        connection.close()


if __name__ == "__main__":
    unittest.main()
