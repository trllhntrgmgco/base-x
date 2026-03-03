#include "BaseX/Database.h"
#include <cstring>
#include <chrono>

namespace BaseX {

DatabaseManager::DatabaseManager() {}

DatabaseManager::~DatabaseManager() {
    close();
}

bool DatabaseManager::open(const std::string& path) {
    if (m_db) {
        close();
    }

    int result = sqlite3_open(path.c_str(), &m_db);
    if (result != SQLITE_OK) {
        setError("Failed to open database: " + std::string(sqlite3_errmsg(m_db)));
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    return initializeSchema();
}

void DatabaseManager::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool DatabaseManager::initializeSchema() {
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS demos (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,
            category TEXT NOT NULL,
            description TEXT,
            launch_count INTEGER DEFAULT 0,
            total_runtime REAL DEFAULT 0.0,
            last_launched REAL DEFAULT 0.0,
            created_at REAL DEFAULT (julianday('now'))
        );

        CREATE TABLE IF NOT EXISTS metrics (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp REAL NOT NULL,
            frame_count INTEGER NOT NULL,
            fps REAL NOT NULL,
            radix_switches INTEGER DEFAULT 0,
            current_radix TEXT,
            average_error REAL DEFAULT 0.0
        );

        CREATE TABLE IF NOT EXISTS precision_tests (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp REAL NOT NULL,
            demo_name TEXT NOT NULL,
            distance REAL NOT NULL,
            radix_used TEXT NOT NULL,
            measured_error REAL NOT NULL,
            passed INTEGER NOT NULL,
            FOREIGN KEY (demo_name) REFERENCES demos(name)
        );

        CREATE INDEX IF NOT EXISTS idx_metrics_timestamp ON metrics(timestamp);
        CREATE INDEX IF NOT EXISTS idx_precision_demo ON precision_tests(demo_name);
        CREATE INDEX IF NOT EXISTS idx_demos_category ON demos(category);
    )";

    return execute(schema);
}

bool DatabaseManager::recordDemoLaunch(const std::string& demoName) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();

    const char* sql = R"(
        INSERT INTO demos (name, category, description, launch_count, last_launched)
        VALUES (?, 'General', 'Demo description', 1, ?)
        ON CONFLICT(name) DO UPDATE SET
            launch_count = launch_count + 1,
            last_launched = ?
    )";

    sqlite3_stmt* stmt;
    if (!prepareStatement(sql, &stmt)) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, demoName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, timestamp);
    sqlite3_bind_double(stmt, 3, timestamp);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        setError("Failed to record demo launch: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    return true;
}

