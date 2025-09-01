#pragma once
#include <atomic>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <functional>
#include "job_scheduler.h"

enum class ComponentType {
    WAL_MANAGER,
    PAGE_CACHE,
    WRITER_QUEUE,
    JOB_SCHEDULER,
    VERSION_MANAGER,
    CHECKPOINT_MANAGER,
    BTREE_ENGINE
};

enum class HealthStatus {
    HEALTHY,
    WARNING,
    CRITICAL,
    FAILED
};

struct HealthMetric {
    std::string name;
    double value;
    double warning_threshold;
    double critical_threshold;
    std::chrono::steady_clock::time_point last_updated;
    HealthStatus status;
    
    HealthMetric(const std::string& n, double warn_thresh, double crit_thresh)
        : name(n), value(0.0), warning_threshold(warn_thresh), 
          critical_threshold(crit_thresh), last_updated(std::chrono::steady_clock::now()),
          status(HealthStatus::HEALTHY) {}
};

struct ComponentHealth {
    ComponentType type;
    std::string name;
    HealthStatus status;
    std::vector<std::shared_ptr<HealthMetric>> metrics;
    std::chrono::steady_clock::time_point last_check;
    std::string last_error;
    size_t consecutive_failures;
    
    ComponentHealth(ComponentType t, const std::string& n) 
        : type(t), name(n), status(HealthStatus::HEALTHY),
          last_check(std::chrono::steady_clock::now()), consecutive_failures(0) {}
};

class HealthMonitor {
private:
    // Component health tracking
    std::unordered_map<ComponentType, std::shared_ptr<ComponentHealth>> components;
    mutable std::mutex health_mutex;
    
    // Health check scheduling
    JobScheduler* job_scheduler;
    std::string health_check_job_name;
    std::chrono::milliseconds check_interval;
    
    // Recovery actions
    std::unordered_map<ComponentType, std::function<bool()>> recovery_actions;
    std::atomic<size_t> recovery_attempts;
    std::atomic<size_t> successful_recoveries;
    
    // System-wide health
    std::atomic<HealthStatus> overall_health;
    std::chrono::steady_clock::time_point last_health_change;
    
    // Alerting
    std::function<void(ComponentType, HealthStatus, const std::string&)> alert_callback;
    
    // Configuration
    size_t max_consecutive_failures;
    std::chrono::minutes recovery_cooldown;
    std::unordered_map<ComponentType, std::chrono::steady_clock::time_point> last_recovery_attempt;
    
    // Health check functions
    bool performHealthCheck();
    void checkComponent(std::shared_ptr<ComponentHealth> component);
    void updateOverallHealth();
    bool shouldAttemptRecovery(ComponentType type) const;
    void attemptRecovery(ComponentType type);
    
public:
    HealthMonitor(JobScheduler* scheduler, std::chrono::milliseconds interval = std::chrono::seconds(30));
    ~HealthMonitor();
    
    // Lifecycle
    void start();
    void stop();
    
    // Component registration
    void registerComponent(ComponentType type, const std::string& name);
    void addMetric(ComponentType type, const std::string& metric_name, 
                   double warning_threshold, double critical_threshold);
    void registerRecoveryAction(ComponentType type, std::function<bool()> recovery_func);
    
    // Metric updates
    void updateMetric(ComponentType type, const std::string& metric_name, double value);
    void reportError(ComponentType type, const std::string& error_message);
    void reportRecovery(ComponentType type);
    
    // Health status queries
    HealthStatus getComponentHealth(ComponentType type) const;
    HealthStatus getOverallHealth() const;
    bool isSystemHealthy() const;
    std::vector<ComponentType> getUnhealthyComponents() const;
    
    // Statistics
    struct HealthStats {
        size_t total_components;
        size_t healthy_components;
        size_t warning_components;
        size_t critical_components;
        size_t failed_components;
        size_t recovery_attempts;
        size_t successful_recoveries;
        double recovery_success_rate;
        HealthStatus overall_status;
        std::chrono::steady_clock::time_point last_health_change;
    };
    
    HealthStats getStats() const;
    void printHealthReport() const;
    
    // Configuration
    void setAlertCallback(std::function<void(ComponentType, HealthStatus, const std::string&)> callback);
    void setMaxConsecutiveFailures(size_t max_failures);
    void setRecoveryCooldown(std::chrono::minutes cooldown);
    
private:
    // Job scheduler function
    bool healthCheckJobFunc();
    
    // Utility functions
    std::string componentTypeToString(ComponentType type) const;
    std::string healthStatusToString(HealthStatus status) const;
};
