// Automatic generation of D-Bus interfaces:
//  - org.chromium.flimflam.Device
//  - org.chromium.flimflam.IPConfig
//  - org.chromium.flimflam.Manager
//  - org.chromium.flimflam.Profile
//  - org.chromium.flimflam.Service
//  - org.chromium.flimflam.Task
//  - org.chromium.flimflam.ThirdPartyVpn
#ifndef ____CHROMEOS_DBUS_BINDING___BUILD_LINK_VAR_CACHE_PORTAGE_CHROMEOS_BASE_SHILL_OUT_DEFAULT_GEN_INCLUDE_SHILL_DBUS_PROXIES_H
#define ____CHROMEOS_DBUS_BINDING___BUILD_LINK_VAR_CACHE_PORTAGE_CHROMEOS_BASE_SHILL_OUT_DEFAULT_GEN_INCLUDE_SHILL_DBUS_PROXIES_H
#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <chromeos/any.h>
#include <chromeos/dbus/dbus_method_invoker.h>
#include <chromeos/dbus/dbus_property.h>
#include <chromeos/dbus/dbus_signal_handler.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_manager.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>

namespace org {
namespace chromium {
namespace flimflam {

// Abstract interface proxy for org::chromium::flimflam::Device.
class DeviceProxyInterface {
 public:
  virtual ~DeviceProxyInterface() = default;

