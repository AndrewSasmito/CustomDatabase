#include "health_monitor.h"
#include <iostream>
#include <algorithm>

HealthMonitor::HealthMonitor(JobScheduler* scheduler, std::chrono::milliseconds interval)
    : job_scheduler(scheduler), health_check_job_name("system_health_check"),
      check_interval(interval), recovery_attempts(0), successful_recoveries(0),
      overall_health(HealthStatus::HEALTHY), 
      last_health_change(std::chrono::steady_clock::now()),
      max_consecutive_failures(3), recovery_cooldown(std::chrono::minutes(5)) {
    
    std::cout << "HealthMonitor: Initialized with " << interval.count() 
              << "ms check interval" << std::endl;
}

HealthMonitor::~HealthMonitor() {
    stop();
}

void HealthMonitor::start() {
    if (!job_scheduler || !job_scheduler->isRunning()) {
        std::cerr << "HealthMonitor: Job scheduler not running" << std::endl;
        return;
    }
    
    // Add recurring health check job
    job_scheduler->addRecurringJob(
        health_check_job_name,
        check_interval,
        [this]() { return this->healthCheckJobFunc(); },
        "System Health Check",
        JobPriority::HIGH
    );
    
    std::cout << "HealthMonitor: Started health monitoring" << std::endl;
}

void HealthMonitor::stop() {
    if (job_scheduler) {
        job_scheduler->removeRecurringJob(health_check_job_name);
    }
    
    std::cout << "HealthMonitor: Stopped health monitoring" << std::endl;
}

void HealthMonitor::registerComponent(ComponentType type, const std::string& name) {
    std::lock_guard<std::mutex> lock(health_mutex);
    
    auto component = std::make_shared<ComponentHealth>(type, name);
    components[type] = component;
    
    std::cout << "HealthMonitor: Registered component " << name 
              << " (" << componentTypeToString(type) << ")" << std::endl;
}

void HealthMonitor::addMetric(ComponentType type, const std::string& metric_name, 
                             double warning_threshold, double critical_threshold) {
    std::lock_guard<std::mutex> lock(health_mutex);
    
    auto it = components.find(type);
    if (it == components.end()) {
        std::cerr << "HealthMonitor: Component " << componentTypeToString(type) 
                  << " not registered" << std::endl;
        return;
    }
    
    auto metric = std::make_shared<HealthMetric>(metric_name, warning_threshold, critical_threshold);
    it->second->metrics.push_back(metric);
    
    std::cout << "HealthMonitor: Added metric " << metric_name << " to " 
              << it->second->name << std::endl;
}

void HealthMonitor::registerRecoveryAction(ComponentType type, std::function<bool()> recovery_func) {
    recovery_actions[type] = recovery_func;
    
    std::cout << "HealthMonitor: Registered recovery action for " 
              << componentTypeToString(type) << std::endl;
}

void HealthMonitor::updateMetric(ComponentType type, const std::string& metric_name, double value) {
    std::lock_guard<std::mutex> lock(health_mutex);
    
    auto comp_it = components.find(type);
    if (comp_it == components.end()) {
        return;
    }
    
    auto& component = comp_it->second;
    for (auto& metric : component->metrics) {
        if (metric->name == metric_name) {
            metric->value = value;
            metric->last_updated = std::chrono::steady_clock::now();
            
            // Update metric status
            if (value >= metric->critical_threshold) {
                metric->status = HealthStatus::CRITICAL;
            } else if (value >= metric->warning_threshold) {
                metric->status = HealthStatus::WARNING;
            } else {
                metric->status = HealthStatus::HEALTHY;
            }
            
            break;
        }
    }
}

void HealthMonitor::reportError(ComponentType type, const std::string& error_message) {
    std::lock_guard<std::mutex> lock(health_mutex);
    
    auto it = components.find(type);
    if (it == components.end()) {
        return;
    }
    
    auto& component = it->second;
    component->last_error = error_message;
    component->consecutive_failures++;
    component->status = HealthStatus::CRITICAL;
    
    std::cout << "HealthMonitor: Error reported for " << component->name 
              << " (" << component->consecutive_failures << " consecutive): " 
              << error_message << std::endl;
    
    // Trigger alert if callback is set
    if (alert_callback) {
        alert_callback(type, HealthStatus::CRITICAL, error_message);
    }
    
    // Attempt recovery if needed
    if (shouldAttemptRecovery(type)) {
        attemptRecovery(type);
    }
}

