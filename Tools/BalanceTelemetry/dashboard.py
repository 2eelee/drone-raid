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
    completed_only: bool = True


@dataclass(frozen=True)
class DashboardSummary:
    completed_sessions: int
    clear_rate: float
    average_duration: float
    average_boss_hp_remaining: float
    sample_warning: bool


def _where(filters: DashboardFilters, prefix: str = "") -> tuple[str, list[str]]:
    clauses = [f"{prefix}completion_state = 'Completed'"] if filters.completed_only else ["1=1"]
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
    connection.row_factory = sqlite3.Row
    return [dict(row) for row in connection.execute(
        f"SELECT environment, completion_state, COUNT(*) AS sessions FROM sessions WHERE {where} GROUP BY environment, completion_state",
        parameters,
    )]


def load_attack_rows(connection: sqlite3.Connection, filters: DashboardFilters) -> list[dict[str, object]]:
    return _filtered_rows(
        connection,
        """
        SELECT COALESCE(json_extract(e.payload_json, '$.Core'), 'None') AS core_part,
               COALESCE(json_extract(e.payload_json, '$.Left'), 'None') AS left_part,
               COALESCE(json_extract(e.payload_json, '$.Right'), 'None') AS right_part,
               SUM(CASE WHEN e.event_type='AttackAttempted' THEN 1 ELSE 0 END) AS attempts,
               SUM(CASE WHEN e.event_type='AttackResolved' AND json_extract(e.payload_json, '$.Result')='Hit' THEN 1 ELSE 0 END) AS hits,
               SUM(CASE WHEN e.event_type='AttackResolved' AND json_extract(e.payload_json, '$.Result')='NoDamage' THEN 1 ELSE 0 END) AS no_damage,
               SUM(CASE WHEN e.event_type='AttackResolved' AND json_extract(e.payload_json, '$.Result')='Rejected' THEN 1 ELSE 0 END) AS rejected,
               ROUND(AVG(CASE WHEN e.event_type='AttackResolved' THEN CAST(json_extract(e.payload_json, '$.AppliedDamage') AS REAL) END), 3) AS avg_applied_damage
        FROM telemetry_events e JOIN sessions s ON s.session_id=e.session_id
        WHERE e.event_type IN ('AttackAttempted', 'AttackResolved') AND {where}
        GROUP BY core_part, left_part, right_part
        ORDER BY attempts DESC, avg_applied_damage DESC
        """,
        filters,
    )


def load_attack_result_rows(connection: sqlite3.Connection, filters: DashboardFilters) -> list[dict[str, object]]:
    return _filtered_rows(
        connection,
        """
        SELECT COALESCE(json_extract(e.payload_json, '$.Result'), 'Unknown') AS result,
               COUNT(*) AS events
        FROM telemetry_events e JOIN sessions s ON s.session_id=e.session_id
        WHERE e.event_type='AttackResolved' AND {where}
        GROUP BY result ORDER BY events DESC, result
        """,
        filters,
    )


def load_damage_rows(connection: sqlite3.Connection, filters: DashboardFilters) -> list[dict[str, object]]:
    return _filtered_rows(
        connection,
        """
        SELECT COALESCE(json_extract(e.payload_json, '$.Cause'), 'Unknown') AS cause,
               COALESCE(json_extract(e.payload_json, '$.Result'), 'Unknown') AS result,
               COUNT(*) AS events,
               ROUND(AVG(CAST(json_extract(e.payload_json, '$.AppliedDamage') AS REAL)), 3) AS avg_applied_damage,
               ROUND(SUM(CAST(json_extract(e.payload_json, '$.AppliedDamage') AS REAL)), 3) AS total_applied_damage
        FROM telemetry_events e JOIN sessions s ON s.session_id=e.session_id
        WHERE e.event_type='PlayerDamageResolved' AND {where}
        GROUP BY cause, result ORDER BY events DESC, cause, result
        """,
        filters,
    )


