// Automatic generation of D-Bus interface mock proxies for:
//  - org.chromium.flimflam.Device
//  - org.chromium.flimflam.IPConfig
//  - org.chromium.flimflam.Manager
//  - org.chromium.flimflam.Profile
//  - org.chromium.flimflam.Service
//  - org.chromium.flimflam.Task
//  - org.chromium.flimflam.ThirdPartyVpn
#ifndef ____CHROMEOS_DBUS_BINDING___BUILD_LINK_VAR_CACHE_PORTAGE_CHROMEOS_BASE_SHILL_OUT_DEFAULT_GEN_INCLUDE_SHILL_DBUS_PROXY_MOCKS_H
#define ____CHROMEOS_DBUS_BINDING___BUILD_LINK_VAR_CACHE_PORTAGE_CHROMEOS_BASE_SHILL_OUT_DEFAULT_GEN_INCLUDE_SHILL_DBUS_PROXY_MOCKS_H
#include <string>
#include <vector>

#include <base/callback_forward.h>
#include <base/logging.h>
#include <base/macros.h>
#include <chromeos/any.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <gmock/gmock.h>

#include "shill/dbus-proxies.h"

namespace org {
namespace chromium {
namespace flimflam {

// Mock object for DeviceProxyInterface.
class DeviceProxyMock : public DeviceProxyInterface {
 public:
  DeviceProxyMock() = default;

