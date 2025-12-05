/*
 * WaylandPortalVncServer.cpp - implementation of WaylandPortalVncServer class
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

extern "C" {
#include "rfb/rfb.h"
}

#include <array>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QImage>
#include <QThread>
#include <QUuid>

#include "WaylandPortalVncServer.h"
#include "VeyonConfiguration.h"

// Portal interface names
static constexpr auto PortalDesktopService = "org.freedesktop.portal.Desktop";
static constexpr auto PortalRequestInterface = "org.freedesktop.portal.Request";
static constexpr auto PortalScreenCastInterface = "org.freedesktop.portal.ScreenCast";
static constexpr auto PortalRemoteDesktopInterface = "org.freedesktop.portal.RemoteDesktop";
static constexpr auto PortalObjectPath = "/org/freedesktop/portal/desktop";

struct WaylandVncScreen
{
	~WaylandVncScreen()
	{
		delete[] passwords[0];
	}

	rfbScreenInfoPtr rfbScreen{nullptr};
	std::array<char *, 2> passwords{};
	QImage framebuffer;
	int width{0};
	int height{0};
};


WaylandPortalVncServer::WaylandPortalVncServer( QObject* parent ) :
	QObject( parent ),
	m_configuration( &VeyonCore::config() )
{
}


WaylandPortalVncServer::~WaylandPortalVncServer()
{
	if( m_pipewireFd >= 0 )
	{
		close( m_pipewireFd );
	}
}


void WaylandPortalVncServer::prepareServer()
{
}


bool WaylandPortalVncServer::runServer( int serverPort, const Password& password )
{
	if (VeyonCore::isDebugging())
	{
		rfbLog = rfbLogDebug;
		rfbErr = rfbLogDebug;
	}
	else
	{
		rfbLog = rfbLogNone;
		rfbErr = rfbLogNone;
	}

	vInfo() << "Starting Wayland Portal VNC server on port" << serverPort;

	// Initialize portal session for screen capture
	if( initPortalSession() == false )
	{
		vWarning() << "Failed to initialize portal session - Wayland screen capture may not be available";
		// Fall back to a basic headless mode
	}

	WaylandVncScreen screen;

	if( initScreen( &screen ) == false ||
		initVncServer( serverPort, password, &screen ) == false )
	{
		return false;
	}

	// Start screen cast if portal is available
	if( m_portalInitialized )
	{
		startScreenCast();
	}

	// Main VNC server loop
	while( true )
	{
		QThread::msleep( DefaultSleepTime );

		// Process any pending portal frames
		if( m_portalInitialized )
		{
			processScreenCastFrames( &screen );
		}

		rfbProcessEvents( screen.rfbScreen, 0 );
	}

	rfbShutdownServer( screen.rfbScreen, true );
	rfbScreenCleanup( screen.rfbScreen );

	return true;
}


bool WaylandPortalVncServer::initPortalSession()
{
	vDebug() << "Initializing xdg-desktop-portal session for Wayland screen capture";

	QDBusInterface screenCast( PortalDesktopService,
							   PortalObjectPath,
							   PortalScreenCastInterface,
							   QDBusConnection::sessionBus() );

	if( screenCast.isValid() == false )
	{
		vWarning() << "xdg-desktop-portal ScreenCast interface not available";
		return false;
	}

	// Generate a unique session handle
	const auto sessionToken = QStringLiteral("veyon_") + QUuid::createUuid().toString(QUuid::WithoutBraces).replace(QLatin1Char('-'), QLatin1Char('_'));

	// Create session options
	QVariantMap sessionOptions;
	sessionOptions[QStringLiteral("handle_token")] = sessionToken;
	sessionOptions[QStringLiteral("session_handle_token")] = sessionToken;

	// Call CreateSession
	QDBusReply<QDBusObjectPath> reply = screenCast.call( QStringLiteral("CreateSession"), sessionOptions );

	if( reply.isValid() == false )
	{
		vWarning() << "Failed to create portal session:" << reply.error().message();
		return false;
	}

	m_sessionPath = reply.value();
	m_sessionHandle = m_sessionPath.path();
	m_portalInitialized = true;

	vInfo() << "Portal session created:" << m_sessionHandle;

	return true;
}


bool WaylandPortalVncServer::initScreen( WaylandVncScreen* screen )
{
	screen->width = DefaultFramebufferWidth;
	screen->height = DefaultFramebufferHeight;
	screen->framebuffer = QImage( screen->width, screen->height, QImage::Format_RGB32 );
	screen->framebuffer.fill( m_configuration.backgroundColor() );

	return true;
}


bool WaylandPortalVncServer::initVncServer( int serverPort, const VncServerPluginInterface::Password& password,
										   WaylandVncScreen* screen )
{
	auto rfbScreen = rfbGetScreen( nullptr, nullptr,
								   screen->framebuffer.width(), screen->framebuffer.height(),
								   8, 3, 4 );

	if( rfbScreen == nullptr )
	{
		vCritical() << "Failed to create RFB screen";
		return false;
	}

	screen->passwords[0] = qstrdup( password.toByteArray().constData() );

	rfbScreen->desktopName = "VeyonVNC-Wayland";
	rfbScreen->frameBuffer = reinterpret_cast<char *>( screen->framebuffer.bits() );
	rfbScreen->port = serverPort;
	rfbScreen->ipv6port = serverPort;

	rfbScreen->authPasswdData = screen->passwords.data();
	rfbScreen->passwordCheck = rfbCheckPasswordByList;

	rfbScreen->serverFormat.redShift = 16;
	rfbScreen->serverFormat.greenShift = 8;
	rfbScreen->serverFormat.blueShift = 0;

	rfbScreen->serverFormat.redMax = 255;
	rfbScreen->serverFormat.greenMax = 255;
	rfbScreen->serverFormat.blueMax = 255;

	rfbScreen->serverFormat.trueColour = true;
	rfbScreen->serverFormat.bitsPerPixel = 32;

	rfbScreen->alwaysShared = true;
	rfbScreen->handleEventsEagerly = true;
	rfbScreen->deferUpdateTime = 5;

	rfbScreen->screenData = screen;

	rfbScreen->cursor = nullptr;

	rfbInitServer( rfbScreen );

	rfbMarkRectAsModified( rfbScreen, 0, 0, rfbScreen->width, rfbScreen->height );

	screen->rfbScreen = rfbScreen;

	vInfo() << "VNC server initialized on port" << serverPort;

	return true;
}


bool WaylandPortalVncServer::startScreenCast()
{
	if( m_portalInitialized == false )
	{
		return false;
	}

	vDebug() << "Starting screen cast via portal";

	QDBusInterface screenCast( PortalDesktopService,
							   PortalObjectPath,
							   PortalScreenCastInterface,
							   QDBusConnection::sessionBus() );

	if( screenCast.isValid() == false )
	{
		return false;
	}

	// Select sources (monitor)
	QVariantMap selectSourcesOptions;
	selectSourcesOptions[QStringLiteral("handle_token")] = QStringLiteral("veyon_sources");
	selectSourcesOptions[QStringLiteral("types")] = QVariant::fromValue<uint32_t>(1); // Monitor
	selectSourcesOptions[QStringLiteral("multiple")] = false;

	QDBusReply<QDBusObjectPath> selectReply = screenCast.call(
		QStringLiteral("SelectSources"),
		QVariant::fromValue(m_sessionPath),
		selectSourcesOptions );

	if( selectReply.isValid() == false )
	{
		vWarning() << "Failed to select sources:" << selectReply.error().message();
		return false;
	}

	// Start the screen cast
	QVariantMap startOptions;
	startOptions[QStringLiteral("handle_token")] = QStringLiteral("veyon_start");

	QDBusReply<QDBusObjectPath> startReply = screenCast.call(
		QStringLiteral("Start"),
		QVariant::fromValue(m_sessionPath),
		QString(),  // Parent window handle (empty for no parent)
		startOptions );

	if( startReply.isValid() == false )
	{
		vWarning() << "Failed to start screen cast:" << startReply.error().message();
		return false;
	}

	vInfo() << "Screen cast started successfully";
	return true;
}


void WaylandPortalVncServer::processScreenCastFrames( WaylandVncScreen* screen )
{
	// This is a placeholder for actual PipeWire frame processing
	// In a full implementation, this would:
	// 1. Connect to PipeWire using the file descriptor from the portal
	// 2. Receive video frames from the screen cast stream
	// 3. Convert frames to the VNC framebuffer format
	// 4. Update the rfbScreen and mark regions as modified

	// For now, we just keep the basic screen available
	// Full PipeWire integration would require libpipewire
	Q_UNUSED(screen)
}


void WaylandPortalVncServer::rfbLogDebug(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	static constexpr int MaxMessageLength = 256;
	char message[MaxMessageLength];

	vsnprintf(message, sizeof(message), format, args);
	message[MaxMessageLength-1] = 0;

	va_end(args);

	vDebug() << message;
}


void WaylandPortalVncServer::rfbLogNone(const char* format, ...)
{
	Q_UNUSED(format);
}


IMPLEMENT_CONFIG_PROXY(WaylandPortalVncConfiguration)