def load_survival_rows(connection: sqlite3.Connection, filters: DashboardFilters) -> list[dict[str, object]]:
    return _filtered_rows(
        connection,
        """
        SELECT COALESCE(p.core_part, 'None') AS core_part,
               COALESCE(p.left_part, 'None') AS left_part,
               COALESCE(p.right_part, 'None') AS right_part,
               COUNT(*) AS player_samples,
               ROUND(AVG(p.survival_time), 3) AS avg_survival_time,
               ROUND(AVG(p.damage_taken_count), 3) AS avg_damage_taken_count,
               ROUND(AVG(p.boss_damage), 3) AS avg_boss_damage
        FROM players p JOIN sessions s ON s.session_id=p.session_id
        WHERE p.survival_time IS NOT NULL AND {where}
        GROUP BY p.core_part, p.left_part, p.right_part
        ORDER BY player_samples DESC, avg_survival_time DESC
        """,
        filters,
    )


def load_dodge_rows(connection: sqlite3.Connection, filters: DashboardFilters) -> list[dict[str, object]]:
    return _filtered_rows(
        connection,
        """
        SELECT COALESCE(json_extract(e.payload_json, '$.Result'), 'Unknown') AS result,
               COALESCE(json_extract(e.payload_json, '$.Reason'), 'Unknown') AS reason,
               COUNT(*) AS events,
               ROUND(AVG(CAST(json_extract(e.payload_json, '$.DistanceMeters') AS REAL)), 3) AS avg_distance_meters
        FROM telemetry_events e JOIN sessions s ON s.session_id=e.session_id
        WHERE e.event_type='DodgeResolved' AND {where}
        GROUP BY result, reason ORDER BY events DESC, result, reason
        """,
        filters,
    )


