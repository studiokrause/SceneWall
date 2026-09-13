#pragma once

#include <QString>

#include <obs-module.h>

inline QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}