bool DatabaseManager::updateDemoRuntime(const std::string& demoName, double runtime) {
    const char* sql = "UPDATE demos SET total_runtime = total_runtime + ? WHERE name = ?";
    
    sqlite3_stmt* stmt;
    if (!prepareStatement(sql, &stmt)) {
        return false;
    }

    sqlite3_bind_double(stmt, 1, runtime);
    sqlite3_bind_text(stmt, 2, demoName.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

std::vector<DemoRecord> DatabaseManager::getAllDemos() {
    std::vector<DemoRecord> demos;
    const char* sql = "SELECT id, name, category, description, launch_count, total_runtime, last_launched FROM demos ORDER BY launch_count DESC";

    sqlite3_stmt* stmt;
    if (!prepareStatement(sql, &stmt)) {
        return demos;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DemoRecord record;
        record.id = sqlite3_column_int(stmt, 0);
        record.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        record.category = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        record.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        record.launchCount = sqlite3_column_int(stmt, 4);
        record.totalRuntime = sqlite3_column_double(stmt, 5);
        record.lastLaunched = sqlite3_column_double(stmt, 6);
        demos.push_back(record);
    }

    sqlite3_finalize(stmt);
    return demos;
}

DemoRecord DatabaseManager::getDemoByName(const std::string& name) {
    DemoRecord record = {};
    const char* sql = "SELECT id, name, category, description, launch_count, total_runtime, last_launched FROM demos WHERE name = ?";

    sqlite3_stmt* stmt;
    if (!prepareStatement(sql, &stmt)) {
        return record;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        record.id = sqlite3_column_int(stmt, 0);
        record.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        record.category = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        record.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        record.launchCount = sqlite3_column_int(stmt, 4);
        record.totalRuntime = sqlite3_column_double(stmt, 5);
        record.lastLaunched = sqlite3_column_double(stmt, 6);
    }

    sqlite3_finalize(stmt);
    return record;
}

bool DatabaseManager::recordMetrics(const MetricsRecord& metrics) {
    const char* sql = R"(
        INSERT INTO metrics (timestamp, frame_count, fps, radix_switches, current_radix, average_error)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (!prepareStatement(sql, &stmt)) {
        return false;
    }

    sqlite3_bind_double(stmt, 1, metrics.timestamp);
    sqlite3_bind_int64(stmt, 2, metrics.frameCount);
    sqlite3_bind_double(stmt, 3, metrics.fps);
    sqlite3_bind_int64(stmt, 4, metrics.radixSwitches);
    sqlite3_bind_text(stmt, 5, metrics.currentRadix.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, metrics.averageError);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

std::vector<MetricsRecord> DatabaseManager::getRecentMetrics(int count) {
    std::vector<MetricsRecord> metrics;
    const char* sql = "SELECT * FROM metrics ORDER BY timestamp DESC LIMIT ?";

    sqlite3_stmt* stmt;
    if (!prepareStatement(sql, &stmt)) {
        return metrics;
    }

    sqlite3_bind_int(stmt, 1, count);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MetricsRecord record;
        record.id = sqlite3_column_int(stmt, 0);
        record.timestamp = sqlite3_column_double(stmt, 1);
        record.frameCount = sqlite3_column_int64(stmt, 2);
        record.fps = sqlite3_column_double(stmt, 3);
        record.radixSwitches = sqlite3_column_int64(stmt, 4);
        record.currentRadix = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        record.averageError = sqlite3_column_double(stmt, 6);
        metrics.push_back(record);
    }

    sqlite3_finalize(stmt);
    return metrics;
}

bool DatabaseManager::recordPrecisionTest(const PrecisionTestRecord& test) {
    const char* sql = R"(
        INSERT INTO precision_tests (timestamp, demo_name, distance, radix_used, measured_error, passed)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (!prepareStatement(sql, &stmt)) {
        return false;
    }

    sqlite3_bind_double(stmt, 1, test.timestamp);
    sqlite3_bind_text(stmt, 2, test.demoName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, test.distance);
    sqlite3_bind_text(stmt, 4, test.radixUsed.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, test.measuredError);
    sqlite3_bind_int(stmt, 6, test.passed ? 1 : 0);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

double DatabaseManager::getPrecisionPassRate(const std::string& demoName) {
    const char* sql = R"(
        SELECT 
            COUNT(CASE WHEN passed = 1 THEN 1 END) * 100.0 / COUNT(*) as pass_rate
        FROM precision_tests
        WHERE demo_name = ?
    )";

    sqlite3_stmt* stmt;
    if (!prepareStatement(sql, &stmt)) {
        return 0.0;
    }

    sqlite3_bind_text(stmt, 1, demoName.c_str(), -1, SQLITE_TRANSIENT);

    double passRate = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        passRate = sqlite3_column_double(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return passRate;
}

bool DatabaseManager::vacuum() {
    return execute("VACUUM");
}

// Private helper methods

bool DatabaseManager::execute(const std::string& sql) {
    char* errMsg = nullptr;
    int result = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);
    
    if (result != SQLITE_OK) {
        setError(errMsg ? errMsg : "Unknown error");
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool DatabaseManager::prepareStatement(const std::string& sql, sqlite3_stmt** stmt) {
    int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, stmt, nullptr);
    if (result != SQLITE_OK) {
        setError("Failed to prepare statement: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }
    return true;
}

void DatabaseManager::setError(const std::string& error) {
    m_lastError = error;
}

} // namespace BaseX