void HealthMonitor::reportRecovery(ComponentType type) {
    std::lock_guard<std::mutex> lock(health_mutex);
    
    auto it = components.find(type);
    if (it == components.end()) {
        return;
    }
    
    auto& component = it->second;
    component->consecutive_failures = 0;
    component->status = HealthStatus::HEALTHY;
    component->last_error.clear();
    
    std::cout << "HealthMonitor: Recovery reported for " << component->name << std::endl;
}

bool HealthMonitor::performHealthCheck() {
    std::lock_guard<std::mutex> lock(health_mutex);
    
    for (auto& [type, component] : components) {
        checkComponent(component);
    }
    
    updateOverallHealth();
    return true;
}

void HealthMonitor::checkComponent(std::shared_ptr<ComponentHealth> component) {
    component->last_check = std::chrono::steady_clock::now();
    
    // Check all metrics
    HealthStatus worst_status = HealthStatus::HEALTHY;
    
    for (const auto& metric : component->metrics) {
        if (metric->status > worst_status) {
            worst_status = metric->status;
        }
        
        // Check if metric is stale
        auto age = std::chrono::steady_clock::now() - metric->last_updated;
        if (age > std::chrono::minutes(5)) {
            worst_status = HealthStatus::WARNING;
        }
    }
    
    // Update component status
    HealthStatus old_status = component->status;
    component->status = worst_status;
    
    // Report status changes
    if (old_status != worst_status) {
        std::cout << "HealthMonitor: " << component->name << " status changed from " 
                  << healthStatusToString(old_status) << " to " 
                  << healthStatusToString(worst_status) << std::endl;
        
        if (alert_callback) {
            alert_callback(component->type, worst_status, "Status change detected");
        }
    }
}

void HealthMonitor::updateOverallHealth() {
    HealthStatus worst_status = HealthStatus::HEALTHY;
    
    for (const auto& [type, component] : components) {
        if (component->status > worst_status) {
            worst_status = component->status;
        }
    }
    
    HealthStatus old_overall = overall_health.load();
    overall_health.store(worst_status);
    
    if (old_overall != worst_status) {
        last_health_change = std::chrono::steady_clock::now();
        std::cout << "HealthMonitor: Overall system health changed to " 
                  << healthStatusToString(worst_status) << std::endl;
    }
}

bool HealthMonitor::shouldAttemptRecovery(ComponentType type) const {
    auto it = components.find(type);
    if (it == components.end()) {
        return false;
    }
    
    // Check if we have a recovery action
    if (recovery_actions.find(type) == recovery_actions.end()) {
        return false;
    }
    
    // Check consecutive failures
    if (it->second->consecutive_failures < max_consecutive_failures) {
        return false;
    }
    
    // Check cooldown period
    auto last_attempt_it = last_recovery_attempt.find(type);
    if (last_attempt_it != last_recovery_attempt.end()) {
        auto time_since_last = std::chrono::steady_clock::now() - last_attempt_it->second;
        if (time_since_last < recovery_cooldown) {
            return false;
        }
    }
    
    return true;
}

