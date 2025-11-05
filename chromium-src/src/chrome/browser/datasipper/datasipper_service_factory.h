// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATASIPPER_DATASIPPER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DATASIPPER_DATASIPPER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace datasipper {
class DataSipperService;
}

// Factory for creating DataSipperService instances per profile
class DataSipperServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static DataSipperServiceFactory* GetInstance();
  static datasipper::DataSipperService* GetForProfile(Profile* profile);

  DataSipperServiceFactory(const DataSipperServiceFactory&) = delete;
  DataSipperServiceFactory& operator=(const DataSipperServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<DataSipperServiceFactory>;

  DataSipperServiceFactory();
  ~DataSipperServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_DATASIPPER_DATASIPPER_SERVICE_FACTORY_H_
