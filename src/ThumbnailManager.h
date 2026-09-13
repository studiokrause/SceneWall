#pragma once

#include <QImage>
#include <QObject>

#include <mutex>
#include <vector>

#include <obs.h>
#include <graphics/graphics.h>

class IThumbnailReceiver {
public:
	virtual ~IThumbnailReceiver() = default;
	virtual void setThumbnailImage(const QImage &image) = 0;
};

class ThumbnailManager {
public:
	static ThumbnailManager &instance();

	void attach(QObject *context, IThumbnailReceiver *receiver, obs_source_t *source);
	void detach(IThumbnailReceiver *receiver);
	void setSize(IThumbnailReceiver *receiver, int width, int height);
	void setPaused(IThumbnailReceiver *receiver, bool paused);
	void setIntervalMs(int ms);
	void shutdown();

private:
	ThumbnailManager() = default;
	~ThumbnailManager() = default;

	struct Entry {
		QObject *context = nullptr;
		IThumbnailReceiver *receiver = nullptr;
		obs_source_t *source = nullptr;
		int width = 160;
		int height = 90;
		bool paused = false;
		double acc = 0.0;
		gs_texrender_t *texrender = nullptr;
		gs_stagesurf_t *stagesurf = nullptr;
		int surfWidth = 0;
		int surfHeight = 0;
	};

	static void tickCallback(void *param, float seconds);
	void onTick(float seconds);
	void renderEntry(Entry &entry);
	void releaseEntryGraphics(Entry &entry);
	void ensureRunning();

	std::mutex mutex;
	std::vector<Entry> entries;
	int intervalMs = 500;
	bool running = false;
};