void HealthMonitor::attemptRecovery(ComponentType type) {
    auto recovery_it = recovery_actions.find(type);
    if (recovery_it == recovery_actions.end()) {
        return;
    }
    
    std::cout << "HealthMonitor: Attempting recovery for " 
              << componentTypeToString(type) << std::endl;
    
    recovery_attempts.fetch_add(1);
    last_recovery_attempt[type] = std::chrono::steady_clock::now();
    
    try {
        bool success = recovery_it->second();
        
        if (success) {
            successful_recoveries.fetch_add(1);
            reportRecovery(type);
            std::cout << "HealthMonitor: Recovery successful for " 
                      << componentTypeToString(type) << std::endl;
        } else {
            std::cout << "HealthMonitor: Recovery failed for " 
                      << componentTypeToString(type) << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "HealthMonitor: Recovery threw exception for " 
                  << componentTypeToString(type) << ": " << e.what() << std::endl;
    }
}

HealthStatus HealthMonitor::getComponentHealth(ComponentType type) const {
    std::lock_guard<std::mutex> lock(health_mutex);
    
    auto it = components.find(type);
    if (it == components.end()) {
        return HealthStatus::FAILED;
    }
    
    return it->second->status;
}

HealthStatus HealthMonitor::getOverallHealth() const {
    return overall_health.load();
}

bool HealthMonitor::isSystemHealthy() const {
    return overall_health.load() == HealthStatus::HEALTHY;
}

std::vector<ComponentType> HealthMonitor::getUnhealthyComponents() const {
    std::lock_guard<std::mutex> lock(health_mutex);
    
    std::vector<ComponentType> unhealthy;
    for (const auto& [type, component] : components) {
        if (component->status != HealthStatus::HEALTHY) {
            unhealthy.push_back(type);
        }
    }
    
    return unhealthy;
}

HealthMonitor::HealthStats HealthMonitor::getStats() const {
    std::lock_guard<std::mutex> lock(health_mutex);
    
    size_t healthy = 0, warning = 0, critical = 0, failed = 0;
    
    for (const auto& [type, component] : components) {
        switch (component->status) {
            case HealthStatus::HEALTHY: healthy++; break;
            case HealthStatus::WARNING: warning++; break;
            case HealthStatus::CRITICAL: critical++; break;
            case HealthStatus::FAILED: failed++; break;
        }
    }
    
    size_t total_attempts = recovery_attempts.load();
    size_t successful = successful_recoveries.load();
    double success_rate = total_attempts > 0 ? (double)successful / total_attempts * 100.0 : 0.0;
    
    return {
        components.size(),
        healthy,
        warning,
        critical,
        failed,
        total_attempts,
        successful,
        success_rate,
        overall_health.load(),
        last_health_change
    };
}

void HealthMonitor::printHealthReport() const {
    auto stats = getStats();
    
    std::cout << "\n=== System Health Report ===" << std::endl;
    std::cout << "Overall Status: " << healthStatusToString(stats.overall_status) << std::endl;
    std::cout << "Total Components: " << stats.total_components << std::endl;
    std::cout << "  Healthy: " << stats.healthy_components << std::endl;
    std::cout << "  Warning: " << stats.warning_components << std::endl;
    std::cout << "  Critical: " << stats.critical_components << std::endl;
    std::cout << "  Failed: " << stats.failed_components << std::endl;
    std::cout << "Recovery Attempts: " << stats.recovery_attempts << std::endl;
    std::cout << "Successful Recoveries: " << stats.successful_recoveries << std::endl;
    std::cout << "Recovery Success Rate: " << stats.recovery_success_rate << "%" << std::endl;
    
    // Component details
    std::lock_guard<std::mutex> lock(health_mutex);
    for (const auto& [type, component] : components) {
        std::cout << "\n" << component->name << " (" << componentTypeToString(type) << "):" << std::endl;
        std::cout << "  Status: " << healthStatusToString(component->status) << std::endl;
        std::cout << "  Consecutive Failures: " << component->consecutive_failures << std::endl;
        
        if (!component->last_error.empty()) {
            std::cout << "  Last Error: " << component->last_error << std::endl;
        }
        
        for (const auto& metric : component->metrics) {
            std::cout << "  " << metric->name << ": " << metric->value 
                      << " (" << healthStatusToString(metric->status) << ")" << std::endl;
        }
    }
    
    std::cout << "============================" << std::endl;
}

void HealthMonitor::setAlertCallback(std::function<void(ComponentType, HealthStatus, const std::string&)> callback) {
    alert_callback = callback;
}

void HealthMonitor::setMaxConsecutiveFailures(size_t max_failures) {
    max_consecutive_failures = max_failures;
}

void HealthMonitor::setRecoveryCooldown(std::chrono::minutes cooldown) {
    recovery_cooldown = cooldown;
}

bool HealthMonitor::healthCheckJobFunc() {
    return performHealthCheck();
}

std::string HealthMonitor::componentTypeToString(ComponentType type) const {
    switch (type) {
        case ComponentType::WAL_MANAGER: return "WAL_MANAGER";
        case ComponentType::PAGE_CACHE: return "PAGE_CACHE";
        case ComponentType::WRITER_QUEUE: return "WRITER_QUEUE";
        case ComponentType::JOB_SCHEDULER: return "JOB_SCHEDULER";
        case ComponentType::VERSION_MANAGER: return "VERSION_MANAGER";
        case ComponentType::CHECKPOINT_MANAGER: return "CHECKPOINT_MANAGER";
        case ComponentType::BTREE_ENGINE: return "BTREE_ENGINE";
        default: return "UNKNOWN";
    }
}

std::string HealthMonitor::healthStatusToString(HealthStatus status) const {
    switch (status) {
        case HealthStatus::HEALTHY: return "HEALTHY";
        case HealthStatus::WARNING: return "WARNING";
        case HealthStatus::CRITICAL: return "CRITICAL";
        case HealthStatus::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}
