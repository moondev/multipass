/*
 * Copyright (C) 2019 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cstring>
#include <iostream>

int main(int argc, char* argv[])
{
    // looks for version just to ensure existance
    if (argc == 2 && strcmp(argv[1], "-V") == 0)
    {
        std::cout << "AppArmor parser version 1.11" << std::endl;
        return 0;
    }

    std::cout << "args: ";
    for (int i = 1; i < argc; i++)
    {
        std::cout << argv[i] << ", ";
    }
    std::cout << std::endl;
    std::string s;
    std::getline(std::cin, s, '\0');
    std::cout << s;
    return 0;
}
