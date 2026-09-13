#include "ThumbnailManager.h"

#include <QMetaObject>

#include <cstring>

#include <graphics/vec4.h>

ThumbnailManager &ThumbnailManager::instance()
{
	static ThumbnailManager manager;
	return manager;
}

void ThumbnailManager::ensureRunning()
{
	if (!running) {
		obs_add_tick_callback(tickCallback, this);
		running = true;
	}
}

void ThumbnailManager::attach(QObject *context, IThumbnailReceiver *receiver, obs_source_t *source)
{
	if (!context || !receiver || !source)
		return;

	if (!(obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO))
		return;

	std::lock_guard<std::mutex> lock(mutex);

	for (Entry &e : entries) {
		if (e.receiver == receiver) {
			e.context = context;
			e.source = obs_source_get_ref(source);
			return;
		}
	}

	Entry entry;
	entry.context = context;
	entry.receiver = receiver;
	entry.source = obs_source_get_ref(source);
	// Rozłóż pierwsze odświeżenia w czasie, aby nie renderować wszystkich naraz.
	const int index = static_cast<int>(entries.size());
	entry.acc = -((index % 8) * (intervalMs / 1000.0) / 8.0);
	entries.push_back(entry);

	ensureRunning();
}

void ThumbnailManager::detach(IThumbnailReceiver *receiver)
{
	if (!receiver)
		return;

	obs_enter_graphics();
	{
		std::lock_guard<std::mutex> lock(mutex);
		for (auto it = entries.begin(); it != entries.end(); ++it) {
			if (it->receiver == receiver) {
				releaseEntryGraphics(*it);
				if (it->source)
					obs_source_release(it->source);
				entries.erase(it);
				break;
			}
		}
	}
	obs_leave_graphics();
}

void ThumbnailManager::setSize(IThumbnailReceiver *receiver, int width, int height)
{
	if (!receiver || width <= 0 || height <= 0)
		return;

	std::lock_guard<std::mutex> lock(mutex);
	for (Entry &e : entries) {
		if (e.receiver == receiver) {
			e.width = width;
			e.height = height;
			return;
		}
	}
}

void ThumbnailManager::setPaused(IThumbnailReceiver *receiver, bool paused)
{
	if (!receiver)
		return;

	std::lock_guard<std::mutex> lock(mutex);
	for (Entry &e : entries) {
		if (e.receiver == receiver) {
			e.paused = paused;
			return;
		}
	}
}

void ThumbnailManager::setIntervalMs(int ms)
{
	if (ms <= 0)
		return;

	std::lock_guard<std::mutex> lock(mutex);
	intervalMs = ms;
}

void ThumbnailManager::tickCallback(void *param, float seconds)
{
	static_cast<ThumbnailManager *>(param)->onTick(seconds);
}

void ThumbnailManager::onTick(float seconds)
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (entries.empty())
			return;
	}

	// Ta sama kolejność blokad (grafika, potem mutex) co w detach/shutdown,
	// aby uniknąć zakleszczenia z wątkiem UI.
	obs_enter_graphics();
	{
		std::lock_guard<std::mutex> lock(mutex);
		for (Entry &e : entries) {
			if (e.paused || !e.source)
				continue;
			e.acc += seconds;
			if (e.acc * 1000.0 >= intervalMs) {
				e.acc = 0.0;
				renderEntry(e);
			}
		}
	}
	obs_leave_graphics();
}

void ThumbnailManager::releaseEntryGraphics(Entry &entry)
{
	if (entry.stagesurf) {
		gs_stagesurface_destroy(entry.stagesurf);
		entry.stagesurf = nullptr;
	}
	if (entry.texrender) {
		gs_texrender_destroy(entry.texrender);
		entry.texrender = nullptr;
	}
	entry.surfWidth = 0;
	entry.surfHeight = 0;
}

void ThumbnailManager::renderEntry(Entry &entry)
{
	if (!entry.source)
		return;

	const uint32_t srcWidth = obs_source_get_width(entry.source);
	const uint32_t srcHeight = obs_source_get_height(entry.source);
	if (!srcWidth || !srcHeight)
		return;

	const int outWidth = entry.width;
	const int outHeight = entry.height;

	if (!entry.texrender)
		entry.texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	if (!entry.texrender)
		return;

	if (!entry.stagesurf || entry.surfWidth != outWidth || entry.surfHeight != outHeight) {
		if (entry.stagesurf) {
			gs_stagesurface_destroy(entry.stagesurf);
			entry.stagesurf = nullptr;
		}
		entry.stagesurf = gs_stagesurface_create(outWidth, outHeight, GS_RGBA);
		entry.surfWidth = outWidth;
		entry.surfHeight = outHeight;
	}
	if (!entry.stagesurf)
		return;

	gs_texrender_reset(entry.texrender);

	if (gs_texrender_begin(entry.texrender, outWidth, outHeight)) {
		struct vec4 clearColor;
		vec4_zero(&clearColor);
		gs_clear(GS_CLEAR_COLOR, &clearColor, 0.0f, 0);

		gs_viewport_push();
		gs_projection_push();

		gs_ortho(0.0f, (float)srcWidth, 0.0f, (float)srcHeight, -100.0f, 100.0f);
		gs_set_viewport(0, 0, outWidth, outHeight);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		obs_source_inc_showing(entry.source);
		obs_source_video_render(entry.source);
		obs_source_dec_showing(entry.source);

		gs_blend_state_pop();
		gs_projection_pop();
		gs_viewport_pop();

		gs_texrender_end(entry.texrender);
	}

	gs_stage_texture(entry.stagesurf, gs_texrender_get_texture(entry.texrender));

	uint8_t *data = nullptr;
	uint32_t linesize = 0;
	if (!gs_stagesurface_map(entry.stagesurf, &data, &linesize) || !data)
		return;

	QImage image(outWidth, outHeight, QImage::Format_RGBX8888);
	const int bytesPerLine = outWidth * 4;
	for (int y = 0; y < outHeight; ++y)
		std::memcpy(image.scanLine(y), data + (size_t)y * linesize, bytesPerLine);
	gs_stagesurface_unmap(entry.stagesurf);

	QObject *context = entry.context;
	IThumbnailReceiver *receiver = entry.receiver;
	QMetaObject::invokeMethod(
		context, [receiver, image]() { receiver->setThumbnailImage(image); }, Qt::QueuedConnection);
}

void ThumbnailManager::shutdown()
{
	if (running) {
		obs_remove_tick_callback(tickCallback, this);
		running = false;
	}

	obs_enter_graphics();
	std::vector<Entry> toRelease;
	{
		std::lock_guard<std::mutex> lock(mutex);
		toRelease.swap(entries);
	}
	for (Entry &e : toRelease)
		releaseEntryGraphics(e);
	obs_leave_graphics();

	for (Entry &e : toRelease) {
		if (e.source)
			obs_source_release(e.source);
	}
}
