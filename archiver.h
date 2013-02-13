/**************************************************************************
 *  Renewal plug-in for FAR 3.0 (http://code.google.com/p/farplugs)       *
 *  Copyright (C) 2012 by Artem Senichev <artemsen@gmail.com>             *
 *                                                                        *
 *  This program is free software: you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 3 of the License, or     *
 *  (at your option) any later version.                                   *
 *                                                                        *
 *  This program is distributed in the hope that it will be useful,       *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

/// Ќемного переписанный мной исходник :)

#pragma once

#include "headers.hpp"

/**
 * Extract files from archive
 * \param data archive data
 * \param dst_path destination path
 */
bool extract(HMODULE seven_dll,const wchar_t* src_path, const wchar_t* dst_path);
