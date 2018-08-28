/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/
#ifndef UPDATER_H_
#define UPDATER_H_

#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "resource.h"
#include "store.h"
#include "time.h"

namespace google {

// Configuration object.
class Configuration;

// An abstract class for asynchronous updates of the metadata mapping.
class MetadataUpdater {
 public:
  struct ResourceMetadata {
    ResourceMetadata(const std::vector<std::string>& ids,
                     const MonitoredResource& resource,
                     MetadataStore::Metadata&& metadata)
        : ids_(ids), resource_(resource), metadata_(std::move(metadata)) {}
    ResourceMetadata(ResourceMetadata&& other)
        : ResourceMetadata(other.ids_, other.resource_,
                           std::move(other.metadata_)) {}

    const MetadataStore::Metadata& metadata() const { return metadata_; }
    const MonitoredResource& resource() const { return resource_; }
    const std::vector<std::string>& ids() const { return ids_; }

   private:
    friend class MetadataUpdater;  // Needs write access to metadata_.

    std::vector<std::string> ids_;
    MonitoredResource resource_;
    MetadataStore::Metadata metadata_;
  };

  // A representation of all validation errors.
  class ConfigurationValidationError {
   public:
    ConfigurationValidationError(const std::string& what)
        : explanation_(what) {}
    const std::string& what() const { return explanation_; }
   private:
    std::string explanation_;
  };

  MetadataUpdater(const Configuration& config, MetadataStore* store,
                  const std::string& name);
  virtual ~MetadataUpdater();

  // Starts updating.
  void Start() throw(ConfigurationValidationError);

  // Notifies the updater to stop updating.
  void NotifyStop();

  using UpdateCallback =
      std::function<void(std::vector<MetadataUpdater::ResourceMetadata>&&)>;

 protected:
  friend class UpdaterTest;

  // Validates static properties of the relevant configuration.
  // If the configuration is invalid, throws a ConfigurationValidationError,
  // which is generally expected to pass through and terminate the program.
  virtual void ValidateStaticConfiguration() const
      throw(ConfigurationValidationError) {}

  // Decides whether the updater logic should be started based on the current
  // configuration.
  virtual bool ShouldStartUpdater() const {
    return true;
  }

  // Validates dynamic properties of the relevant configuration.
  // If the configuration is invalid, throws a ConfigurationValidationError,
  // which is generally expected to pass through and terminate the program.
  // This should only run when we know the updater is about to be started.
  virtual void ValidateDynamicConfiguration() const
      throw(ConfigurationValidationError) {}

  // Internal method for starting the updater's logic.
  virtual void StartUpdater() = 0;

  // Internal method for notifying the updater's to stop its logic.
  // This method should not perform any blocking operations (e.g., wait).
  virtual void NotifyStopUpdater() = 0;

  // Updates the resource map in the store.
  void UpdateResourceCallback(const ResourceMetadata& result) {
    store_->UpdateResource(result.ids_, result.resource_);
  }

  // Updates the metadata in the store. Consumes result.
  void UpdateMetadataCallback(ResourceMetadata&& result) {
    store_->UpdateMetadata(std::move(result.metadata_));
  }

  const std::string& name() const {
    return name_;
  }

  const Configuration& config() const {
    return config_;
  }

 private:
  // The name of the updater provided by subclasses.
  std::string name_;

  const Configuration& config_;

  // The store for the metadata.
  MetadataStore* store_;
};

// Abstract class for a timer.
class Timer {
 public:
  // Initializes the timer.
  virtual void Init() = 0;

  // Waits for one duration to pass.  Returns false if the timer was
  // canceled while waiting.
  virtual bool Wait(time::seconds duration) = 0;

  // Cancels the timer.
  virtual void Cancel() = 0;
};

// Implementation of a timer parameterized over a clock type.
template<typename Clock>
class TimerImpl : public Timer {
 public:
  TimerImpl(bool verbose, const std::string& name)
      : verbose_(verbose), name_(name) {}
  void Init() override {
    timer_.lock();
    if (verbose_) {
      LOG(INFO) << "Locked timer for " << name_;
    }
  }
  bool Wait(time::seconds duration) override {
    // An unlocked timer means the wait is cancelled.
    auto start = Clock::now();
    auto wakeup = start + duration;
    if (verbose_) {
      LOG(INFO) << "Trying to unlock the timer for " << name_;
    }
    while (!timer_.try_lock_until(wakeup)) {
      auto now = Clock::now();
      // Detect spurious wakeups.
      if (now < wakeup) {
        continue;
      }
      if (verbose_) {
        LOG(INFO) << " Timer unlock timed out after "
                  << std::chrono::duration_cast<time::seconds>(now-start).count()
                  << "s (good) for " << name_;
      }
      return true;
    }
    return false;
  }
  void Cancel() override {
    timer_.unlock();
    if (verbose_) {
      LOG(INFO) << "Unlocked timer for " << name_;
    }
  }
 private:
  std::timed_mutex timer_;
  bool verbose_;
  std::string name_;
};

// A class for all periodic updates of the metadata mapping.
class PollingMetadataUpdater : public MetadataUpdater {
 public:
  PollingMetadataUpdater(
      const Configuration& config, MetadataStore* store,
      const std::string& name, double period_s,
      std::function<std::vector<ResourceMetadata>()> query_metadata);
  ~PollingMetadataUpdater();

 protected:
  friend class UpdaterTest;

  PollingMetadataUpdater(
      const Configuration& config, MetadataStore* store,
      const std::string& name, double period_s,
      std::function<std::vector<ResourceMetadata>()> query_metadata,
      std::unique_ptr<Timer> timer);

  void ValidateStaticConfiguration() const throw(ConfigurationValidationError);
  using MetadataUpdater::ValidateDynamicConfiguration;
  bool ShouldStartUpdater() const;
  void StartUpdater();
  void NotifyStopUpdater();

 private:
  friend class InstanceTest;

  // Metadata poller.
  void PollForMetadata();

  // The polling period in seconds.
  time::seconds period_;

  // The function to actually query for metadata.
  std::function<std::vector<ResourceMetadata>()> query_metadata_;

  // The timer.
  std::unique_ptr<Timer> timer_;

  // The thread that polls for new metadata.
  std::thread reporter_thread_;
};

}

#endif  // UPDATER_H_
