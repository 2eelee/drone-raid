from __future__ import annotations

import argparse
import csv
import json
import sqlite3
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


MARKER = "[DR_SUMMARY] Telemetry "
FORBIDDEN_KEYS = {
    "callsign", "uniquenetid", "uid", "pid", "playerid", "playername",
    "pcname", "account", "ip", "address",
}
COMMON_KEYS = {
    "Schema", "Event", "Session", "Seq", "T", "Environment",
    "BuildVersion", "BalanceVersion",
}
EXPORT_VIEWS = ("loadout_balance", "pattern_balance", "raid_balance")


@dataclass(frozen=True)
class ImportStats:
    inserted: int = 0
    rejected: int = 0
    ignored: int = 0


def parse_telemetry_line(line: str) -> dict[str, str] | None:
    if MARKER not in line:
        return None
    payload = line.split(MARKER, 1)[1].strip()
    fields = {}
    for token in payload.split():
        if "=" in token:
            key, value = token.split("=", 1)
            fields[key] = value
    if fields.get("Schema") != "1":
        return None
    if FORBIDDEN_KEYS.intersection(key.lower() for key in fields):
        raise ValueError("forbidden identity field")
    required = COMMON_KEYS
    missing = sorted(required.difference(fields))
    if missing:
        raise ValueError(f"missing required fields: {', '.join(missing)}")
    return fields


def initialize_database(connection: sqlite3.Connection) -> None:
    schema_path = Path(__file__).with_name("schema.sql")
    existing_columns = {
        row[1] for row in connection.execute("PRAGMA table_info(telemetry_events)")
    }
    legacy_events = bool(existing_columns) and "event_id" not in existing_columns
    if legacy_events:
        connection.executescript(
            """
            DROP VIEW IF EXISTS loadout_balance;
            DROP VIEW IF EXISTS pattern_balance;
            DROP VIEW IF EXISTS raid_balance;
            DROP INDEX IF EXISTS idx_events_session_type;
            ALTER TABLE telemetry_events RENAME TO telemetry_events_legacy;
            """
        )
    connection.executescript(schema_path.read_text(encoding="utf-8"))
    if legacy_events:
        connection.executescript(
            """
            INSERT OR IGNORE INTO telemetry_events(
                source_file, source_line, source_commit, schema_version, session_id,
                sequence, event_time, environment, event_type, player_id, payload_json
            )
            SELECT source_file, source_line, source_commit, schema_version, session_id,
                   sequence, event_time, environment, event_type, player_id, payload_json
            FROM telemetry_events_legacy;
            DROP TABLE telemetry_events_legacy;
            """
        )
    connection.execute("PRAGMA user_version = 2")
    connection.commit()


def _float(fields: dict[str, str], key: str) -> float | None:
    value = fields.get(key)
    return float(value) if value not in (None, "", "None") else None


def _int(fields: dict[str, str], key: str) -> int | None:
    value = fields.get(key)
    return int(value) if value not in (None, "", "None") else None


def _ensure_session(connection: sqlite3.Connection, fields: dict[str, str], source_commit: str) -> None:
    connection.execute(
        """
        INSERT OR IGNORE INTO sessions(
            session_id, environment, build_version, balance_version, source_commit, completion_state
        ) VALUES (?, ?, ?, ?, ?, 'Aborted')
        """,
        (
            fields["Session"], fields["Environment"], fields["BuildVersion"],
            fields["BalanceVersion"], source_commit,
        ),
    )


def _materialize_event(connection: sqlite3.Connection, fields: dict[str, str], source_commit: str) -> None:
    _ensure_session(connection, fields, source_commit)
    event = fields["Event"]
    session = fields["Session"]
    player = fields.get("Player")

    if event == "RaidSessionStarted":
        connection.execute(
            """
            UPDATE sessions SET environment=?, server_slot=?, map_name=?, build_version=?,
                balance_version=?, source_commit=?, started_utc=? WHERE session_id=?
            """,
            (
                fields["Environment"], fields.get("ServerSlot"), fields.get("Map"),
                fields["BuildVersion"], fields["BalanceVersion"], source_commit,
                fields.get("StartedUtc"), session,
            ),
        )
    elif event == "RaidEnded":
        connection.execute(
            """
            UPDATE sessions SET duration=?, outcome=?, completion_state=?, player_count=?,
                boss_hp_remaining=? WHERE session_id=?
            """,
            (
                _float(fields, "Duration"), fields.get("Outcome"),
                fields.get("CompletionState", "Completed"), _int(fields, "PlayerCount"),
                _float(fields, "BossHPRemaining"), session,
            ),
        )
    elif player:
        connection.execute(
            "INSERT OR IGNORE INTO players(session_id, player_id) VALUES (?, ?)",
            (session, player),
        )
        if event == "LoadoutLocked":
            connection.execute(
                "UPDATE players SET core_part=?, left_part=?, right_part=? WHERE session_id=? AND player_id=?",
                (fields.get("Core"), fields.get("Left"), fields.get("Right"), session, player),
            )
        elif event == "DroneReportCreated":
            connection.execute(
                """
                UPDATE players SET survival_time=?, boss_damage=?, move_distance=?, heal_amount=?,
                    damage_taken_count=?, bonus_score=?, grade=?, report_score=?
                WHERE session_id=? AND player_id=?
                """,
                (
                    _float(fields, "SurvivalTime"), _float(fields, "BossDamage"),
                    _float(fields, "MoveDistance"), _float(fields, "HealAmount"),
                    _int(fields, "DamageTakenCount"), _int(fields, "BonusScore"),
                    fields.get("Grade"), _float(fields, "ReportScore"), session, player,
                ),
            )