  MOCK_METHOD3(AddWakeOnPacketConnection,
               bool(const std::string& /*in_ip_endpoint*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(AddWakeOnPacketConnectionAsync,
               void(const std::string& /*in_ip_endpoint*/,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(RemoveWakeOnPacketConnection,
               bool(const std::string& /*in_ip_endpoint*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(RemoveWakeOnPacketConnectionAsync,
               void(const std::string& /*in_ip_endpoint*/,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(RemoveAllWakeOnPacketConnections,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(RemoveAllWakeOnPacketConnectionsAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetProperties,
               bool(chromeos::VariantDictionary*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetPropertiesAsync,
               void(const base::Callback<void(const chromeos::VariantDictionary&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SetProperty,
               bool(const std::string&,
                    const chromeos::Any&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(SetPropertyAsync,
               void(const std::string&,
                    const chromeos::Any&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ClearProperty,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(ClearPropertyAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(Enable,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(EnableAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(Disable,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(DisableAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(ProposeScan,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ProposeScanAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(AddIPConfig,
               bool(const std::string&,
                    dbus::ObjectPath*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(AddIPConfigAsync,
               void(const std::string&,
                    const base::Callback<void(const dbus::ObjectPath&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(Register,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(RegisterAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(RequirePin,
               bool(const std::string&,
                    bool,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(RequirePinAsync,
               void(const std::string&,
                    bool,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(EnterPin,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(EnterPinAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(UnblockPin,
               bool(const std::string&,
                    const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(UnblockPinAsync,
               void(const std::string&,
                    const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(ChangePin,
               bool(const std::string&,
                    const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(ChangePinAsync,
               void(const std::string&,
                    const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(PerformTDLSOperation,
               bool(const std::string&,
                    const std::string&,
                    std::string*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(PerformTDLSOperationAsync,
               void(const std::string&,
                    const std::string&,
                    const base::Callback<void(const std::string&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(Reset,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ResetAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(ResetByteCounters,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ResetByteCountersAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(SetCarrier,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SetCarrierAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(RequestRoam,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(RequestRoamAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(RegisterPropertyChangedSignalHandler,
               void(const base::Callback<void(const std::string&,
                                              const chromeos::Any&)>& /*signal_callback*/,
                    dbus::ObjectProxy::OnConnectedCallback /*on_connected_callback*/));

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceProxyMock);
};
}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Mock object for IPConfigProxyInterface.
class IPConfigProxyMock : public IPConfigProxyInterface {
 public:
  IPConfigProxyMock() = default;

  MOCK_METHOD3(GetProperties,
               bool(chromeos::VariantDictionary*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetPropertiesAsync,
               void(const base::Callback<void(const chromeos::VariantDictionary&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SetProperty,
               bool(const std::string&,
                    const chromeos::Any&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(SetPropertyAsync,
               void(const std::string&,
                    const chromeos::Any&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ClearProperty,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(ClearPropertyAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(Remove,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(RemoveAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(Refresh,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(RefreshAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(RegisterPropertyChangedSignalHandler,
               void(const base::Callback<void(const std::string&,
                                              const chromeos::Any&)>& /*signal_callback*/,
                    dbus::ObjectProxy::OnConnectedCallback /*on_connected_callback*/));

 private:
  DISALLOW_COPY_AND_ASSIGN(IPConfigProxyMock);
};
}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Mock object for ManagerProxyInterface.
class ManagerProxyMock : public ManagerProxyInterface {
 public:
  ManagerProxyMock() = default;

  MOCK_METHOD3(GetProperties,
               bool(chromeos::VariantDictionary*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetPropertiesAsync,
               void(const base::Callback<void(const chromeos::VariantDictionary&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SetProperty,
               bool(const std::string&,
                    const chromeos::Any&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(SetPropertyAsync,
               void(const std::string&,
                    const chromeos::Any&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetState,
               bool(std::string*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetStateAsync,
               void(const base::Callback<void(const std::string&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(CreateProfile,
               bool(const std::string&,
                    dbus::ObjectPath*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(CreateProfileAsync,
               void(const std::string&,
                    const base::Callback<void(const dbus::ObjectPath&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(RemoveProfile,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(RemoveProfileAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(PushProfile,
               bool(const std::string&,
                    dbus::ObjectPath*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(PushProfileAsync,
               void(const std::string&,
                    const base::Callback<void(const dbus::ObjectPath&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(InsertUserProfile,
               bool(const std::string&,
                    const std::string&,
                    dbus::ObjectPath*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(InsertUserProfileAsync,
               void(const std::string&,
                    const std::string&,
                    const base::Callback<void(const dbus::ObjectPath&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(PopProfile,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(PopProfileAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(PopAnyProfile,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(PopAnyProfileAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(PopAllUserProfiles,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(PopAllUserProfilesAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(RecheckPortal,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(RecheckPortalAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(RequestScan,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(RequestScanAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(EnableTechnology,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(EnableTechnologyAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(DisableTechnology,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(DisableTechnologyAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(GetService,
               bool(const chromeos::VariantDictionary&,
                    dbus::ObjectPath*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(GetServiceAsync,
               void(const chromeos::VariantDictionary&,
                    const base::Callback<void(const dbus::ObjectPath&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(GetWifiService,
               bool(const chromeos::VariantDictionary&,
                    dbus::ObjectPath*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(GetWifiServiceAsync,
               void(const chromeos::VariantDictionary&,
                    const base::Callback<void(const dbus::ObjectPath&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(ConfigureService,
               bool(const chromeos::VariantDictionary&,
                    dbus::ObjectPath*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(ConfigureServiceAsync,
               void(const chromeos::VariantDictionary&,
                    const base::Callback<void(const dbus::ObjectPath&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(ConfigureServiceForProfile,
               bool(const dbus::ObjectPath&,
                    const chromeos::VariantDictionary&,
                    dbus::ObjectPath*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(ConfigureServiceForProfileAsync,
               void(const dbus::ObjectPath&,
                    const chromeos::VariantDictionary&,
                    const base::Callback<void(const dbus::ObjectPath&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(FindMatchingService,
               bool(const chromeos::VariantDictionary&,
                    dbus::ObjectPath*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(FindMatchingServiceAsync,
               void(const chromeos::VariantDictionary&,
                    const base::Callback<void(const dbus::ObjectPath&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(GetVPNService,
               bool(const chromeos::VariantDictionary&,
                    dbus::ObjectPath*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(GetVPNServiceAsync,
               void(const chromeos::VariantDictionary&,
                    const base::Callback<void(const dbus::ObjectPath&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetDebugLevel,
               bool(int32_t*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetDebugLevelAsync,
               void(const base::Callback<void(int32_t)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(SetDebugLevel,
               bool(int32_t,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SetDebugLevelAsync,
               void(int32_t,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetServiceOrder,
               bool(std::string*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetServiceOrderAsync,
               void(const base::Callback<void(const std::string&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(SetServiceOrder,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SetServiceOrderAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetDebugTags,
               bool(std::string*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetDebugTagsAsync,
               void(const base::Callback<void(const std::string&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(SetDebugTags,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SetDebugTagsAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ListDebugTags,
               bool(std::string*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ListDebugTagsAsync,
               void(const base::Callback<void(const std::string&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetNetworksForGeolocation,
               bool(chromeos::VariantDictionary*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetNetworksForGeolocationAsync,
               void(const base::Callback<void(const chromeos::VariantDictionary&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD10(VerifyDestination,
                bool(const std::string& /*in_certificate*/,
                     const std::string& /*in_public_key*/,
                     const std::string& /*in_nonce*/,
                     const std::string& /*in_signed_data*/,
                     const std::string& /*in_destination_udn*/,
                     const std::string& /*in_hotspot_ssid*/,
                     const std::string& /*in_hotspot_bssid*/,
                     bool*,
                     chromeos::ErrorPtr* /*error*/,
                     int /*timeout_ms*/));
  MOCK_METHOD10(VerifyDestinationAsync,
                void(const std::string& /*in_certificate*/,
                     const std::string& /*in_public_key*/,
                     const std::string& /*in_nonce*/,
                     const std::string& /*in_signed_data*/,
                     const std::string& /*in_destination_udn*/,
                     const std::string& /*in_hotspot_ssid*/,
                     const std::string& /*in_hotspot_bssid*/,
                     const base::Callback<void(bool)>& /*success_callback*/,
                     const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                     int /*timeout_ms*/));
  bool VerifyAndEncryptCredentials(const std::string& /*in_certificate*/,
                                   const std::string& /*in_public_key*/,
                                   const std::string& /*in_nonce*/,
                                   const std::string& /*in_signed_data*/,
                                   const std::string& /*in_destination_udn*/,
                                   const std::string& /*in_hotspot_ssid*/,
                                   const std::string& /*in_hotspot_bssid*/,
                                   const dbus::ObjectPath& /*in_network*/,
                                   std::string*,
                                   chromeos::ErrorPtr* /*error*/,
                                   int /*timeout_ms*/) override {
    LOG(WARNING) << "VerifyAndEncryptCredentials(): gmock can't handle methods with 11 arguments. You can override this method in a subclass if you need to.";
    return false;
  }
  void VerifyAndEncryptCredentialsAsync(const std::string& /*in_certificate*/,
                                        const std::string& /*in_public_key*/,
                                        const std::string& /*in_nonce*/,
                                        const std::string& /*in_signed_data*/,
                                        const std::string& /*in_destination_udn*/,
                                        const std::string& /*in_hotspot_ssid*/,
                                        const std::string& /*in_hotspot_bssid*/,
                                        const dbus::ObjectPath& /*in_network*/,
                                        const base::Callback<void(const std::string&)>& /*success_callback*/,
                                        const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                                        int /*timeout_ms*/) override {
    LOG(WARNING) << "VerifyAndEncryptCredentialsAsync(): gmock can't handle methods with 11 arguments. You can override this method in a subclass if you need to.";
  }
  bool VerifyAndEncryptData(const std::string& /*in_certificate*/,
                            const std::string& /*in_public_key*/,
                            const std::string& /*in_nonce*/,
                            const std::string& /*in_signed_data*/,
                            const std::string& /*in_destination_udn*/,
                            const std::string& /*in_hotspot_ssid*/,
                            const std::string& /*in_hotspot_bssid*/,
                            const std::string& /*in_data*/,
                            std::string*,
                            chromeos::ErrorPtr* /*error*/,
                            int /*timeout_ms*/) override {
    LOG(WARNING) << "VerifyAndEncryptData(): gmock can't handle methods with 11 arguments. You can override this method in a subclass if you need to.";
    return false;
  }
  void VerifyAndEncryptDataAsync(const std::string& /*in_certificate*/,
                                 const std::string& /*in_public_key*/,
                                 const std::string& /*in_nonce*/,
                                 const std::string& /*in_signed_data*/,
                                 const std::string& /*in_destination_udn*/,
                                 const std::string& /*in_hotspot_ssid*/,
                                 const std::string& /*in_hotspot_bssid*/,
                                 const std::string& /*in_data*/,
                                 const base::Callback<void(const std::string&)>& /*success_callback*/,
                                 const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                                 int /*timeout_ms*/) override {
    LOG(WARNING) << "VerifyAndEncryptDataAsync(): gmock can't handle methods with 11 arguments. You can override this method in a subclass if you need to.";
  }
  MOCK_METHOD2(ConnectToBestServices,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ConnectToBestServicesAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(CreateConnectivityReport,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(CreateConnectivityReportAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(ClaimInterface,
               bool(const std::string& /*in_claimer_name*/,
                    const std::string& /*in_interface_name*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(ClaimInterfaceAsync,
               void(const std::string& /*in_claimer_name*/,
                    const std::string& /*in_interface_name*/,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(ReleaseInterface,
               bool(const std::string& /*in_claimer_name*/,
                    const std::string& /*in_interface_name*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(ReleaseInterfaceAsync,
               void(const std::string& /*in_claimer_name*/,
                    const std::string& /*in_interface_name*/,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(SetSchedScan,
               bool(bool,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SetSchedScanAsync,
               void(bool,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(RegisterPropertyChangedSignalHandler,
               void(const base::Callback<void(const std::string&,
                                              const chromeos::Any&)>& /*signal_callback*/,
                    dbus::ObjectProxy::OnConnectedCallback /*on_connected_callback*/));
  MOCK_METHOD2(RegisterStateChangedSignalHandler,
               void(const base::Callback<void(const std::string&)>& /*signal_callback*/,
                    dbus::ObjectProxy::OnConnectedCallback /*on_connected_callback*/));

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagerProxyMock);
};
}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Mock object for ProfileProxyInterface.
class ProfileProxyMock : public ProfileProxyInterface {
 public:
  ProfileProxyMock() = default;

  MOCK_METHOD3(GetProperties,
               bool(chromeos::VariantDictionary*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetPropertiesAsync,
               void(const base::Callback<void(const chromeos::VariantDictionary&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SetProperty,
               bool(const std::string&,
                    const chromeos::Any&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(SetPropertyAsync,
               void(const std::string&,
                    const chromeos::Any&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(GetEntry,
               bool(const std::string&,
                    chromeos::VariantDictionary*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(GetEntryAsync,
               void(const std::string&,
                    const base::Callback<void(const chromeos::VariantDictionary&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(DeleteEntry,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(DeleteEntryAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(RegisterPropertyChangedSignalHandler,
               void(const base::Callback<void(const std::string&,
                                              const chromeos::Any&)>& /*signal_callback*/,
                    dbus::ObjectProxy::OnConnectedCallback /*on_connected_callback*/));

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileProxyMock);
};
}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Mock object for ServiceProxyInterface.
class ServiceProxyMock : public ServiceProxyInterface {
 public:
  ServiceProxyMock() = default;

  MOCK_METHOD3(GetProperties,
               bool(chromeos::VariantDictionary*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetPropertiesAsync,
               void(const base::Callback<void(const chromeos::VariantDictionary&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SetProperty,
               bool(const std::string&,
                    const chromeos::Any&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(SetPropertyAsync,
               void(const std::string&,
                    const chromeos::Any&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(SetProperties,
               bool(const chromeos::VariantDictionary&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SetPropertiesAsync,
               void(const chromeos::VariantDictionary&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ClearProperty,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(ClearPropertyAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(ClearProperties,
               bool(const std::vector<std::string>&,
                    std::vector<bool>*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(ClearPropertiesAsync,
               void(const std::vector<std::string>&,
                    const base::Callback<void(const std::vector<bool>&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(Connect,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ConnectAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(Disconnect,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(DisconnectAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(Remove,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(RemoveAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ActivateCellularModem,
               bool(const std::string&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(ActivateCellularModemAsync,
               void(const std::string&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(CompleteCellularActivation,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(CompleteCellularActivationAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetLoadableProfileEntries,
               bool(std::map<dbus::ObjectPath, std::string>*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetLoadableProfileEntriesAsync,
               void(const base::Callback<void(const std::map<dbus::ObjectPath, std::string>&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(RegisterPropertyChangedSignalHandler,
               void(const base::Callback<void(const std::string&,
                                              const chromeos::Any&)>& /*signal_callback*/,
                    dbus::ObjectProxy::OnConnectedCallback /*on_connected_callback*/));

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceProxyMock);
};
}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Mock object for TaskProxyInterface.
class TaskProxyMock : public TaskProxyInterface {
 public:
  TaskProxyMock() = default;

  MOCK_METHOD4(getsec,
               bool(std::string*,
                    std::string*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(getsecAsync,
               void(const base::Callback<void(const std::string&, const std::string&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(notify,
               bool(const std::string&,
                    const std::map<std::string, std::string>&,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(notifyAsync,
               void(const std::string&,
                    const std::map<std::string, std::string>&,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskProxyMock);
};
}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Mock object for ThirdPartyVpnProxyInterface.
class ThirdPartyVpnProxyMock : public ThirdPartyVpnProxyInterface {
 public:
  ThirdPartyVpnProxyMock() = default;

  MOCK_METHOD4(SetParameters,
               bool(const std::map<std::string, std::string>& /*in_parameters*/,
                    std::string* /*out_warning*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SetParametersAsync,
               void(const std::map<std::string, std::string>& /*in_parameters*/,
                    const base::Callback<void(const std::string& /*warning*/)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(UpdateConnectionState,
               bool(uint32_t /*in_connection_state*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(UpdateConnectionStateAsync,
               void(uint32_t /*in_connection_state*/,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(SendPacket,
               bool(const std::vector<uint8_t>& /*in_ip_packet*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(SendPacketAsync,
               void(const std::vector<uint8_t>& /*in_ip_packet*/,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(RegisterOnPacketReceivedSignalHandler,
               void(const base::Callback<void(const std::vector<uint8_t>&)>& /*signal_callback*/,
                    dbus::ObjectProxy::OnConnectedCallback /*on_connected_callback*/));
  MOCK_METHOD2(RegisterOnPlatformMessageSignalHandler,
               void(const base::Callback<void(uint32_t)>& /*signal_callback*/,
                    dbus::ObjectProxy::OnConnectedCallback /*on_connected_callback*/));

 private:
  DISALLOW_COPY_AND_ASSIGN(ThirdPartyVpnProxyMock);
};
}  // namespace flimflam
}  // namespace chromium
}  // namespace org

#endif  // ____CHROMEOS_DBUS_BINDING___BUILD_LINK_VAR_CACHE_PORTAGE_CHROMEOS_BASE_SHILL_OUT_DEFAULT_GEN_INCLUDE_SHILL_DBUS_PROXY_MOCKS_H
