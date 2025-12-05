/*
 * WaylandPortalVncServer.h - declaration of WaylandPortalVncServer class
 *
 * Copyright (c) 2024-2025 Veyon Solutions
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include "PluginInterface.h"
#include "VncServerPluginInterface.h"
#include "WaylandPortalVncConfiguration.h"

#include <QDBusObjectPath>

struct WaylandVncScreen;

class WaylandPortalVncServer : public QObject, VncServerPluginInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.WaylandPortalVncServer")
	Q_INTERFACES(PluginInterface VncServerPluginInterface)
public:
	explicit WaylandPortalVncServer( QObject* parent = nullptr );
	~WaylandPortalVncServer() override;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("b8e2c6d4-9a1f-4e8c-b5d7-3f6a9c2e1d0b") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "WaylandPortalVncServer" );
	}

	QString description() const override
	{
		return tr( "Wayland VNC server (Portal/PipeWire)" );
	}

	QString vendor() const override
	{
		return QStringLiteral( "Veyon Community" );
	}

	QString copyright() const override
	{
		return QStringLiteral( "Veyon Solutions" );
	}

	Plugin::Flags flags() const override
	{
		return Plugin::ProvidesDefaultImplementation;
	}

	QStringList supportedSessionTypes() const override
	{
		return { QStringLiteral("wayland") };
	}

	QWidget* configurationWidget() override
	{
		return nullptr;
	}

	void prepareServer() override;

	bool runServer( int serverPort, const Password& password ) override;

	int configuredServerPort() override
	{
		return -1;
	}

	Password configuredPassword() override
	{
		return {};
	}

private:
	static constexpr auto DefaultFramebufferWidth = 1920;
	static constexpr auto DefaultFramebufferHeight = 1080;
	static constexpr auto DefaultSleepTime = 16; // ~60fps

	bool initPortalSession();
	bool initScreen( WaylandVncScreen* screen );
	bool initVncServer( int serverPort, const VncServerPluginInterface::Password& password,
						WaylandVncScreen* screen );
	bool startScreenCast();
	void processScreenCastFrames( WaylandVncScreen* screen );

	static void rfbLogDebug(const char* format, ...);
	static void rfbLogNone(const char* format, ...);

	// Portal session management
	QString m_sessionHandle;
	QDBusObjectPath m_sessionPath;
	bool m_portalInitialized{false};
	int m_pipewireFd{-1};
	uint32_t m_pipewireNode{0};

	WaylandPortalVncConfiguration m_configuration;

};
