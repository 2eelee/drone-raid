PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS telemetry_events (
    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_file TEXT NOT NULL,
    source_line INTEGER NOT NULL,
    source_commit TEXT NOT NULL,
    schema_version INTEGER NOT NULL,
    session_id TEXT NOT NULL,
    sequence INTEGER NOT NULL,
    event_time REAL NOT NULL,
    environment TEXT NOT NULL,
    event_type TEXT NOT NULL,
    player_id TEXT,
    payload_json TEXT NOT NULL,
    UNIQUE (session_id, sequence)
);

CREATE TABLE IF NOT EXISTS sessions (
    session_id TEXT PRIMARY KEY,
    environment TEXT NOT NULL,
    server_slot TEXT,
    map_name TEXT,
    build_version TEXT NOT NULL,
    balance_version TEXT NOT NULL,
    source_commit TEXT NOT NULL,
    started_utc TEXT,
    duration REAL,
    outcome TEXT,
    completion_state TEXT NOT NULL DEFAULT 'Aborted',
    player_count INTEGER,
    boss_hp_remaining REAL
);

CREATE TABLE IF NOT EXISTS players (
    session_id TEXT NOT NULL,
    player_id TEXT NOT NULL,
    core_part TEXT,
    left_part TEXT,
    right_part TEXT,
    survival_time REAL,
    boss_damage REAL,
    move_distance REAL,
    heal_amount REAL,
    damage_taken_count INTEGER,
    bonus_score INTEGER,
    grade TEXT,
    report_score REAL,
    PRIMARY KEY (session_id, player_id),
    FOREIGN KEY (session_id) REFERENCES sessions(session_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_events_session_type
ON telemetry_events(session_id, event_type);

CREATE INDEX IF NOT EXISTS idx_sessions_filters
ON sessions(environment, balance_version, build_version, server_slot, completion_state);

DROP VIEW IF EXISTS loadout_balance;
CREATE VIEW loadout_balance AS
SELECT
    s.balance_version,
    s.environment,
    COALESCE(p.core_part, 'None') AS core_part,
    COALESCE(p.left_part, 'None') AS left_part,
    COALESCE(p.right_part, 'None') AS right_part,
    COUNT(*) AS player_samples,
    ROUND(AVG(p.boss_damage), 3) AS avg_boss_damage,
    ROUND(AVG(p.survival_time), 3) AS avg_survival_time,
    ROUND(AVG(p.report_score), 3) AS avg_report_score
FROM players p
JOIN sessions s ON s.session_id = p.session_id
WHERE s.completion_state = 'Completed' AND s.environment <> 'Automation'
GROUP BY s.balance_version, s.environment, p.core_part, p.left_part, p.right_part;

DROP VIEW IF EXISTS pattern_balance;
CREATE VIEW pattern_balance AS
SELECT
    s.balance_version,
    s.environment,
    json_extract(e.payload_json, '$.Pattern') AS pattern,
    COUNT(*) AS contacts,
    SUM(CASE WHEN json_extract(e.payload_json, '$.Result') = 'Hit' THEN 1 ELSE 0 END) AS hits,
    SUM(CASE WHEN json_extract(e.payload_json, '$.Result') = 'Avoided' THEN 1 ELSE 0 END) AS avoided,
    SUM(CASE WHEN json_extract(e.payload_json, '$.Result') = 'Suppressed' THEN 1 ELSE 0 END) AS suppressed,
    ROUND(AVG(CAST(json_extract(e.payload_json, '$.AppliedDamage') AS REAL)), 3) AS avg_applied_damage,
    SUM(CASE WHEN json_extract(e.payload_json, '$.Killed') = '1' THEN 1 ELSE 0 END) AS kills
FROM telemetry_events e
JOIN sessions s ON s.session_id = e.session_id
WHERE e.event_type = 'PatternContactResolved'
  AND s.completion_state = 'Completed'
  AND s.environment <> 'Automation'
GROUP BY s.balance_version, s.environment, json_extract(e.payload_json, '$.Pattern');

DROP VIEW IF EXISTS raid_balance;
CREATE VIEW raid_balance AS
SELECT
    balance_version,
    environment,
    server_slot,
    COUNT(*) AS completed_sessions,
    ROUND(AVG(CASE WHEN outcome = 'BossDefeated' THEN 1.0 ELSE 0.0 END), 4) AS clear_rate,
    ROUND(AVG(duration), 3) AS avg_duration,
    ROUND(AVG(boss_hp_remaining), 3) AS avg_boss_hp_remaining
FROM sessions
WHERE completion_state = 'Completed' AND environment <> 'Automation'
GROUP BY balance_version, environment, server_slot;