  virtual bool AddWakeOnPacketConnection(
      const std::string& in_ip_endpoint,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void AddWakeOnPacketConnectionAsync(
      const std::string& in_ip_endpoint,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool RemoveWakeOnPacketConnection(
      const std::string& in_ip_endpoint,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RemoveWakeOnPacketConnectionAsync(
      const std::string& in_ip_endpoint,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool RemoveAllWakeOnPacketConnections(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RemoveAllWakeOnPacketConnectionsAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool GetProperties(
      chromeos::VariantDictionary* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetPropertiesAsync(
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool SetProperty(
      const std::string& in_1,
      const chromeos::Any& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SetPropertyAsync(
      const std::string& in_1,
      const chromeos::Any& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ClearProperty(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ClearPropertyAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool Enable(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void EnableAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool Disable(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void DisableAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ProposeScan(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ProposeScanAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool AddIPConfig(
      const std::string& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void AddIPConfigAsync(
      const std::string& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool Register(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RegisterAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool RequirePin(
      const std::string& in_1,
      bool in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RequirePinAsync(
      const std::string& in_1,
      bool in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool EnterPin(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void EnterPinAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool UnblockPin(
      const std::string& in_1,
      const std::string& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void UnblockPinAsync(
      const std::string& in_1,
      const std::string& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ChangePin(
      const std::string& in_1,
      const std::string& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ChangePinAsync(
      const std::string& in_1,
      const std::string& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool PerformTDLSOperation(
      const std::string& in_1,
      const std::string& in_2,
      std::string* out_3,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void PerformTDLSOperationAsync(
      const std::string& in_1,
      const std::string& in_2,
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool Reset(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ResetAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ResetByteCounters(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ResetByteCountersAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool SetCarrier(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SetCarrierAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool RequestRoam(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RequestRoamAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RegisterPropertyChangedSignalHandler(
      const base::Callback<void(const std::string&,
                                const chromeos::Any&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) = 0;
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Interface proxy for org::chromium::flimflam::Device.
class DeviceProxy final : public DeviceProxyInterface {
 public:
  DeviceProxy(
      const scoped_refptr<dbus::Bus>& bus,
      const dbus::ObjectPath& object_path) :
          bus_{bus},
          object_path_{object_path},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~DeviceProxy() override {
    bus_->RemoveObjectProxy(
        service_name_, object_path_, base::Bind(&base::DoNothing));
  }

  void RegisterPropertyChangedSignalHandler(
      const base::Callback<void(const std::string&,
                                const chromeos::Any&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) override {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "PropertyChanged",
        signal_callback,
        on_connected_callback);
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  bool AddWakeOnPacketConnection(
      const std::string& in_ip_endpoint,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "AddWakeOnPacketConnection",
        error,
        in_ip_endpoint);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void AddWakeOnPacketConnectionAsync(
      const std::string& in_ip_endpoint,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "AddWakeOnPacketConnection",
        success_callback,
        error_callback,
        in_ip_endpoint);
  }

  bool RemoveWakeOnPacketConnection(
      const std::string& in_ip_endpoint,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "RemoveWakeOnPacketConnection",
        error,
        in_ip_endpoint);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void RemoveWakeOnPacketConnectionAsync(
      const std::string& in_ip_endpoint,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "RemoveWakeOnPacketConnection",
        success_callback,
        error_callback,
        in_ip_endpoint);
  }

  bool RemoveAllWakeOnPacketConnections(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "RemoveAllWakeOnPacketConnections",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void RemoveAllWakeOnPacketConnectionsAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "RemoveAllWakeOnPacketConnections",
        success_callback,
        error_callback);
  }

  bool GetProperties(
      chromeos::VariantDictionary* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "GetProperties",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void GetPropertiesAsync(
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "GetProperties",
        success_callback,
        error_callback);
  }

  bool SetProperty(
      const std::string& in_1,
      const chromeos::Any& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "SetProperty",
        error,
        in_1,
        in_2);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void SetPropertyAsync(
      const std::string& in_1,
      const chromeos::Any& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "SetProperty",
        success_callback,
        error_callback,
        in_1,
        in_2);
  }

  bool ClearProperty(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "ClearProperty",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void ClearPropertyAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "ClearProperty",
        success_callback,
        error_callback,
        in_1);
  }

  bool Enable(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "Enable",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void EnableAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "Enable",
        success_callback,
        error_callback);
  }

  bool Disable(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "Disable",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void DisableAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "Disable",
        success_callback,
        error_callback);
  }

  bool ProposeScan(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "ProposeScan",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void ProposeScanAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "ProposeScan",
        success_callback,
        error_callback);
  }

  bool AddIPConfig(
      const std::string& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "AddIPConfig",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_2);
  }

  void AddIPConfigAsync(
      const std::string& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "AddIPConfig",
        success_callback,
        error_callback,
        in_1);
  }

  bool Register(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "Register",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void RegisterAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "Register",
        success_callback,
        error_callback,
        in_1);
  }

  bool RequirePin(
      const std::string& in_1,
      bool in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "RequirePin",
        error,
        in_1,
        in_2);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void RequirePinAsync(
      const std::string& in_1,
      bool in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "RequirePin",
        success_callback,
        error_callback,
        in_1,
        in_2);
  }

  bool EnterPin(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "EnterPin",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void EnterPinAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "EnterPin",
        success_callback,
        error_callback,
        in_1);
  }

  bool UnblockPin(
      const std::string& in_1,
      const std::string& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "UnblockPin",
        error,
        in_1,
        in_2);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void UnblockPinAsync(
      const std::string& in_1,
      const std::string& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "UnblockPin",
        success_callback,
        error_callback,
        in_1,
        in_2);
  }

  bool ChangePin(
      const std::string& in_1,
      const std::string& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "ChangePin",
        error,
        in_1,
        in_2);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void ChangePinAsync(
      const std::string& in_1,
      const std::string& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "ChangePin",
        success_callback,
        error_callback,
        in_1,
        in_2);
  }

  bool PerformTDLSOperation(
      const std::string& in_1,
      const std::string& in_2,
      std::string* out_3,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "PerformTDLSOperation",
        error,
        in_1,
        in_2);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_3);
  }

  void PerformTDLSOperationAsync(
      const std::string& in_1,
      const std::string& in_2,
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "PerformTDLSOperation",
        success_callback,
        error_callback,
        in_1,
        in_2);
  }

  bool Reset(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "Reset",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void ResetAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "Reset",
        success_callback,
        error_callback);
  }

  bool ResetByteCounters(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "ResetByteCounters",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void ResetByteCountersAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "ResetByteCounters",
        success_callback,
        error_callback);
  }

  bool SetCarrier(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "SetCarrier",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void SetCarrierAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "SetCarrier",
        success_callback,
        error_callback,
        in_1);
  }

  bool RequestRoam(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "RequestRoam",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void RequestRoamAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Device",
        "RequestRoam",
        success_callback,
        error_callback,
        in_1);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.flimflam"};
  dbus::ObjectPath object_path_;
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(DeviceProxy);
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Abstract interface proxy for org::chromium::flimflam::IPConfig.
class IPConfigProxyInterface {
 public:
  virtual ~IPConfigProxyInterface() = default;

  virtual bool GetProperties(
      chromeos::VariantDictionary* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetPropertiesAsync(
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool SetProperty(
      const std::string& in_1,
      const chromeos::Any& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SetPropertyAsync(
      const std::string& in_1,
      const chromeos::Any& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ClearProperty(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ClearPropertyAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool Remove(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RemoveAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool Refresh(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RefreshAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RegisterPropertyChangedSignalHandler(
      const base::Callback<void(const std::string&,
                                const chromeos::Any&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) = 0;
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Interface proxy for org::chromium::flimflam::IPConfig.
class IPConfigProxy final : public IPConfigProxyInterface {
 public:
  IPConfigProxy(
      const scoped_refptr<dbus::Bus>& bus,
      const dbus::ObjectPath& object_path) :
          bus_{bus},
          object_path_{object_path},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~IPConfigProxy() override {
    bus_->RemoveObjectProxy(
        service_name_, object_path_, base::Bind(&base::DoNothing));
  }

  void RegisterPropertyChangedSignalHandler(
      const base::Callback<void(const std::string&,
                                const chromeos::Any&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) override {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.flimflam.IPConfig",
        "PropertyChanged",
        signal_callback,
        on_connected_callback);
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  bool GetProperties(
      chromeos::VariantDictionary* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.IPConfig",
        "GetProperties",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void GetPropertiesAsync(
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.IPConfig",
        "GetProperties",
        success_callback,
        error_callback);
  }

  bool SetProperty(
      const std::string& in_1,
      const chromeos::Any& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.IPConfig",
        "SetProperty",
        error,
        in_1,
        in_2);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void SetPropertyAsync(
      const std::string& in_1,
      const chromeos::Any& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.IPConfig",
        "SetProperty",
        success_callback,
        error_callback,
        in_1,
        in_2);
  }

  bool ClearProperty(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.IPConfig",
        "ClearProperty",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void ClearPropertyAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.IPConfig",
        "ClearProperty",
        success_callback,
        error_callback,
        in_1);
  }

  bool Remove(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.IPConfig",
        "Remove",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void RemoveAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.IPConfig",
        "Remove",
        success_callback,
        error_callback);
  }

  bool Refresh(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.IPConfig",
        "Refresh",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void RefreshAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.IPConfig",
        "Refresh",
        success_callback,
        error_callback);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.flimflam"};
  dbus::ObjectPath object_path_;
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(IPConfigProxy);
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Abstract interface proxy for org::chromium::flimflam::Manager.
class ManagerProxyInterface {
 public:
  virtual ~ManagerProxyInterface() = default;

  virtual bool GetProperties(
      chromeos::VariantDictionary* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetPropertiesAsync(
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool SetProperty(
      const std::string& in_1,
      const chromeos::Any& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SetPropertyAsync(
      const std::string& in_1,
      const chromeos::Any& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool GetState(
      std::string* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetStateAsync(
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool CreateProfile(
      const std::string& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void CreateProfileAsync(
      const std::string& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool RemoveProfile(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RemoveProfileAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool PushProfile(
      const std::string& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void PushProfileAsync(
      const std::string& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool InsertUserProfile(
      const std::string& in_1,
      const std::string& in_2,
      dbus::ObjectPath* out_3,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void InsertUserProfileAsync(
      const std::string& in_1,
      const std::string& in_2,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool PopProfile(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void PopProfileAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool PopAnyProfile(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void PopAnyProfileAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool PopAllUserProfiles(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void PopAllUserProfilesAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool RecheckPortal(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RecheckPortalAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool RequestScan(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RequestScanAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool EnableTechnology(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void EnableTechnologyAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool DisableTechnology(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void DisableTechnologyAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool GetService(
      const chromeos::VariantDictionary& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetServiceAsync(
      const chromeos::VariantDictionary& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool GetWifiService(
      const chromeos::VariantDictionary& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetWifiServiceAsync(
      const chromeos::VariantDictionary& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ConfigureService(
      const chromeos::VariantDictionary& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ConfigureServiceAsync(
      const chromeos::VariantDictionary& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ConfigureServiceForProfile(
      const dbus::ObjectPath& in_1,
      const chromeos::VariantDictionary& in_2,
      dbus::ObjectPath* out_3,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ConfigureServiceForProfileAsync(
      const dbus::ObjectPath& in_1,
      const chromeos::VariantDictionary& in_2,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool FindMatchingService(
      const chromeos::VariantDictionary& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void FindMatchingServiceAsync(
      const chromeos::VariantDictionary& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool GetVPNService(
      const chromeos::VariantDictionary& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetVPNServiceAsync(
      const chromeos::VariantDictionary& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool GetDebugLevel(
      int32_t* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetDebugLevelAsync(
      const base::Callback<void(int32_t)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool SetDebugLevel(
      int32_t in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SetDebugLevelAsync(
      int32_t in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool GetServiceOrder(
      std::string* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetServiceOrderAsync(
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool SetServiceOrder(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SetServiceOrderAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool GetDebugTags(
      std::string* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetDebugTagsAsync(
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool SetDebugTags(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SetDebugTagsAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ListDebugTags(
      std::string* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ListDebugTagsAsync(
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool GetNetworksForGeolocation(
      chromeos::VariantDictionary* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetNetworksForGeolocationAsync(
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool VerifyDestination(
      const std::string& in_certificate,
      const std::string& in_public_key,
      const std::string& in_nonce,
      const std::string& in_signed_data,
      const std::string& in_destination_udn,
      const std::string& in_hotspot_ssid,
      const std::string& in_hotspot_bssid,
      bool* out_8,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void VerifyDestinationAsync(
      const std::string& in_certificate,
      const std::string& in_public_key,
      const std::string& in_nonce,
      const std::string& in_signed_data,
      const std::string& in_destination_udn,
      const std::string& in_hotspot_ssid,
      const std::string& in_hotspot_bssid,
      const base::Callback<void(bool)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool VerifyAndEncryptCredentials(
      const std::string& in_certificate,
      const std::string& in_public_key,
      const std::string& in_nonce,
      const std::string& in_signed_data,
      const std::string& in_destination_udn,
      const std::string& in_hotspot_ssid,
      const std::string& in_hotspot_bssid,
      const dbus::ObjectPath& in_network,
      std::string* out_9,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void VerifyAndEncryptCredentialsAsync(
      const std::string& in_certificate,
      const std::string& in_public_key,
      const std::string& in_nonce,
      const std::string& in_signed_data,
      const std::string& in_destination_udn,
      const std::string& in_hotspot_ssid,
      const std::string& in_hotspot_bssid,
      const dbus::ObjectPath& in_network,
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool VerifyAndEncryptData(
      const std::string& in_certificate,
      const std::string& in_public_key,
      const std::string& in_nonce,
      const std::string& in_signed_data,
      const std::string& in_destination_udn,
      const std::string& in_hotspot_ssid,
      const std::string& in_hotspot_bssid,
      const std::string& in_data,
      std::string* out_9,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void VerifyAndEncryptDataAsync(
      const std::string& in_certificate,
      const std::string& in_public_key,
      const std::string& in_nonce,
      const std::string& in_signed_data,
      const std::string& in_destination_udn,
      const std::string& in_hotspot_ssid,
      const std::string& in_hotspot_bssid,
      const std::string& in_data,
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ConnectToBestServices(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ConnectToBestServicesAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool CreateConnectivityReport(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void CreateConnectivityReportAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ClaimInterface(
      const std::string& in_claimer_name,
      const std::string& in_interface_name,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ClaimInterfaceAsync(
      const std::string& in_claimer_name,
      const std::string& in_interface_name,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ReleaseInterface(
      const std::string& in_claimer_name,
      const std::string& in_interface_name,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ReleaseInterfaceAsync(
      const std::string& in_claimer_name,
      const std::string& in_interface_name,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool SetSchedScan(
      bool in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SetSchedScanAsync(
      bool in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RegisterPropertyChangedSignalHandler(
      const base::Callback<void(const std::string&,
                                const chromeos::Any&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) = 0;

  virtual void RegisterStateChangedSignalHandler(
      const base::Callback<void(const std::string&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) = 0;
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Interface proxy for org::chromium::flimflam::Manager.
class ManagerProxy final : public ManagerProxyInterface {
 public:
  ManagerProxy(const scoped_refptr<dbus::Bus>& bus) :
      bus_{bus},
      dbus_object_proxy_{
          bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~ManagerProxy() override {
    bus_->RemoveObjectProxy(
        service_name_, object_path_, base::Bind(&base::DoNothing));
  }

  void RegisterPropertyChangedSignalHandler(
      const base::Callback<void(const std::string&,
                                const chromeos::Any&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) override {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "PropertyChanged",
        signal_callback,
        on_connected_callback);
  }

  void RegisterStateChangedSignalHandler(
      const base::Callback<void(const std::string&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) override {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "StateChanged",
        signal_callback,
        on_connected_callback);
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  bool GetProperties(
      chromeos::VariantDictionary* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetProperties",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void GetPropertiesAsync(
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetProperties",
        success_callback,
        error_callback);
  }

  bool SetProperty(
      const std::string& in_1,
      const chromeos::Any& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "SetProperty",
        error,
        in_1,
        in_2);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void SetPropertyAsync(
      const std::string& in_1,
      const chromeos::Any& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "SetProperty",
        success_callback,
        error_callback,
        in_1,
        in_2);
  }

  bool GetState(
      std::string* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetState",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void GetStateAsync(
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetState",
        success_callback,
        error_callback);
  }

  bool CreateProfile(
      const std::string& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "CreateProfile",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_2);
  }

  void CreateProfileAsync(
      const std::string& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "CreateProfile",
        success_callback,
        error_callback,
        in_1);
  }

  bool RemoveProfile(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "RemoveProfile",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void RemoveProfileAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "RemoveProfile",
        success_callback,
        error_callback,
        in_1);
  }

  bool PushProfile(
      const std::string& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "PushProfile",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_2);
  }

  void PushProfileAsync(
      const std::string& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "PushProfile",
        success_callback,
        error_callback,
        in_1);
  }

  bool InsertUserProfile(
      const std::string& in_1,
      const std::string& in_2,
      dbus::ObjectPath* out_3,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "InsertUserProfile",
        error,
        in_1,
        in_2);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_3);
  }

  void InsertUserProfileAsync(
      const std::string& in_1,
      const std::string& in_2,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "InsertUserProfile",
        success_callback,
        error_callback,
        in_1,
        in_2);
  }

  bool PopProfile(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "PopProfile",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void PopProfileAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "PopProfile",
        success_callback,
        error_callback,
        in_1);
  }

  bool PopAnyProfile(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "PopAnyProfile",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void PopAnyProfileAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "PopAnyProfile",
        success_callback,
        error_callback);
  }

  bool PopAllUserProfiles(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "PopAllUserProfiles",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void PopAllUserProfilesAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "PopAllUserProfiles",
        success_callback,
        error_callback);
  }

  bool RecheckPortal(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "RecheckPortal",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void RecheckPortalAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "RecheckPortal",
        success_callback,
        error_callback);
  }

  bool RequestScan(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "RequestScan",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void RequestScanAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "RequestScan",
        success_callback,
        error_callback,
        in_1);
  }

  bool EnableTechnology(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "EnableTechnology",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void EnableTechnologyAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "EnableTechnology",
        success_callback,
        error_callback,
        in_1);
  }

  bool DisableTechnology(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "DisableTechnology",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void DisableTechnologyAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "DisableTechnology",
        success_callback,
        error_callback,
        in_1);
  }

  bool GetService(
      const chromeos::VariantDictionary& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetService",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_2);
  }

  void GetServiceAsync(
      const chromeos::VariantDictionary& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetService",
        success_callback,
        error_callback,
        in_1);
  }

  bool GetWifiService(
      const chromeos::VariantDictionary& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetWifiService",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_2);
  }

  void GetWifiServiceAsync(
      const chromeos::VariantDictionary& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetWifiService",
        success_callback,
        error_callback,
        in_1);
  }

  bool ConfigureService(
      const chromeos::VariantDictionary& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "ConfigureService",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_2);
  }

  void ConfigureServiceAsync(
      const chromeos::VariantDictionary& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "ConfigureService",
        success_callback,
        error_callback,
        in_1);
  }

  bool ConfigureServiceForProfile(
      const dbus::ObjectPath& in_1,
      const chromeos::VariantDictionary& in_2,
      dbus::ObjectPath* out_3,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "ConfigureServiceForProfile",
        error,
        in_1,
        in_2);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_3);
  }

  void ConfigureServiceForProfileAsync(
      const dbus::ObjectPath& in_1,
      const chromeos::VariantDictionary& in_2,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "ConfigureServiceForProfile",
        success_callback,
        error_callback,
        in_1,
        in_2);
  }

  bool FindMatchingService(
      const chromeos::VariantDictionary& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "FindMatchingService",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_2);
  }

  void FindMatchingServiceAsync(
      const chromeos::VariantDictionary& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "FindMatchingService",
        success_callback,
        error_callback,
        in_1);
  }

  bool GetVPNService(
      const chromeos::VariantDictionary& in_1,
      dbus::ObjectPath* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetVPNService",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_2);
  }

  void GetVPNServiceAsync(
      const chromeos::VariantDictionary& in_1,
      const base::Callback<void(const dbus::ObjectPath&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetVPNService",
        success_callback,
        error_callback,
        in_1);
  }

  bool GetDebugLevel(
      int32_t* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetDebugLevel",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void GetDebugLevelAsync(
      const base::Callback<void(int32_t)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetDebugLevel",
        success_callback,
        error_callback);
  }

  bool SetDebugLevel(
      int32_t in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "SetDebugLevel",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void SetDebugLevelAsync(
      int32_t in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "SetDebugLevel",
        success_callback,
        error_callback,
        in_1);
  }

  bool GetServiceOrder(
      std::string* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetServiceOrder",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void GetServiceOrderAsync(
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetServiceOrder",
        success_callback,
        error_callback);
  }

  bool SetServiceOrder(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "SetServiceOrder",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void SetServiceOrderAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "SetServiceOrder",
        success_callback,
        error_callback,
        in_1);
  }

  bool GetDebugTags(
      std::string* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetDebugTags",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void GetDebugTagsAsync(
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetDebugTags",
        success_callback,
        error_callback);
  }

  bool SetDebugTags(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "SetDebugTags",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void SetDebugTagsAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "SetDebugTags",
        success_callback,
        error_callback,
        in_1);
  }

  bool ListDebugTags(
      std::string* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "ListDebugTags",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void ListDebugTagsAsync(
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "ListDebugTags",
        success_callback,
        error_callback);
  }

  bool GetNetworksForGeolocation(
      chromeos::VariantDictionary* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetNetworksForGeolocation",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void GetNetworksForGeolocationAsync(
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "GetNetworksForGeolocation",
        success_callback,
        error_callback);
  }

  bool VerifyDestination(
      const std::string& in_certificate,
      const std::string& in_public_key,
      const std::string& in_nonce,
      const std::string& in_signed_data,
      const std::string& in_destination_udn,
      const std::string& in_hotspot_ssid,
      const std::string& in_hotspot_bssid,
      bool* out_8,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "VerifyDestination",
        error,
        in_certificate,
        in_public_key,
        in_nonce,
        in_signed_data,
        in_destination_udn,
        in_hotspot_ssid,
        in_hotspot_bssid);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_8);
  }

  void VerifyDestinationAsync(
      const std::string& in_certificate,
      const std::string& in_public_key,
      const std::string& in_nonce,
      const std::string& in_signed_data,
      const std::string& in_destination_udn,
      const std::string& in_hotspot_ssid,
      const std::string& in_hotspot_bssid,
      const base::Callback<void(bool)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "VerifyDestination",
        success_callback,
        error_callback,
        in_certificate,
        in_public_key,
        in_nonce,
        in_signed_data,
        in_destination_udn,
        in_hotspot_ssid,
        in_hotspot_bssid);
  }

  bool VerifyAndEncryptCredentials(
      const std::string& in_certificate,
      const std::string& in_public_key,
      const std::string& in_nonce,
      const std::string& in_signed_data,
      const std::string& in_destination_udn,
      const std::string& in_hotspot_ssid,
      const std::string& in_hotspot_bssid,
      const dbus::ObjectPath& in_network,
      std::string* out_9,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "VerifyAndEncryptCredentials",
        error,
        in_certificate,
        in_public_key,
        in_nonce,
        in_signed_data,
        in_destination_udn,
        in_hotspot_ssid,
        in_hotspot_bssid,
        in_network);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_9);
  }

  void VerifyAndEncryptCredentialsAsync(
      const std::string& in_certificate,
      const std::string& in_public_key,
      const std::string& in_nonce,
      const std::string& in_signed_data,
      const std::string& in_destination_udn,
      const std::string& in_hotspot_ssid,
      const std::string& in_hotspot_bssid,
      const dbus::ObjectPath& in_network,
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "VerifyAndEncryptCredentials",
        success_callback,
        error_callback,
        in_certificate,
        in_public_key,
        in_nonce,
        in_signed_data,
        in_destination_udn,
        in_hotspot_ssid,
        in_hotspot_bssid,
        in_network);
  }

  bool VerifyAndEncryptData(
      const std::string& in_certificate,
      const std::string& in_public_key,
      const std::string& in_nonce,
      const std::string& in_signed_data,
      const std::string& in_destination_udn,
      const std::string& in_hotspot_ssid,
      const std::string& in_hotspot_bssid,
      const std::string& in_data,
      std::string* out_9,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "VerifyAndEncryptData",
        error,
        in_certificate,
        in_public_key,
        in_nonce,
        in_signed_data,
        in_destination_udn,
        in_hotspot_ssid,
        in_hotspot_bssid,
        in_data);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_9);
  }

  void VerifyAndEncryptDataAsync(
      const std::string& in_certificate,
      const std::string& in_public_key,
      const std::string& in_nonce,
      const std::string& in_signed_data,
      const std::string& in_destination_udn,
      const std::string& in_hotspot_ssid,
      const std::string& in_hotspot_bssid,
      const std::string& in_data,
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "VerifyAndEncryptData",
        success_callback,
        error_callback,
        in_certificate,
        in_public_key,
        in_nonce,
        in_signed_data,
        in_destination_udn,
        in_hotspot_ssid,
        in_hotspot_bssid,
        in_data);
  }

  bool ConnectToBestServices(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "ConnectToBestServices",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void ConnectToBestServicesAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "ConnectToBestServices",
        success_callback,
        error_callback);
  }

  bool CreateConnectivityReport(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "CreateConnectivityReport",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void CreateConnectivityReportAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "CreateConnectivityReport",
        success_callback,
        error_callback);
  }

  bool ClaimInterface(
      const std::string& in_claimer_name,
      const std::string& in_interface_name,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "ClaimInterface",
        error,
        in_claimer_name,
        in_interface_name);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void ClaimInterfaceAsync(
      const std::string& in_claimer_name,
      const std::string& in_interface_name,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "ClaimInterface",
        success_callback,
        error_callback,
        in_claimer_name,
        in_interface_name);
  }

  bool ReleaseInterface(
      const std::string& in_claimer_name,
      const std::string& in_interface_name,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "ReleaseInterface",
        error,
        in_claimer_name,
        in_interface_name);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void ReleaseInterfaceAsync(
      const std::string& in_claimer_name,
      const std::string& in_interface_name,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "ReleaseInterface",
        success_callback,
        error_callback,
        in_claimer_name,
        in_interface_name);
  }

  bool SetSchedScan(
      bool in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "SetSchedScan",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void SetSchedScanAsync(
      bool in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Manager",
        "SetSchedScan",
        success_callback,
        error_callback,
        in_1);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.flimflam"};
  const dbus::ObjectPath object_path_{"/"};
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ManagerProxy);
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Abstract interface proxy for org::chromium::flimflam::Profile.
class ProfileProxyInterface {
 public:
  virtual ~ProfileProxyInterface() = default;

  virtual bool GetProperties(
      chromeos::VariantDictionary* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetPropertiesAsync(
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool SetProperty(
      const std::string& in_1,
      const chromeos::Any& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SetPropertyAsync(
      const std::string& in_1,
      const chromeos::Any& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool GetEntry(
      const std::string& in_1,
      chromeos::VariantDictionary* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetEntryAsync(
      const std::string& in_1,
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool DeleteEntry(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void DeleteEntryAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RegisterPropertyChangedSignalHandler(
      const base::Callback<void(const std::string&,
                                const chromeos::Any&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) = 0;
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Interface proxy for org::chromium::flimflam::Profile.
class ProfileProxy final : public ProfileProxyInterface {
 public:
  ProfileProxy(
      const scoped_refptr<dbus::Bus>& bus,
      const dbus::ObjectPath& object_path) :
          bus_{bus},
          object_path_{object_path},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~ProfileProxy() override {
    bus_->RemoveObjectProxy(
        service_name_, object_path_, base::Bind(&base::DoNothing));
  }

  void RegisterPropertyChangedSignalHandler(
      const base::Callback<void(const std::string&,
                                const chromeos::Any&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) override {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.flimflam.Profile",
        "PropertyChanged",
        signal_callback,
        on_connected_callback);
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  bool GetProperties(
      chromeos::VariantDictionary* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Profile",
        "GetProperties",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void GetPropertiesAsync(
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Profile",
        "GetProperties",
        success_callback,
        error_callback);
  }

  bool SetProperty(
      const std::string& in_1,
      const chromeos::Any& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Profile",
        "SetProperty",
        error,
        in_1,
        in_2);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void SetPropertyAsync(
      const std::string& in_1,
      const chromeos::Any& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Profile",
        "SetProperty",
        success_callback,
        error_callback,
        in_1,
        in_2);
  }

  bool GetEntry(
      const std::string& in_1,
      chromeos::VariantDictionary* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Profile",
        "GetEntry",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_2);
  }

  void GetEntryAsync(
      const std::string& in_1,
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Profile",
        "GetEntry",
        success_callback,
        error_callback,
        in_1);
  }

  bool DeleteEntry(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Profile",
        "DeleteEntry",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void DeleteEntryAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Profile",
        "DeleteEntry",
        success_callback,
        error_callback,
        in_1);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.flimflam"};
  dbus::ObjectPath object_path_;
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ProfileProxy);
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Abstract interface proxy for org::chromium::flimflam::Service.
class ServiceProxyInterface {
 public:
  virtual ~ServiceProxyInterface() = default;

  virtual bool GetProperties(
      chromeos::VariantDictionary* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetPropertiesAsync(
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool SetProperty(
      const std::string& in_1,
      const chromeos::Any& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SetPropertyAsync(
      const std::string& in_1,
      const chromeos::Any& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool SetProperties(
      const chromeos::VariantDictionary& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SetPropertiesAsync(
      const chromeos::VariantDictionary& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ClearProperty(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ClearPropertyAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ClearProperties(
      const std::vector<std::string>& in_1,
      std::vector<bool>* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ClearPropertiesAsync(
      const std::vector<std::string>& in_1,
      const base::Callback<void(const std::vector<bool>&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool Connect(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ConnectAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool Disconnect(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void DisconnectAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool Remove(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RemoveAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool ActivateCellularModem(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void ActivateCellularModemAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool CompleteCellularActivation(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void CompleteCellularActivationAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool GetLoadableProfileEntries(
      std::map<dbus::ObjectPath, std::string>* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void GetLoadableProfileEntriesAsync(
      const base::Callback<void(const std::map<dbus::ObjectPath, std::string>&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RegisterPropertyChangedSignalHandler(
      const base::Callback<void(const std::string&,
                                const chromeos::Any&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) = 0;
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Interface proxy for org::chromium::flimflam::Service.
class ServiceProxy final : public ServiceProxyInterface {
 public:
  ServiceProxy(
      const scoped_refptr<dbus::Bus>& bus,
      const dbus::ObjectPath& object_path) :
          bus_{bus},
          object_path_{object_path},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~ServiceProxy() override {
    bus_->RemoveObjectProxy(
        service_name_, object_path_, base::Bind(&base::DoNothing));
  }

  void RegisterPropertyChangedSignalHandler(
      const base::Callback<void(const std::string&,
                                const chromeos::Any&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) override {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "PropertyChanged",
        signal_callback,
        on_connected_callback);
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  bool GetProperties(
      chromeos::VariantDictionary* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "GetProperties",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void GetPropertiesAsync(
      const base::Callback<void(const chromeos::VariantDictionary&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "GetProperties",
        success_callback,
        error_callback);
  }

  bool SetProperty(
      const std::string& in_1,
      const chromeos::Any& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "SetProperty",
        error,
        in_1,
        in_2);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void SetPropertyAsync(
      const std::string& in_1,
      const chromeos::Any& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "SetProperty",
        success_callback,
        error_callback,
        in_1,
        in_2);
  }

  bool SetProperties(
      const chromeos::VariantDictionary& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "SetProperties",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void SetPropertiesAsync(
      const chromeos::VariantDictionary& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "SetProperties",
        success_callback,
        error_callback,
        in_1);
  }

  bool ClearProperty(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "ClearProperty",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void ClearPropertyAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "ClearProperty",
        success_callback,
        error_callback,
        in_1);
  }

  bool ClearProperties(
      const std::vector<std::string>& in_1,
      std::vector<bool>* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "ClearProperties",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_2);
  }

  void ClearPropertiesAsync(
      const std::vector<std::string>& in_1,
      const base::Callback<void(const std::vector<bool>&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "ClearProperties",
        success_callback,
        error_callback,
        in_1);
  }

  bool Connect(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "Connect",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void ConnectAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "Connect",
        success_callback,
        error_callback);
  }

  bool Disconnect(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "Disconnect",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void DisconnectAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "Disconnect",
        success_callback,
        error_callback);
  }

  bool Remove(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "Remove",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void RemoveAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "Remove",
        success_callback,
        error_callback);
  }

  bool ActivateCellularModem(
      const std::string& in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "ActivateCellularModem",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void ActivateCellularModemAsync(
      const std::string& in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "ActivateCellularModem",
        success_callback,
        error_callback,
        in_1);
  }

  bool CompleteCellularActivation(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "CompleteCellularActivation",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void CompleteCellularActivationAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "CompleteCellularActivation",
        success_callback,
        error_callback);
  }

  bool GetLoadableProfileEntries(
      std::map<dbus::ObjectPath, std::string>* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "GetLoadableProfileEntries",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void GetLoadableProfileEntriesAsync(
      const base::Callback<void(const std::map<dbus::ObjectPath, std::string>&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Service",
        "GetLoadableProfileEntries",
        success_callback,
        error_callback);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.flimflam"};
  dbus::ObjectPath object_path_;
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProxy);
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Abstract interface proxy for org::chromium::flimflam::Task.
class TaskProxyInterface {
 public:
  virtual ~TaskProxyInterface() = default;

  virtual bool getsec(
      std::string* out_1,
      std::string* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void getsecAsync(
      const base::Callback<void(const std::string&, const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool notify(
      const std::string& in_1,
      const std::map<std::string, std::string>& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void notifyAsync(
      const std::string& in_1,
      const std::map<std::string, std::string>& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Interface proxy for org::chromium::flimflam::Task.
class TaskProxy final : public TaskProxyInterface {
 public:
  TaskProxy(
      const scoped_refptr<dbus::Bus>& bus,
      const dbus::ObjectPath& object_path) :
          bus_{bus},
          object_path_{object_path},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~TaskProxy() override {
    bus_->RemoveObjectProxy(
        service_name_, object_path_, base::Bind(&base::DoNothing));
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  bool getsec(
      std::string* out_1,
      std::string* out_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Task",
        "getsec",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1, out_2);
  }

  void getsecAsync(
      const base::Callback<void(const std::string&, const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Task",
        "getsec",
        success_callback,
        error_callback);
  }

  bool notify(
      const std::string& in_1,
      const std::map<std::string, std::string>& in_2,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Task",
        "notify",
        error,
        in_1,
        in_2);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void notifyAsync(
      const std::string& in_1,
      const std::map<std::string, std::string>& in_2,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.Task",
        "notify",
        success_callback,
        error_callback,
        in_1,
        in_2);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.flimflam"};
  dbus::ObjectPath object_path_;
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(TaskProxy);
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Abstract interface proxy for org::chromium::flimflam::ThirdPartyVpn.
class ThirdPartyVpnProxyInterface {
 public:
  virtual ~ThirdPartyVpnProxyInterface() = default;

  virtual bool SetParameters(
      const std::map<std::string, std::string>& in_parameters,
      std::string* out_warning,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SetParametersAsync(
      const std::map<std::string, std::string>& in_parameters,
      const base::Callback<void(const std::string& /*warning*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool UpdateConnectionState(
      uint32_t in_connection_state,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void UpdateConnectionStateAsync(
      uint32_t in_connection_state,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool SendPacket(
      const std::vector<uint8_t>& in_ip_packet,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void SendPacketAsync(
      const std::vector<uint8_t>& in_ip_packet,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RegisterOnPacketReceivedSignalHandler(
      const base::Callback<void(const std::vector<uint8_t>&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) = 0;

  virtual void RegisterOnPlatformMessageSignalHandler(
      const base::Callback<void(uint32_t)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) = 0;
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace flimflam {

// Interface proxy for org::chromium::flimflam::ThirdPartyVpn.
class ThirdPartyVpnProxy final : public ThirdPartyVpnProxyInterface {
 public:
  ThirdPartyVpnProxy(
      const scoped_refptr<dbus::Bus>& bus,
      const dbus::ObjectPath& object_path) :
          bus_{bus},
          object_path_{object_path},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~ThirdPartyVpnProxy() override {
    bus_->RemoveObjectProxy(
        service_name_, object_path_, base::Bind(&base::DoNothing));
  }

  void RegisterOnPacketReceivedSignalHandler(
      const base::Callback<void(const std::vector<uint8_t>&)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) override {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.flimflam.ThirdPartyVpn",
        "OnPacketReceived",
        signal_callback,
        on_connected_callback);
  }

  void RegisterOnPlatformMessageSignalHandler(
      const base::Callback<void(uint32_t)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) override {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.flimflam.ThirdPartyVpn",
        "OnPlatformMessage",
        signal_callback,
        on_connected_callback);
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  bool SetParameters(
      const std::map<std::string, std::string>& in_parameters,
      std::string* out_warning,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.ThirdPartyVpn",
        "SetParameters",
        error,
        in_parameters);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_warning);
  }

  void SetParametersAsync(
      const std::map<std::string, std::string>& in_parameters,
      const base::Callback<void(const std::string& /*warning*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.ThirdPartyVpn",
        "SetParameters",
        success_callback,
        error_callback,
        in_parameters);
  }

  bool UpdateConnectionState(
      uint32_t in_connection_state,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.ThirdPartyVpn",
        "UpdateConnectionState",
        error,
        in_connection_state);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void UpdateConnectionStateAsync(
      uint32_t in_connection_state,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.ThirdPartyVpn",
        "UpdateConnectionState",
        success_callback,
        error_callback,
        in_connection_state);
  }

  bool SendPacket(
      const std::vector<uint8_t>& in_ip_packet,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.ThirdPartyVpn",
        "SendPacket",
        error,
        in_ip_packet);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void SendPacketAsync(
      const std::vector<uint8_t>& in_ip_packet,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.flimflam.ThirdPartyVpn",
        "SendPacket",
        success_callback,
        error_callback,
        in_ip_packet);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.flimflam"};
  dbus::ObjectPath object_path_;
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyVpnProxy);
};

}  // namespace flimflam
}  // namespace chromium
}  // namespace org

#endif  // ____CHROMEOS_DBUS_BINDING___BUILD_LINK_VAR_CACHE_PORTAGE_CHROMEOS_BASE_SHILL_OUT_DEFAULT_GEN_INCLUDE_SHILL_DBUS_PROXIES_H
