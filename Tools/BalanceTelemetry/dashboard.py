from __future__ import annotations

import argparse
import sqlite3
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class DashboardFilters:
    environments: tuple[str, ...] = ()
    balance_versions: tuple[str, ...] = ()
    build_versions: tuple[str, ...] = ()
    server_slots: tuple[str, ...] = ()
    exclude_automation: bool = True


@dataclass(frozen=True)
class DashboardSummary:
    completed_sessions: int
    clear_rate: float
    average_duration: float
    average_boss_hp_remaining: float
    sample_warning: bool


def _where(filters: DashboardFilters, prefix: str = "") -> tuple[str, list[str]]:
    clauses = [f"{prefix}completion_state = 'Completed'"]
    if filters.exclude_automation and "Automation" not in filters.environments:
        clauses.append(f"{prefix}environment <> 'Automation'")
    parameters: list[str] = []
    for column, values in (
        ("environment", filters.environments),
        ("balance_version", filters.balance_versions),
        ("build_version", filters.build_versions),
        ("server_slot", filters.server_slots),
    ):
        if values:
            clauses.append(f"{prefix}{column} IN ({','.join('?' for _ in values)})")
            parameters.extend(values)
    return " AND ".join(clauses), parameters


def load_summary(
    connection: sqlite3.Connection,
    filters: DashboardFilters,
    minimum_completed_sessions: int = 20,
) -> DashboardSummary:
    where, parameters = _where(filters)
    row = connection.execute(
        f"""
        SELECT COUNT(*),
               COALESCE(AVG(CASE WHEN outcome='BossDefeated' THEN 1.0 ELSE 0.0 END), 0),
               COALESCE(AVG(duration), 0),
               COALESCE(AVG(boss_hp_remaining), 0)
        FROM sessions WHERE {where}
        """,
        parameters,
    ).fetchone()
    completed = int(row[0])
    return DashboardSummary(
        completed_sessions=completed,
        clear_rate=float(row[1]),
        average_duration=float(row[2]),
        average_boss_hp_remaining=float(row[3]),
        sample_warning=completed < minimum_completed_sessions,
    )


def _options(connection: sqlite3.Connection, column: str) -> list[str]:
    return [row[0] for row in connection.execute(
        f"SELECT DISTINCT {column} FROM sessions WHERE {column} IS NOT NULL ORDER BY {column}"
    )]


def _rows(connection: sqlite3.Connection, query: str) -> list[dict[str, object]]:
    connection.row_factory = sqlite3.Row
    return [dict(row) for row in connection.execute(query)]


def _filtered_rows(
    connection: sqlite3.Connection,
    query: str,
    filters: DashboardFilters,
    prefix: str = "s.",
) -> list[dict[str, object]]:
    where, parameters = _where(filters, prefix)
    connection.row_factory = sqlite3.Row
    return [dict(row) for row in connection.execute(query.format(where=where), parameters)]


def load_raid_rows(connection: sqlite3.Connection, filters: DashboardFilters) -> list[dict[str, object]]:
    return _filtered_rows(
        connection,
        """
        SELECT s.balance_version, s.environment, s.server_slot,
               COUNT(*) AS completed_sessions,
               ROUND(AVG(CASE WHEN s.outcome='BossDefeated' THEN 1.0 ELSE 0.0 END), 4) AS clear_rate,
               ROUND(AVG(s.duration), 3) AS avg_duration,
               ROUND(AVG(s.boss_hp_remaining), 3) AS avg_boss_hp_remaining
        FROM sessions s WHERE {where}
        GROUP BY s.balance_version, s.environment, s.server_slot
        ORDER BY s.balance_version, s.environment
        """,
        filters,
    )


def load_loadout_rows(connection: sqlite3.Connection, filters: DashboardFilters) -> list[dict[str, object]]:
    return _filtered_rows(
        connection,
        """
        SELECT s.balance_version, s.environment,
               COALESCE(p.core_part, 'None') AS core_part,
               COALESCE(p.left_part, 'None') AS left_part,
               COALESCE(p.right_part, 'None') AS right_part,
               COUNT(*) AS player_samples,
               ROUND(AVG(p.boss_damage), 3) AS avg_boss_damage,
               ROUND(AVG(p.survival_time), 3) AS avg_survival_time,
               ROUND(AVG(p.report_score), 3) AS avg_report_score
        FROM players p JOIN sessions s ON s.session_id=p.session_id
        WHERE {where}
        GROUP BY s.balance_version, s.environment, p.core_part, p.left_part, p.right_part
        ORDER BY player_samples DESC
        """,
        filters,
    )


