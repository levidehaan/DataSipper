// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/datasipper/datasipper_service_factory.h"

#include "base/logging.h"
#include "chrome/browser/datasipper/datasipper_network_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/datasipper/datasipper_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
DataSipperServiceFactory* DataSipperServiceFactory::GetInstance() {
  static base::NoDestructor<DataSipperServiceFactory> instance;
  return instance.get();
}

// static
datasipper::DataSipperService* DataSipperServiceFactory::GetForProfile(
    Profile* profile) {
  if (!base::FeatureList::IsEnabled(features::kDataSipperEnabled)) {
    return nullptr;
  }

  return static_cast<datasipper::DataSipperService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DataSipperServiceFactory::DataSipperServiceFactory()
    : ProfileKeyedServiceFactory(
          "DataSipperService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
}

DataSipperServiceFactory::~DataSipperServiceFactory() = default;

std::unique_ptr<KeyedService>
DataSipperServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  base::FilePath database_path = profile->GetPath().Append(FILE_PATH_LITERAL("DataSipper"));

  // Create the service
  auto service = std::make_unique<datasipper::DataSipperService>(database_path);

  // Initialize the service
  if (!service->Initialize()) {
    LOG(ERROR) << "❌ DataSipperService: Failed to initialize for profile: "
               << profile->GetPath();
    return nullptr;
  }

  // Connect the service to the network bridge
  DataSipperNetworkBridge::GetInstance()->SetService(service.get());

  LOG(INFO) << "✅ DataSipperService: Created and initialized for profile: "
            << profile->GetPath();

  return service;
}

bool DataSipperServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool DataSipperServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
