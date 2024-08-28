#pragma once

#include <functional>

#include <pipewire/core.h>
#include <pipewire/device.h>
#include <pipewire/type.h>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qtmetamacros.h>
#include <spa/pod/builder.h>

#include "core.hpp"
#include "registry.hpp"

namespace qs::service::pipewire {

class PwDevice;

constexpr const char TYPE_INTERFACE_Device[] = PW_TYPE_INTERFACE_Device; // NOLINT
class PwDevice: public PwBindable<pw_device, TYPE_INTERFACE_Device, PW_VERSION_DEVICE> {
	Q_OBJECT;

public:
	void bindHooks() override;
	void unbindHooks() override;

	bool setVolumes(qint32 routeDevice, const QVector<float>& volumes);
	bool setMuted(qint32 routeDevice, bool muted);

	void waitForDevice();
	[[nodiscard]] bool waitingForDevice() const;

signals:
	void deviceReady();

private slots:
	void polled();

private:
	static const pw_device_events EVENTS;
	static void onInfo(void* data, const pw_device_info* info);
	static void
	onParam(void* data, qint32 seq, quint32 id, quint32 index, quint32 next, const spa_pod* param);

	QHash<qint32, qint32> routeDeviceIndexes;
	QHash<qint32, qint32> stagingIndexes;
	void addDeviceIndexPairs(const spa_pod* param);

	bool
	setRouteProps(qint32 routeDevice, const std::function<void*(spa_pod_builder*)>& propsCallback);

	bool mWaitingForDevice = false;
	bool deviceResponded = false;
	SpaHook listener;
};

} // namespace qs::service::pipewire
