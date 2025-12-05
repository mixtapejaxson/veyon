/*
 * LinuxKeyboardInput.cpp - implementation of LinuxKeyboardInput class
 *
 * Copyright (c) 2019-2025 Tobias Junghans <tobydox@veyon.io>
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

#include <QProcessEnvironment>

#include "LinuxKeyboardInput.h"

#include <X11/Xlib.h>

#include <fakekey/fakekey.h>


LinuxKeyboardInput::LinuxKeyboardInput() :
	m_isWayland( QProcessEnvironment::systemEnvironment().contains( QStringLiteral("WAYLAND_DISPLAY") ) )
{
	if( m_isWayland == false )
	{
		m_display = XOpenDisplay( nullptr );
		if( m_display )
		{
			m_fakeKeyHandle = fakekey_init( m_display );
		}
	}
}



LinuxKeyboardInput::~LinuxKeyboardInput()
{
	if( m_fakeKeyHandle )
	{
		free( m_fakeKeyHandle );
	}
	if( m_display )
	{
		XCloseDisplay( m_display );
	}
}


bool LinuxKeyboardInput::isWaylandSession() const
{
	return m_isWayland;
}


void LinuxKeyboardInput::pressAndReleaseKey( uint32_t keysym )
{
	if( m_isWayland )
	{
		pressAndReleaseKeyWayland( keysym );
	}
	else
	{
		pressAndReleaseKeyX11( keysym );
	}
}



void LinuxKeyboardInput::pressAndReleaseKey( const QByteArray& utf8Data )
{
	if( m_isWayland )
	{
		pressAndReleaseKeyWayland( utf8Data );
	}
	else
	{
		pressAndReleaseKeyX11( utf8Data );
	}
}



void LinuxKeyboardInput::sendString( const QString& string )
{
	for( int i = 0; i < string.size(); ++i )
	{
		pressAndReleaseKey( string.mid( i, 1 ).toUtf8() );
	}
}


void LinuxKeyboardInput::pressAndReleaseKeyX11( uint32_t keysym )
{
	if( m_fakeKeyHandle )
	{
		fakekey_press_keysym( m_fakeKeyHandle, keysym, 0 );
		fakekey_release( m_fakeKeyHandle );
	}
}


void LinuxKeyboardInput::pressAndReleaseKeyX11( const QByteArray& utf8Data )
{
	if( m_fakeKeyHandle )
	{
		fakekey_press( m_fakeKeyHandle, reinterpret_cast<const unsigned char *>( utf8Data.constData() ), utf8Data.size(), 0 );
		fakekey_release( m_fakeKeyHandle );
	}
}


void LinuxKeyboardInput::pressAndReleaseKeyWayland( uint32_t keysym )
{
	// For Wayland sessions, keyboard input must be sent through the RemoteDesktop portal.
	// The portal provides NotifyKeyboardKeysym method for this purpose.
	// This requires an active RemoteDesktop session to be established first.
	// For now, this is a placeholder - full implementation would require
	// maintaining a portal session handle and using D-Bus to send key events.
	Q_UNUSED(keysym)
	vDebug() << "Wayland: keyboard input via portal not fully implemented";
}


void LinuxKeyboardInput::pressAndReleaseKeyWayland( const QByteArray& utf8Data )
{
	// For Wayland sessions, text input should ideally go through the
	// RemoteDesktop portal's keyboard methods or input-method protocol.
	// This is a placeholder for future implementation.
	Q_UNUSED(utf8Data)
	vDebug() << "Wayland: keyboard text input via portal not fully implemented";
}