def load_raw_event_rows(
    connection: sqlite3.Connection,
    filters: DashboardFilters,
    limit: int = 500,
) -> list[dict[str, object]]:
    where, parameters = _where(filters, "s.")
    connection.row_factory = sqlite3.Row
    return [dict(row) for row in connection.execute(
        f"""
        SELECT e.session_id, e.sequence, e.event_time, e.environment, e.event_type,
               e.player_id, e.payload_json
        FROM telemetry_events e JOIN sessions s ON s.session_id=e.session_id
        WHERE {where}
        ORDER BY COALESCE(s.started_utc, '') DESC, e.sequence DESC
        LIMIT ?
        """,
        [*parameters, max(1, min(limit, 500))],
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
        completed_only = st.sidebar.toggle("완료 세션만", value=True)
        filters = DashboardFilters(
            environments=tuple(environments),
            balance_versions=tuple(balance_versions),
            build_versions=tuple(build_versions),
            server_slots=tuple(server_slots),
            exclude_automation="Automation" not in environments,
            completed_only=completed_only,
        )
        summary = load_summary(connection, filters)
        columns = st.columns(4)
        columns[0].metric("완료 세션" if completed_only else "선택 세션", summary.completed_sessions)
        columns[1].metric("클리어율", f"{summary.clear_rate:.1%}")
        columns[2].metric("평균 플레이 시간", f"{summary.average_duration:.1f}초")
        columns[3].metric("평균 보스 잔여 HP", f"{summary.average_boss_hp_remaining:.1f}")
        if summary.completed_sessions == 0:
            st.info("Automation을 선택하거나 로컬 PIE/서버 플레이를 완료하세요.")
        if summary.sample_warning:
            st.warning("완료 세션이 20개 미만입니다. 밸런스 결론보다 동작 확인용으로만 보세요.")

        overview, loadouts, attacks, damage, dodges, patterns, quality, raw_events = st.tabs((
            "종합", "부품", "공격 분석", "피해·생존", "회피", "보스 패턴", "세션 품질", "원본 이벤트",
        ))
        with overview:
            rows = load_raid_rows(connection, filters)
            if rows:
                st.dataframe(rows, width="stretch")
                st.subheader("밸런스 버전별 클리어율")
                st.bar_chart(rows, x="balance_version", y="clear_rate")
            else:
                st.info("선택한 필터의 레이드 세션이 없습니다.")
        with loadouts:
            rows = load_loadout_rows(connection, filters)
            if rows:
                st.dataframe(rows, width="stretch")
                chart_rows = [
                    {"loadout": f"{row['core_part']} / {row['left_part']} / {row['right_part']}", "avg_boss_damage": row["avg_boss_damage"]}
                    for row in rows
                ]
                st.subheader("로드아웃별 평균 보스 피해")
                st.bar_chart(chart_rows, x="loadout", y="avg_boss_damage")
            else:
                st.info("부품을 확정하고 레이드 종료 후 DroneReport를 생성하세요.")
        with attacks:
            rows = load_attack_rows(connection, filters)
            result_rows = load_attack_result_rows(connection, filters)
            if rows:
                st.dataframe(rows, width="stretch")
                chart_rows = [
                    {"loadout": f"{row['core_part']} / {row['left_part']} / {row['right_part']}", "avg_applied_damage": row["avg_applied_damage"] or 0}
                    for row in rows
                ]
                st.subheader("로드아웃별 평균 적용 피해")
                st.bar_chart(chart_rows, x="loadout", y="avg_applied_damage")
                st.subheader("공격 결과 분포")
                st.bar_chart(result_rows, x="result", y="events")
            else:
                st.info("선택한 세션에서 보스를 공격하면 시도·성공·거부 결과가 표시됩니다.")
        with damage:
            damage_rows = load_damage_rows(connection, filters)
            survival_rows = load_survival_rows(connection, filters)
            if damage_rows:
                st.subheader("피해 원인별 결과")
                st.dataframe(damage_rows, width="stretch")
                damage_chart_rows = [
                    {"cause_result": f"{row['cause']} / {row['result']}", "total_applied_damage": row["total_applied_damage"] or 0}
                    for row in damage_rows
                ]
                st.bar_chart(damage_chart_rows, x="cause_result", y="total_applied_damage")
            else:
                st.info("보스 패턴에 피격되거나 회피해 PlayerDamageResolved 이벤트를 생성하세요.")
            if survival_rows:
                st.subheader("로드아웃별 생존 결과")
                st.dataframe(survival_rows, width="stretch")
                survival_chart_rows = [
                    {"loadout": f"{row['core_part']} / {row['left_part']} / {row['right_part']}", "avg_survival_time": row["avg_survival_time"] or 0}
                    for row in survival_rows
                ]
                st.bar_chart(survival_chart_rows, x="loadout", y="avg_survival_time")
            else:
                st.info("레이드를 종료해 DroneReport 생존 요약을 생성하세요.")
        with dodges:
            rows = load_dodge_rows(connection, filters)
            if rows:
                st.dataframe(rows, width="stretch")
                chart_rows = [
                    {"result_reason": f"{row['result']} / {row['reason']}", "events": row["events"]}
                    for row in rows
                ]
                st.subheader("회피 결과·사유 분포")
                st.bar_chart(chart_rows, x="result_reason", y="events")
            else:
                st.info("선택한 세션에서 회피를 시도하면 성공·거부·Clamp 사유가 표시됩니다.")
        with patterns:
            rows = load_pattern_rows(connection, filters)
            if rows:
                st.dataframe(rows, width="stretch")
                st.subheader("패턴별 피격·회피")
                st.bar_chart(rows, x="pattern", y=["hits", "avoided", "kills"])
            else:
                st.info("활성 보스 패턴에 접촉하거나 회피해 패턴 결과를 생성하세요.")
        with quality:
            rows = load_quality_rows(connection, filters)
            if rows:
                st.dataframe(rows, width="stretch")
                st.bar_chart(rows, x="environment", y="sessions")
            else:
                st.info("선택한 필터의 세션이 없습니다.")
        with raw_events:
            rows = load_raw_event_rows(connection, filters)
            if rows:
                st.caption("최신 500건까지 표시합니다.")
                st.dataframe(rows, width="stretch")
            else:
                st.info("선택한 필터의 이벤트가 없습니다.")
    finally:
        connection.close()


def main() -> None:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--db", type=Path, required=True)
    args, _ = parser.parse_known_args()
    run_dashboard(args.db)


if __name__ == "__main__":
    main()