def load_pattern_rows(connection: sqlite3.Connection, filters: DashboardFilters) -> list[dict[str, object]]:
    return _filtered_rows(
        connection,
        """
        SELECT s.balance_version, s.environment,
               json_extract(e.payload_json, '$.Pattern') AS pattern,
               COUNT(*) AS contacts,
               SUM(CASE WHEN json_extract(e.payload_json, '$.Result')='Hit' THEN 1 ELSE 0 END) AS hits,
               SUM(CASE WHEN json_extract(e.payload_json, '$.Result')='Avoided' THEN 1 ELSE 0 END) AS avoided,
               SUM(CASE WHEN json_extract(e.payload_json, '$.Killed')='1' THEN 1 ELSE 0 END) AS kills,
               ROUND(AVG(CAST(json_extract(e.payload_json, '$.AppliedDamage') AS REAL)), 3) AS avg_applied_damage
        FROM telemetry_events e JOIN sessions s ON s.session_id=e.session_id
        WHERE e.event_type='PatternContactResolved' AND {where}
        GROUP BY s.balance_version, s.environment, json_extract(e.payload_json, '$.Pattern')
        ORDER BY contacts DESC
        """,
        filters,
    )


def load_quality_rows(connection: sqlite3.Connection, filters: DashboardFilters) -> list[dict[str, object]]:
    where, parameters = _where(filters, "")
    filter_only = where.replace("completion_state = 'Completed'", "1=1")
    connection.row_factory = sqlite3.Row
    return [dict(row) for row in connection.execute(
        f"SELECT environment, completion_state, COUNT(*) AS sessions FROM sessions WHERE {filter_only} GROUP BY environment, completion_state",
        parameters,
    )]


def run_dashboard(database: Path) -> None:
    import streamlit as st

    st.set_page_config(page_title="DroneProto 밸런스", layout="wide")
    st.title("DroneProto 밸런스 텔레메트리")
    if not database.exists():
        st.warning("아직 수집된 DB가 없습니다. inbox에 Unreal 로그를 넣고 다시 실행하세요.")
        return

    connection = sqlite3.connect(database)
    try:
        environments = st.sidebar.multiselect(
            "환경", _options(connection, "environment"), default=[v for v in _options(connection, "environment") if v != "Automation"]
        )
        balance_versions = st.sidebar.multiselect("밸런스 버전", _options(connection, "balance_version"))
        build_versions = st.sidebar.multiselect("빌드 버전", _options(connection, "build_version"))
        server_slots = st.sidebar.multiselect("서버", _options(connection, "server_slot"))
        filters = DashboardFilters(
            environments=tuple(environments),
            balance_versions=tuple(balance_versions),
            build_versions=tuple(build_versions),
            server_slots=tuple(server_slots),
            exclude_automation="Automation" not in environments,
        )
        summary = load_summary(connection, filters)
        columns = st.columns(4)
        columns[0].metric("완료 세션", summary.completed_sessions)
        columns[1].metric("클리어율", f"{summary.clear_rate:.1%}")
        columns[2].metric("평균 플레이 시간", f"{summary.average_duration:.1f}초")
        columns[3].metric("평균 보스 잔여 HP", f"{summary.average_boss_hp_remaining:.1f}")
        if summary.sample_warning:
            st.warning("완료 세션이 20개 미만입니다. 밸런스 결론보다 동작 확인용으로만 보세요.")

        overview, loadouts, patterns, quality = st.tabs(("종합", "부품", "보스 패턴", "세션 품질"))
        with overview:
            st.dataframe(load_raid_rows(connection, filters), width="stretch")
        with loadouts:
            st.dataframe(load_loadout_rows(connection, filters), width="stretch")
        with patterns:
            st.dataframe(load_pattern_rows(connection, filters), width="stretch")
        with quality:
            st.dataframe(load_quality_rows(connection, filters), width="stretch")
    finally:
        connection.close()


def main() -> None:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--db", type=Path, required=True)
    args, _ = parser.parse_known_args()
    run_dashboard(args.db)


if __name__ == "__main__":
    main()