def import_lines(
    connection: sqlite3.Connection,
    source_file: str,
    lines: Iterable[str],
    source_commit: str,
) -> ImportStats:
    inserted = rejected = ignored = 0
    with connection:
        for line_number, line in enumerate(lines, 1):
            try:
                fields = parse_telemetry_line(line)
                if fields is None:
                    ignored += 1
                    continue
                payload = {key: value for key, value in fields.items() if key not in COMMON_KEYS}
                cursor = connection.execute(
                    """
                    INSERT OR IGNORE INTO telemetry_events(
                        source_file, source_line, source_commit, schema_version, session_id,
                        sequence, event_time, environment, event_type, player_id, payload_json
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        source_file, line_number, source_commit, int(fields["Schema"]),
                        fields["Session"], int(fields["Seq"]), float(fields["T"]),
                        fields["Environment"], fields["Event"], fields.get("Player"),
                        json.dumps(payload, ensure_ascii=True, sort_keys=True),
                    ),
                )
                if cursor.rowcount == 0:
                    continue
                inserted += 1
                _materialize_event(connection, fields, source_commit)
            except (KeyError, TypeError, ValueError, sqlite3.DatabaseError) as error:
                rejected += 1
                print(f"{source_file}:{line_number}: {error}", file=sys.stderr)
    return ImportStats(inserted=inserted, rejected=rejected, ignored=ignored)


def export_views(connection: sqlite3.Connection, output: Path) -> list[Path]:
    output.mkdir(parents=True, exist_ok=True)
    exported = []
    for view in EXPORT_VIEWS:
        cursor = connection.execute(f"SELECT * FROM {view}")
        path = output / f"{view}.csv"
        with path.open("w", newline="", encoding="utf-8-sig") as stream:
            writer = csv.writer(stream)
            writer.writerow(column[0] for column in cursor.description)
            writer.writerows(cursor.fetchall())
        exported.append(path)
    return exported


def _open_database(path: Path) -> sqlite3.Connection:
    path.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(path)
    initialize_database(connection)
    return connection


def main() -> int:
    parser = argparse.ArgumentParser(description="DroneProto balance telemetry importer")
    commands = parser.add_subparsers(dest="command", required=True)
    import_parser = commands.add_parser("import")
    import_parser.add_argument("--input", type=Path, required=True)
    import_parser.add_argument("--db", type=Path, required=True)
    import_parser.add_argument("--source-commit", default="Unknown")
    export_parser = commands.add_parser("export")
    export_parser.add_argument("--db", type=Path, required=True)
    export_parser.add_argument("--output", type=Path, required=True)
    export_parser.add_argument("--exclude-environment", action="append", default=[])
    export_parser.add_argument("--completed-only", action="store_true")
    prune_parser = commands.add_parser("prune")
    prune_parser.add_argument("--db", type=Path, required=True)
    prune_parser.add_argument("--older-than-days", type=int, default=90)
    args = parser.parse_args()

    connection = _open_database(args.db)
    try:
        if args.command == "import":
            with args.input.open("r", encoding="utf-8", errors="replace") as stream:
                stats = import_lines(connection, args.input.name, stream, args.source_commit)
            print(f"inserted={stats.inserted} rejected={stats.rejected} ignored={stats.ignored}")
            return 0 if stats.rejected == 0 else 2
        if args.command == "export":
            paths = export_views(connection, args.output)
            for path in paths:
                print(path)
            return 0
        cutoff = f"-{max(0, args.older_than_days)} days"
        with connection:
            cursor = connection.execute(
                """
                DELETE FROM telemetry_events WHERE session_id IN (
                    SELECT session_id FROM sessions
                    WHERE completion_state='Completed' AND datetime(started_utc) < datetime('now', ?)
                )
                """,
                (cutoff,),
            )
        print(f"deleted={cursor.rowcount}")
        return 0
    finally:
        connection.close()


if __name__ == "__main__":
    raise SystemExit(main())
