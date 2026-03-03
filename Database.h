#ifndef BASEX_DATABASE_H
#define BASEX_DATABASE_H

#include <string>
#include <vector>
#include <memory>
#include <sqlite3.h>

namespace BaseX {

// Database record structures
struct DemoRecord {
    int id;
    std::string name;
    std::string category;
    std::string description;
    int launchCount;
    double totalRuntime;
    double lastLaunched;
};

struct MetricsRecord {
    int id;
    double timestamp;
    uint64_t frameCount;
    float fps;
    uint64_t radixSwitches;
    std::string currentRadix;
    float averageError;
};

struct PrecisionTestRecord {
    int id;
    double timestamp;
    std::string demoName;
    float distance;
    std::string radixUsed;
    float measuredError;
    bool passed;
};

// Database manager
class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    // Connection management
    bool open(const std::string& path);
    void close();
    bool isOpen() const { return m_db != nullptr; }

    // Schema initialization
    bool initializeSchema();

    // Demo tracking
    bool recordDemoLaunch(const std::string& demoName);
    bool updateDemoRuntime(const std::string& demoName, double runtime);
    std::vector<DemoRecord> getAllDemos();
    DemoRecord getDemoByName(const std::string& name);

    // Performance metrics
    bool recordMetrics(const MetricsRecord& metrics);
    std::vector<MetricsRecord> getRecentMetrics(int count = 100);
    MetricsRecord getAverageMetrics(double timePeriod);

    // Precision testing
    bool recordPrecisionTest(const PrecisionTestRecord& test);
    std::vector<PrecisionTestRecord> getPrecisionTests(const std::string& demoName);
    double getPrecisionPassRate(const std::string& demoName);

    // Utility
    std::string getLastError() const { return m_lastError; }
    bool vacuum(); // Optimize database

private:
    sqlite3* m_db = nullptr;
    std::string m_lastError;

    // Helper methods
    bool execute(const std::string& sql);
    bool prepareStatement(const std::string& sql, sqlite3_stmt** stmt);
    void setError(const std::string& error);
};

} // namespace BaseX

#endif // BASEX_DATABASE_H
