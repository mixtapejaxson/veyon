/*
 * LinuxInputDeviceFunctions.cpp - implementation of LinuxInputDeviceFunctions class
 *
 * Copyright (c) 2017-2025 Tobias Junghans <tobydox@veyon.io>
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

#include "PlatformServiceFunctions.h"
#include "LinuxInputDeviceFunctions.h"
#include "LinuxKeyboardShortcutTrapper.h"

#include <X11/XKBlib.h>


bool LinuxInputDeviceFunctions::isWaylandSession() const
{
	return QProcessEnvironment::systemEnvironment().contains( QStringLiteral("WAYLAND_DISPLAY") );
}


void LinuxInputDeviceFunctions::enableInputDevices()
{
	if( m_inputDevicesDisabled )
	{
		if( isWaylandSession() )
		{
			enableInputDevicesWayland();
		}
		else
		{
			restoreKeyMapTable();
		}

		m_inputDevicesDisabled = false;
	}
}



void LinuxInputDeviceFunctions::disableInputDevices()
{
	if( m_inputDevicesDisabled == false )
	{
		if( isWaylandSession() )
		{
			disableInputDevicesWayland();
		}
		else
		{
			setEmptyKeyMapTable();
		}

		m_inputDevicesDisabled = true;
	}
}



KeyboardShortcutTrapper* LinuxInputDeviceFunctions::createKeyboardShortcutTrapper( QObject* parent )
{
	return new LinuxKeyboardShortcutTrapper( parent );
}



void LinuxInputDeviceFunctions::setEmptyKeyMapTable()
{
	if( m_origKeyTable )
	{
		XFree( m_origKeyTable );
	}

	auto display = XOpenDisplay( nullptr );
	if( display == nullptr )
	{
		vDebug() << "Could not open X11 display for keyboard mapping manipulation";
		return;
	}

	XDisplayKeycodes( display, &m_keyCodeMin, &m_keyCodeMax );
	m_keyCodeCount = m_keyCodeMax - m_keyCodeMin;

	m_origKeyTable = XGetKeyboardMapping( display, ::KeyCode( m_keyCodeMin ), m_keyCodeCount, &m_keySymsPerKeyCode );

	auto newKeyTable = XGetKeyboardMapping( display, ::KeyCode( m_keyCodeMin ), m_keyCodeCount, &m_keySymsPerKeyCode );

	for( int i = 0; i < m_keyCodeCount * m_keySymsPerKeyCode; i++ )
	{
		newKeyTable[i] = 0;
	}

	XChangeKeyboardMapping( display, m_keyCodeMin, m_keySymsPerKeyCode, newKeyTable, m_keyCodeCount );
	XFlush( display );
	XFree( newKeyTable );
	XCloseDisplay( display );
}



void LinuxInputDeviceFunctions::restoreKeyMapTable()
{
	Display* display = XOpenDisplay( nullptr );
	if( display == nullptr )
	{
		vDebug() << "Could not open X11 display for keyboard mapping restoration";
		return;
	}

	if( m_origKeyTable == nullptr )
	{
		XCloseDisplay( display );
		return;
	}

	XChangeKeyboardMapping( display, m_keyCodeMin, m_keySymsPerKeyCode,
							static_cast<::KeySym *>( m_origKeyTable ), m_keyCodeCount );

	XFlush( display );
	XCloseDisplay( display );

	XFree( m_origKeyTable );
	m_origKeyTable = nullptr;
}


void LinuxInputDeviceFunctions::enableInputDevicesWayland()
{
	// For Wayland sessions, input device control is handled through the
	// RemoteDesktop portal. The portal automatically manages input permissions.
	// This is a no-op as the portal session handles input state.
	vDebug() << "Wayland: enabling input devices via portal (no-op)";
}


void LinuxInputDeviceFunctions::disableInputDevicesWayland()
{
	// For Wayland sessions, we cannot directly disable input devices.
	// Input control must be managed through the RemoteDesktop portal.
	// Screen locking functionality will need to use alternative methods.
	vDebug() << "Wayland: disabling input devices via portal (limited functionality)";
}
